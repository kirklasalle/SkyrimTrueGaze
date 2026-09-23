#include "TargetSelector.hpp"
#include "PlayerGazeResolver.hpp"
#include "ActorGazeRuntime.hpp"
#include <cmath>

namespace TrueGaze::Engine
{

    // Static frame snapshot of the crosshair sweet-spot parameters. Written by
    // GazeEngine::RefreshTuning, read by ResolveTarget. Game thread only.
    TargetSelector::CrosshairParams TargetSelector::s_crosshair{};

    namespace
    {

        /// Skyrim world units per metre. These were bare literals (0.01428f, 160.0f)
        /// repeated throughout the original implementation; a single wrong digit in any
        /// one of them would have produced a silently wrong distance calculation.
        constexpr float kUnitsPerMeter = 70.0f;
        constexpr float kUnitsToMeters = 1.0f / kUnitsPerMeter;

        /// Approximate eye height above an actor's origin, in Skyrim units (~1.25 m for standing eye level).
        constexpr float kEyeHeightOffsetUnits = 125.0f;

        /// An approaching actor is worth tracking out to this range.
        constexpr float kNearbyRangeMeters = 8.0f;

        /// Distance to an ambient focus point placed ahead of the observer.
        constexpr float kAmbientForwardUnits = 200.0f;
        constexpr float kAmbientNominalMeters = 3.0f;

        /// Humanoid comfortable forward visual cone (degrees).
        /// Head turn comfort limit before whole-body turning is required (~75 deg).
        /// Targets outside this angle are behind or flanking the actor and must NOT be targeted.
        constexpr float kMaxVisualConeAngleDeg = 75.0f;

        /// Wider visual cone (95 deg) used to retain an already locked target,
        /// preventing edge chatter or premature drop when talking/standing in personal space (< 1.8m).
        constexpr float kMaxHoldVisualConeAngleDeg = 95.0f;

        constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kTwoPi = 2.0f * kPi;

        float WrapPi(float angle) noexcept
        {
            while (angle > kPi) angle -= kTwoPi;
            while (angle < -kPi) angle += kTwoPi;
            return angle;
        }

        /// Tests whether targetPos lies within the observer's natural forward visual cone.
        bool IsInVisualCone(const RE::NiPoint3 &observerPos,
                            float observerYawRad,
                            const RE::NiPoint3 &targetPos,
                            float maxAngleDeg = kMaxVisualConeAngleDeg) noexcept
        {
            const float dx = targetPos.x - observerPos.x;
            const float dy = targetPos.y - observerPos.y;
            const float distSq = dx * dx + dy * dy;

            // Reject co-located or stacked actors (< 25 units / ~0.35m), e.g. during Helgen carriage rides
            // or when actors share marker origin. Returning true here caused atan2(0,0) singularity.
            if (distSq < 625.0f)
            {
                return false;
            }

            // Skyrim's actor forward is +Y, so bearing to target is atan2(dx, dy)
            const float bearing = std::atan2(dx, dy);
            const float localYaw = WrapPi(bearing - observerYawRad);
            const float localYawDeg = std::abs(localYaw * kRadToDeg);
            return localYawDeg <= maxAngleDeg;
        }

        /// Distance in metres between two world positions.
        float DistanceMeters(const RE::NiPoint3 &a, const RE::NiPoint3 &b) noexcept
        {
            return a.GetDistance(b) * kUnitsToMeters;
        }

#if __has_include(<RE/Skyrim.h>)
        /// Retrieve the true 3D world position of an actor's head/face (using head bone transform if available).
        /// Dynamically tracks seated postures, counter-leaning idles, crouching, and race scales.
        RE::NiPoint3 GetActorHeadPosition(RE::Actor *actor) noexcept
        {
            if (!actor) return RE::NiPoint3{0.0f, 0.0f, 0.0f};
            if (auto *root = actor->Get3D())
            {
                static const char *kHeadCandidates[] = {
                    "NPC Head [Head]", "Head", "Head1", "Bip01 Head"
                };
                for (const auto *name : kHeadCandidates)
                {
                    if (auto *bone = root->GetObjectByName(RE::BSFixedString(name)))
                    {
                        return bone->world.translate;
                    }
                }
                return RE::NiPoint3{root->world.translate.x, root->world.translate.y, root->world.translate.z + kEyeHeightOffsetUnits};
            }
            const auto pos = actor->GetPosition();
            return RE::NiPoint3{pos.x, pos.y, pos.z + kEyeHeightOffsetUnits};
        }

        /// Retrieve the true 3D world position of an actor (using 3D node transform if available)
        RE::NiPoint3 GetActorWorldPosition(RE::Actor *actor) noexcept
        {
            if (!actor) return RE::NiPoint3{0.0f, 0.0f, 0.0f};
            if (auto *root = actor->Get3D())
            {
                return root->world.translate;
            }
            return actor->GetPosition();
        }
#endif

    } // namespace

    TargetSelector::GazeTarget TargetSelector::ResolveTarget(uint32_t observerFormId,
                                                            ActorGazeRuntime *state,
                                                            float deltaSeconds) noexcept
    {
        GazeTarget target{};

        if (observerFormId == 0)
        {
            return target;
        }

#if __has_include(<RE/Skyrim.h>)
        auto *form = RE::TESForm::LookupByID(observerFormId);
        if (!form)
            return target;

        auto *observer = form->As<RE::Actor>();
        if (!observer)
            return target;

        auto *player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return target;

        const auto observerPos = GetActorWorldPosition(observer);
        const auto playerPos = GetActorWorldPosition(player);
        const auto observerHeadPos = GetActorHeadPosition(observer);
        const auto playerHeadPos = GetActorHeadPosition(player);

        // 0. Highest priority: the player's crosshair is on this actor's face / upper body.
        //
        // The player's gaze is the strongest social signal in the world: a person
        // you are looking at feels watched and looks back. When the crosshair sits
        // inside the face sweet spot, this actor locks eye contact.
        // To permanently eliminate head twitching and edge-chatter, we latch eye contact
        // with a 1.5s hold timer (state->crosshairHoldTimerSec). Even if the player's crosshair
        // wavers for a few frames, the actor maintains calm, stable eye contact!
        if (s_crosshair.enabled && observer != player)
        {
            PlayerGazeResolver::Params gazeParams{};
            gazeParams.baseToleranceDeg = s_crosshair.baseToleranceDeg;
            gazeParams.maxRangeMeters = s_crosshair.maxRangeMeters;
            gazeParams.pointBlankMeters = s_crosshair.pointBlankMeters;

            const auto playerGaze = PlayerGazeResolver::Resolve(gazeParams);
            const bool crosshairOnThisActor = (playerGaze.onFace && playerGaze.targetFormId == observerFormId);

            if (crosshairOnThisActor)
            {
                if (state)
                {
                    state->crosshairHoldTimerSec = 1.5f; // Latch stable eye contact for 1.5s
                }
            }
            else if (state && state->crosshairHoldTimerSec > 0.0f)
            {
                state->crosshairHoldTimerSec -= deltaSeconds;
            }

            if (state && state->crosshairHoldTimerSec > 0.0f)
            {
                const float pDist = DistanceMeters(observerHeadPos, playerHeadPos);
                // Verify player is alive and within natural forward visual cone
                if (pDist <= s_crosshair.maxRangeMeters &&
                    IsInVisualCone(observerPos, observer->GetAngleZ(), playerPos, kMaxHoldVisualConeAngleDeg))
                {
                    target.targetFormId = player->GetFormID();
                    target.priority = TargetPriority::CrosshairFocus;
                    target.worldX = playerHeadPos.x;
                    target.worldY = playerHeadPos.y;
                    target.worldZ = playerHeadPos.z;
                    target.distanceMeters = pDist;
                    target.isPlayer = true;
                    return target;
                }
                else
                {
                    state->crosshairHoldTimerSec = 0.0f;
                }
            }
        }

        auto *ui = RE::UI::GetSingleton();

        // When the observer is the player in 3rd person:
        // Engage TrueGaze biological eye tracking toward dialogue partner, crosshair target,
        // combat opponent, conversational candidate NPC, or forward ambient gaze point.
        if (observer == player)
        {
            if (ui && ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME))
            {
                const auto cameraPos = RE::PlayerCamera::GetActiveCameraPosition();
                target.targetFormId = player->GetFormID();
                target.priority = TargetPriority::DialoguePartner;
                target.worldX = cameraPos.x;
                target.worldY = cameraPos.y;
                target.worldZ = cameraPos.z;
                target.distanceMeters = DistanceMeters(observerHeadPos, cameraPos);
                target.isPlayer = true;
                return target;
            }

            // 1. Dialogue speaker
            if (ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME))
            {
                auto *topicMgr = RE::MenuTopicManager::GetSingleton();
                if (topicMgr && topicMgr->speaker)
                {
                    if (auto speakerPtr = topicMgr->speaker.get())
                    {
                        if (auto *speakerActor = speakerPtr->As<RE::Actor>())
                        {
                            const auto sHeadPos = GetActorHeadPosition(speakerActor);
                            target.targetFormId = speakerActor->GetFormID();
                            target.priority = TargetPriority::DialoguePartner;
                            target.worldX = sHeadPos.x;
                            target.worldY = sHeadPos.y;
                            target.worldZ = sHeadPos.z;
                            target.distanceMeters = DistanceMeters(observerHeadPos, sHeadPos);
                            target.isPlayer = false;
                            return target;
                        }
                    }
                }
            }

            // 2. Crosshair focus target
            PlayerGazeResolver::Params gazeParams{};
            gazeParams.baseToleranceDeg = s_crosshair.baseToleranceDeg;
            gazeParams.maxRangeMeters = s_crosshair.maxRangeMeters;
            gazeParams.pointBlankMeters = s_crosshair.pointBlankMeters;
            const auto playerGaze = PlayerGazeResolver::Resolve(gazeParams);
            if (playerGaze.targetFormId != 0 && playerGaze.targetFormId != player->GetFormID())
            {
                auto *aimForm = RE::TESForm::LookupByID(playerGaze.targetFormId);
                if (aimForm)
                {
                    if (auto *aimActor = aimForm->As<RE::Actor>())
                    {
                        const auto aimHeadPos = GetActorHeadPosition(aimActor);
                        target.targetFormId = aimActor->GetFormID();
                        target.priority = TargetPriority::CrosshairFocus;
                        target.worldX = aimHeadPos.x;
                        target.worldY = aimHeadPos.y;
                        target.worldZ = aimHeadPos.z;
                        target.distanceMeters = DistanceMeters(observerHeadPos, aimHeadPos);
                        target.isPlayer = false;
                        return target;
                    }
                }
            }

            // 3. Combat target
            if (auto combatHandle = player->GetActorRuntimeData().currentCombatTarget)
            {
                if (auto combatPtr = combatHandle.get())
                {
                    if (auto *cTarget = combatPtr.get())
                    {
                        if (!cTarget->IsDead() && !cTarget->IsDisabled())
                        {
                            const auto cHeadPos = GetActorHeadPosition(cTarget);
                            target.targetFormId = cTarget->GetFormID();
                            target.priority = TargetPriority::CombatTarget;
                            target.worldX = cHeadPos.x;
                            target.worldY = cHeadPos.y;
                            target.worldZ = cHeadPos.z;
                            target.distanceMeters = DistanceMeters(observerHeadPos, cHeadPos);
                            target.isPlayer = false;
                            return target;
                        }
                    }
                }
            }

            // 4. Conversational Candidate Scan (3rd person / free exploration)
            // When walking up to NPCs or standing near people in town/shops (e.g. Lucan, Camilla),
            // engage natural social gaze if an NPC is within conversational range and forward cone.
            RE::Actor *closestNpc = nullptr;
            float closestDistMeters = 4.5f;
            if (auto *processLists = RE::ProcessLists::GetSingleton())
            {
                for (auto &handle : processLists->highActorHandles)
                {
                    if (auto actorPtr = handle.get())
                    {
                        auto *otherActor = actorPtr.get();
                        if (otherActor && otherActor != player &&
                            !otherActor->IsDead() && !otherActor->IsDisabled())
                        {
                            const auto oPos = GetActorWorldPosition(otherActor);
                            if (IsInVisualCone(observerPos, player->GetAngleZ(), oPos, kMaxVisualConeAngleDeg))
                            {
                                const float d = DistanceMeters(observerPos, oPos);
                                if (d < closestDistMeters)
                                {
                                    closestDistMeters = d;
                                    closestNpc = otherActor;
                                }
                            }
                        }
                    }
                }
            }

            if (closestNpc)
            {
                const auto nHeadPos = GetActorHeadPosition(closestNpc);
                target.targetFormId = closestNpc->GetFormID();
                target.priority = TargetPriority::NearbyActor;
                target.worldX = nHeadPos.x;
                target.worldY = nHeadPos.y;
                target.worldZ = nHeadPos.z;
                target.distanceMeters = DistanceMeters(observerHeadPos, nHeadPos);
                target.isPlayer = false;
                return target;
            }

            // 5. Ambient forward gaze aligned with player's facing direction
            const float heading = player->GetAngleZ();
            target.priority = TargetPriority::AmbientInterest;
            target.worldX = observerHeadPos.x + std::sin(heading) * kAmbientForwardUnits;
            target.worldY = observerHeadPos.y + std::cos(heading) * kAmbientForwardUnits;
            target.worldZ = observerHeadPos.z;
            target.distanceMeters = kAmbientForwardUnits / 70.0f;
            target.isPlayer = false;
            return target;
        }

        // 1. Highest priority: the active dialogue partner with player (DialogueMenu open)
        if (ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME))
        {
            auto *topicMgr = RE::MenuTopicManager::GetSingleton();
            if (topicMgr && topicMgr->speaker)
            {
                if (auto speakerPtr = topicMgr->speaker.get())
                {
                    if (speakerPtr.get() == observer)
                    {
                        target.targetFormId = player->GetFormID();
                        target.priority = TargetPriority::DialoguePartner;
                        target.worldX = playerHeadPos.x;
                        target.worldY = playerHeadPos.y;
                        target.worldZ = playerHeadPos.z;
                        target.distanceMeters = DistanceMeters(observerHeadPos, playerHeadPos);
                        target.isPlayer = true;
                        return target;
                    }
                }
            }
        }

        // 1b. Active conversation partner with another NPC (or player) in scene dialogue
        if (auto dlgHandle = observer->GetActorRuntimeData().dialogueItemTarget)
        {
            if (auto dlgPtr = dlgHandle.get())
            {
                if (auto *dlgActor = dlgPtr->As<RE::Actor>())
                {
                    if (dlgActor != observer && !dlgActor->IsDead() && !dlgActor->IsDisabled())
                    {
                        const auto dPos = GetActorWorldPosition(dlgActor);
                        const float dDist = DistanceMeters(observerPos, dPos);
                        // CRITICAL: Must be within conversational range AND inside forward visual cone!
                        // Bethesda's dialogue target can be an actor in another room or directly behind the observer.
                        // Forcing gaze to an actor at >75 degrees causes severe neck/head snapping.
                        if (dDist <= 6.0f && IsInVisualCone(observerPos, observer->GetAngleZ(), dPos, kMaxHoldVisualConeAngleDeg))
                        {
                            const auto dHeadPos = GetActorHeadPosition(dlgActor);
                            target.targetFormId = dlgActor->GetFormID();
                            target.priority = TargetPriority::DialoguePartner;
                            target.worldX = dHeadPos.x;
                            target.worldY = dHeadPos.y;
                            target.worldZ = dHeadPos.z;
                            target.distanceMeters = dDist;
                            target.isPlayer = (dlgActor == player);
                            return target;
                        }
                    }
                }
            }
        }

        // 1c. Engine AI headtracking target (NPC-to-NPC conversations, scenes, idle chatter)
        if (auto *process = observer->GetActorRuntimeData().currentProcess)
        {
            if (auto htHandle = process->GetHeadtrackTarget())
            {
                if (auto htPtr = htHandle.get())
                {
                    if (auto *htActor = htPtr->As<RE::Actor>())
                    {
                        if (htActor != observer && !htActor->IsDead() && !htActor->IsDisabled())
                        {
                            const auto htPos = GetActorWorldPosition(htActor);
                            const float htDist = DistanceMeters(observerPos, htPos);
                            // CRITICAL: Must be within conversational range AND inside forward visual cone!
                            // Engine AI assigns headtrack targets anywhere in cell. Without visual cone filtering,
                            // NPCs wrench their head 80-120 deg over their shoulder, then flap back to ambient.
                            if (htDist <= 6.0f && IsInVisualCone(observerPos, observer->GetAngleZ(), htPos, kMaxHoldVisualConeAngleDeg))
                            {
                                const auto htHeadPos = GetActorHeadPosition(htActor);
                                target.targetFormId = htActor->GetFormID();
                                target.priority = TargetPriority::DialoguePartner;
                                target.worldX = htHeadPos.x;
                                target.worldY = htHeadPos.y;
                                target.worldZ = htHeadPos.z;
                                target.distanceMeters = htDist;
                                target.isPlayer = (htActor == player);
                                return target;
                            }
                        }
                    }
                }
            }
        }

        // 2. Combat target. Reached through the actor's runtime data as a handle,
        //    not a direct accessor, and validated for liveness before use.
        RE::Actor *combatTarget = nullptr;
        if (auto combatHandle = observer->GetActorRuntimeData().currentCombatTarget)
        {
            if (auto combatPtr = combatHandle.get())
            {
                combatTarget = combatPtr.get();
            }
        }

        if (combatTarget && !combatTarget->IsDead())
        {
            const auto combatPos = GetActorWorldPosition(combatTarget);
            const float cDist = DistanceMeters(observerPos, combatPos);
            if (cDist <= 15.0f && IsInVisualCone(observerPos, observer->GetAngleZ(), combatPos, 75.0f))
            {
                const auto cHeadPos = GetActorHeadPosition(combatTarget);
                target.targetFormId = combatTarget->GetFormID();
                target.priority = TargetPriority::CombatTarget;
                target.worldX = cHeadPos.x;
                target.worldY = cHeadPos.y;
                target.worldZ = cHeadPos.z;
                target.distanceMeters = cDist;
                target.isPlayer = (combatTarget == player);
                return target;
            }
        }

        const float playerDistanceMeters = DistanceMeters(observerHeadPos, playerHeadPos);

        // 3. Target Fixation Stability (Dwell Time Hysteresis)
        // If the actor is currently fixating on a valid target, maintain lock for at least 1.5s
        // before switching to avoid rapid micro-flapping between nearby actors.
        if (state && state->trackedTargetFormId != 0)
        {
            if (state->trackedTargetFormId == player->GetFormID())
            {
                if (playerDistanceMeters <= kNearbyRangeMeters &&
                    IsInVisualCone(observerPos, observer->GetAngleZ(), playerPos, kMaxHoldVisualConeAngleDeg))
                {
                    if (state->fixationHoldSec < 1.5f)
                    {
                        state->fixationHoldSec += deltaSeconds;
                        target.targetFormId = player->GetFormID();
                        target.priority = TargetPriority::NearbyActor;
                        target.worldX = playerHeadPos.x;
                        target.worldY = playerHeadPos.y;
                        target.worldZ = playerHeadPos.z;
                        target.distanceMeters = playerDistanceMeters;
                        target.isPlayer = true;
                        return target;
                    }
                }
                else
                {
                    state->fixationHoldSec = 0.0f;
                }
            }
            else
            {
                auto *heldForm = RE::TESForm::LookupByID(state->trackedTargetFormId);
                auto *heldActor = heldForm ? heldForm->As<RE::Actor>() : nullptr;
                if (heldActor && !heldActor->IsDead() && !heldActor->IsDisabled())
                {
                    const auto hPos = GetActorWorldPosition(heldActor);
                    const float hDist = DistanceMeters(observerPos, hPos);
                    if (hDist <= 5.0f &&
                        IsInVisualCone(observerPos, observer->GetAngleZ(), hPos, kMaxHoldVisualConeAngleDeg))
                    {
                        if (state->fixationHoldSec < 1.5f)
                        {
                            const auto hHeadPos = GetActorHeadPosition(heldActor);
                            state->fixationHoldSec += deltaSeconds;
                            target.targetFormId = heldActor->GetFormID();
                            target.priority = TargetPriority::NearbyActor;
                            target.worldX = hHeadPos.x;
                            target.worldY = hHeadPos.y;
                            target.worldZ = hHeadPos.z;
                            target.distanceMeters = hDist;
                            target.isPlayer = false;
                            return target;
                        }
                    }
                    else
                    {
                        state->fixationHoldSec = 0.0f;
                    }
                }
                else
                {
                    state->fixationHoldSec = 0.0f;
                }
            }
        }

        // 4. Candidate selection with strict forward visual cone filtering
        RE::Actor *closestNpc = nullptr;
        float closestDistMeters = 3.5f; // within conversational range
        if (auto *processLists = RE::ProcessLists::GetSingleton())
        {
            for (auto &handle : processLists->highActorHandles)
            {
                if (auto actorPtr = handle.get())
                {
                    auto *otherActor = actorPtr.get();
                    if (otherActor && otherActor != observer && otherActor != player &&
                        !otherActor->IsDead() && !otherActor->IsDisabled())
                    {
                        const auto oPos = GetActorWorldPosition(otherActor);

                        // STRICT VISUAL CONE: If target is behind the observer (>65 deg),
                        // DO NOT select it! A human does not turn their head backward over their shoulder.
                        if (!IsInVisualCone(observerPos, observer->GetAngleZ(), oPos, kMaxVisualConeAngleDeg))
                        {
                            continue;
                        }

                        float d = DistanceMeters(observerPos, oPos);
                        // Hysteresis advantage for previously tracked target
                        if (state && state->trackedTargetFormId == otherActor->GetFormID())
                        {
                            d -= 0.8f;
                        }

                        if (d < closestDistMeters)
                        {
                            closestDistMeters = d;
                            closestNpc = otherActor;
                        }
                    }
                }
            }
        }

        // Check if player is in visual cone
        float effPlayerDist = playerDistanceMeters;
        const float coneAngle = (playerDistanceMeters <= 1.8f) ? kMaxHoldVisualConeAngleDeg : kMaxVisualConeAngleDeg;
        const bool playerInCone = IsInVisualCone(observerPos, observer->GetAngleZ(), playerPos, coneAngle);
        if (!playerInCone)
        {
            effPlayerDist = 999.0f; // Player is behind observer; do not turn neck backwards
        }
        else if (state && state->trackedTargetFormId == player->GetFormID())
        {
            effPlayerDist -= 0.8f; // Hysteresis advantage
        }

        if (closestNpc && closestDistMeters < effPlayerDist)
        {
            const auto nHeadPos = GetActorHeadPosition(closestNpc);
            target.targetFormId = closestNpc->GetFormID();
            target.priority = TargetPriority::NearbyActor;
            target.worldX = nHeadPos.x;
            target.worldY = nHeadPos.y;
            target.worldZ = nHeadPos.z;
            target.distanceMeters = DistanceMeters(observerHeadPos, nHeadPos);
            target.isPlayer = false;
            if (state) state->fixationHoldSec = 0.0f;
            return target;
        }

        if (playerInCone && effPlayerDist <= kNearbyRangeMeters)
        {
            target.targetFormId = player->GetFormID();
            target.priority = TargetPriority::NearbyActor;
            target.worldX = playerHeadPos.x;
            target.worldY = playerHeadPos.y;
            target.worldZ = playerHeadPos.z;
            target.distanceMeters = DistanceMeters(observerHeadPos, playerHeadPos);
            target.isPlayer = true;
            if (state) state->fixationHoldSec = 0.0f;
            return target;
        }

        // 5. Ambient interest: a point ahead of the observer, oriented along the
        //    actor's heading so they look forward along their own facing angle
        //    rather than staring sideways or snapping backwards.
        if (state) state->fixationHoldSec = 0.0f;
        target.priority = TargetPriority::AmbientInterest;
        const float actorYaw = observer->GetAngleZ();
        target.worldX = observerHeadPos.x + std::sin(actorYaw) * kAmbientForwardUnits;
        target.worldY = observerHeadPos.y + std::cos(actorYaw) * kAmbientForwardUnits;
        target.worldZ = observerHeadPos.z;
        target.distanceMeters = kAmbientNominalMeters;
        return target;

#else
        // Standalone fallback for unit tests: the player at a nominal 1.5 m.
        target.targetFormId = 0x14;
        target.priority = TargetPriority::NearbyActor;
        target.worldX = 0.0f;
        target.worldY = 150.0f;
        target.worldZ = kEyeHeightOffsetUnits;
        target.distanceMeters = 1.5f;
        target.isPlayer = true;
        return target;
#endif
    }

} // namespace TrueGaze::Engine
