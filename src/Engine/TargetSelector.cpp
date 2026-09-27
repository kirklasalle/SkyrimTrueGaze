#include "TargetSelector.hpp"
#include "ActorGazeRuntime.hpp"
#include "GazeAnchors.hpp"
#include "GazeEngine.hpp"
#include "PlayerGazeResolver.hpp"
#include <cmath>
#include <cstdint>

namespace TrueGaze::Engine
{

    // Static frame snapshot of the crosshair sweet-spot parameters. Written by
    // GazeEngine::RefreshTuning, read by ResolveTarget. Game thread only.
    TargetSelector::CrosshairParams TargetSelector::s_crosshair{};

    // Static frame snapshot of the eye-anchor offsets (head bone -> eyeline). Written
    // by GazeEngine::RefreshTuning, read by GetActorHeadPosition. Game thread only.
    TargetSelector::EyeAnchorParams TargetSelector::s_eyeAnchor{};

    namespace
    {

        /// Skyrim world units per metre. These were bare literals (0.01428f, 160.0f)
        /// repeated throughout the original implementation; a single wrong digit in any
        /// one of them would have produced a silently wrong distance calculation.
        constexpr float kUnitsPerMeter = 70.0f;
        constexpr float kUnitsToMeters = 1.0f / kUnitsPerMeter;

        /// Approximate eye height above an actor's origin, in Skyrim units.
        /// R14 E7.5 — canonical value now lives in GazeAnchors.hpp.
        constexpr float kEyeHeightOffsetUnits = TrueGaze::Anchors::kEyeHeightUnits;

        /// An approaching actor is worth tracking out to this range.
        /// Widened from 8m: a person 10m away is still clearly in the same room
        /// and warrants social attention from nearby NPCs.
        constexpr float kNearbyRangeMeters = 12.0f;

        /// Distance to an ambient focus point placed ahead of the observer.
        constexpr float kAmbientForwardUnits = 200.0f;
        constexpr float kAmbientNominalMeters = 3.0f;

        /// Humanoid comfortable forward visual cone (degrees).
        /// Head turn comfort limit before whole-body turning is required (~75 deg).
        /// Targets outside this angle are behind or flanking the actor and must NOT be targeted.
        constexpr float kMaxVisualConeAngleDeg = 75.0f;

        /// Wider visual cone used to retain an already locked target and to detect
        /// the player even when the NPC is not squarely facing them. 110° covers
        /// nearly the full peripheral social attention range without allowing
        /// backwards neck-snap (that needs a full body turn, not a head turn).
        constexpr float kMaxHoldVisualConeAngleDeg = 110.0f;

        constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kTwoPi = 2.0f * kPi;

        float WrapPi(float angle) noexcept
        {
            while (angle > kPi)
                angle -= kTwoPi;
            while (angle < -kPi)
                angle += kTwoPi;
            return angle;
        }

        /// Tests whether targetPos lies within the observer's natural forward visual cone.
        bool IsInVisualCone(const RE::NiPoint3& observerPos, float observerYawRad,
                            const RE::NiPoint3& targetPos,
                            float maxAngleDeg = kMaxVisualConeAngleDeg) noexcept
        {
            const float dx = targetPos.x - observerPos.x;
            const float dy = targetPos.y - observerPos.y;
            const float distSq = dx * dx + dy * dy;

            // Only reject a genuine atan2(0,0) singularity: actors sharing an origin
            // within ~6 cm (16 units^2). The previous 25-unit / 0.35 m rejection
            // dropped POINT-BLANK seated actors — exactly the Helgen cart, where the
            // player and NPCs sit within arm's reach — which then fell through to the
            // ambient forward stare. A person sitting across from you is unambiguously
            // in your field of view, so anyone this close is treated as in-cone
            // regardless of bearing.
            if (distSq < 16.0f)
            {
                return true;
            }

            // Point-blank social range (< ~0.9 m / 64 units): a seated conversational
            // partner is always "in view" even if the actor's facing has been rotated
            // by a seated idle. Angle gating here produces false ambient fallbacks.
            if (distSq < 4096.0f)
            {
                return true;
            }

            // Skyrim's actor forward is +Y, so bearing to target is atan2(dx, dy)
            const float bearing = std::atan2(dx, dy);
            const float localYaw = WrapPi(bearing - observerYawRad);
            const float localYawDeg = std::abs(localYaw * kRadToDeg);
            return localYawDeg <= maxAngleDeg;
        }

        /// Distance in metres between two world positions.
        float DistanceMeters(const RE::NiPoint3& a, const RE::NiPoint3& b) noexcept
        {
            return a.GetDistance(b) * kUnitsToMeters;
        }

#if __has_include(<RE/Skyrim.h>)
        /// Skyrim world units per centimetre (70 units == 1 m).
        constexpr float kUnitsPerCm = kUnitsPerMeter / 100.0f;

        /// R14 E7.5 — EyeAnchorFromHeadBone moved to GazeAnchors.hpp (shared with
        /// GazeEngine.cpp, which previously carried a byte-identical copy).
        RE::NiPoint3 EyeAnchorFromHeadBone(const RE::NiAVObject* headBone) noexcept
        {
            return TrueGaze::Anchors::EyeAnchorFromHeadBone(
                headBone, TargetSelector::s_eyeAnchor.forwardCm, TargetSelector::s_eyeAnchor.upCm);
        }

        /// Retrieve the true 3D world position of an actor's EYES (eye anchor projected
        /// from the head bone transform when available). "The eyes are the target."
        /// Dynamically tracks seated postures, counter-leaning idles, crouching, and race scales.
        RE::NiPoint3 GetActorHeadPosition(RE::Actor* actor) noexcept
        {
            if (!actor)
                return RE::NiPoint3{0.0f, 0.0f, 0.0f};
            if (auto* root = actor->Get3D())
            {
                static const char* kHeadCandidates[] = {"NPC Head [Head]", "Head", "Head1",
                                                        "Bip01 Head"};
                for (const auto* name : kHeadCandidates)
                {
                    if (auto* bone = root->GetObjectByName(RE::BSFixedString(name)))
                    {
                        return EyeAnchorFromHeadBone(bone);
                    }
                }
                return RE::NiPoint3{root->world.translate.x, root->world.translate.y,
                                    root->world.translate.z + kEyeHeightOffsetUnits};
            }
            const auto pos = actor->GetPosition();
            return RE::NiPoint3{pos.x, pos.y, pos.z + kEyeHeightOffsetUnits};
        }

        /// Retrieve the true 3D world position of an actor (using 3D node transform if available)
        RE::NiPoint3 GetActorWorldPosition(RE::Actor* actor) noexcept
        {
            if (!actor)
                return RE::NiPoint3{0.0f, 0.0f, 0.0f};
            if (auto* root = actor->Get3D())
            {
                return root->world.translate;
            }
            return actor->GetPosition();
        }
#endif

    } // namespace

    TargetSelector::GazeTarget TargetSelector::ResolveTarget(uint32_t observerFormId,
                                                             ActorGazeRuntime* state,
                                                             float deltaSeconds) noexcept
    {
        GazeTarget target{};

        if (observerFormId == 0)
        {
            return target;
        }

#if __has_include(<RE/Skyrim.h>)
        auto* form = RE::TESForm::LookupByID(observerFormId);
        if (!form)
            return target;

        auto* observer = form->As<RE::Actor>();
        if (!observer)
            return target;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return target;

        const auto observerPos = GetActorWorldPosition(observer);
        const auto playerPos = GetActorWorldPosition(player);
        const auto observerHeadPos = GetActorHeadPosition(observer);
        const auto playerHeadPos = GetActorHeadPosition(player);

        // SCENE DEFER IS A PER-FRAME DECISION (Gold Standard fix, 2026-09-26).
        // The flag is cleared at the top of every resolution and only re-set by
        // the 1c head-track classification below. Previously it was set on the
        // first deferred frame and never cleared when the scene ended, so an
        // actor's head chain stayed yielded to vanilla FOREVER after any
        // NPC-to-NPC scene — TrueGaze silently stopped working for them. A
        // fresh decision each frame means the moment a scene releases the
        // actor, TrueGaze re-engages on the very next frame.
        if (state)
        {
            state->sceneDeferActive = false;
        }

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
            const bool crosshairOnThisActor =
                (playerGaze.onFace && playerGaze.targetFormId == observerFormId);

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
                    IsInVisualCone(observerPos, observer->GetAngleZ(), playerPos,
                                   kMaxHoldVisualConeAngleDeg))
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

        auto* ui = RE::UI::GetSingleton();

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
                auto* topicMgr = RE::MenuTopicManager::GetSingleton();
                if (topicMgr && topicMgr->speaker)
                {
                    if (auto speakerPtr = topicMgr->speaker.get())
                    {
                        if (auto* speakerActor = speakerPtr->As<RE::Actor>())
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
                auto* aimForm = RE::TESForm::LookupByID(playerGaze.targetFormId);
                if (aimForm)
                {
                    if (auto* aimActor = aimForm->As<RE::Actor>())
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
                    if (auto* cTarget = combatPtr.get())
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
            RE::Actor* closestNpc = nullptr;
            float closestDistMeters = 4.5f;
            if (auto* processLists = RE::ProcessLists::GetSingleton())
            {
                for (auto& handle : processLists->highActorHandles)
                {
                    if (auto actorPtr = handle.get())
                    {
                        auto* otherActor = actorPtr.get();
                        if (otherActor && otherActor != player && !otherActor->IsDead() &&
                            !otherActor->IsDisabled())
                        {
                            const auto oPos = GetActorWorldPosition(otherActor);
                            if (IsInVisualCone(observerPos, player->GetAngleZ(), oPos,
                                               kMaxVisualConeAngleDeg))
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
            auto* topicMgr = RE::MenuTopicManager::GetSingleton();
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
                if (auto* dlgActor = dlgPtr->As<RE::Actor>())
                {
                    if (dlgActor != observer && !dlgActor->IsDead() && !dlgActor->IsDisabled())
                    {
                        const auto dPos = GetActorWorldPosition(dlgActor);
                        const float dDist = DistanceMeters(observerPos, dPos);
                        // CRITICAL: Must be within conversational range AND inside forward visual
                        // cone! Bethesda's dialogue target can be an actor in another room or
                        // directly behind the observer. Forcing gaze to an actor at >75 degrees
                        // causes severe neck/head snapping.
                        //
                        // EXCEPTION — THE PLAYER (Kirk directive, September 25 2026,
                        // reconfirmed by the September 26 cart screenshot where Ralof
                        // says "Hey you, you're finally awake" while looking at
                        // someone else): a person ADDRESSING you looks at you, full
                        // stop. Seated/sideways staging (the Helgen cart) puts the
                        // player outside the body-yaw cone, so the cone check must
                        // never veto the player as dialogue partner. Range still
                        // applies — dialogue beyond 6 m is not an address.
                        if (dDist <= 6.0f && (dlgActor == player ||
                                              IsInVisualCone(observerPos, observer->GetAngleZ(),
                                                             dPos, kMaxHoldVisualConeAngleDeg)))
                        {
                            const auto dHeadPos = GetActorHeadPosition(dlgActor);
                            target.targetFormId = dlgActor->GetFormID();
                            target.priority = TargetPriority::DialoguePartner;
                            target.worldX = dHeadPos.x;
                            target.worldY = dHeadPos.y;
                            target.worldZ = dHeadPos.z;
                            target.distanceMeters = dDist;
                            target.isPlayer = (dlgActor == player);
                            if (dlgActor == player && state)
                            {
                                // DIALOGUE PLAYER HOLD: arm the latch (see 2b).
                                state->dialoguePlayerHoldSec = 3.0f;
                            }
                            return target;
                        }
                    }
                }
            }
        }

        // 1c. Engine AI headtracking target (NPC-to-NPC conversations, scenes, idle chatter)
        //
        // SCENE DEFER (Kirk directive, September 25 2026; Gold Standard
        // generalisation, September 26 2026): when the game's own AI has
        // assigned this NPC a headtrack target through scripted scene direction
        // (PackageStart scenes like the Helgen cart, dialogue staging, scripted
        // LookAt from quests or OAR-adjacent mods), vanilla direction is
        // AUTHORITATIVE for its own staged moments. TrueGaze yields — returning
        // a None-priority target so the engine leaves vanilla headtracking
        // untouched — UNLESS the directed target IS the player, in which case
        // TrueGaze takes hold and applies its eye-anchor precision (the player
        // is the scene's focus, and eye-to-eye is what TrueGaze exists to
        // deliver).
        //
        // GOLD STANDARD SLOT MODEL (HighProcessData::HEAD_TRACK_TYPE):
        //   kDefault   — ambient social glances the AI fabricates on its own.
        //                NOT direction; TrueGaze replaces these freely.
        //   kAction    — action-driven look (activating a door/object). Direction.
        //   kScript    — scripted LookAt (quests, OAR-adjacent mods). Direction.
        //   kCombat    — combat targeting. TrueGaze already owns combat gaze
        //                (priority 2 below); NOT treated as scene direction.
        //   kDialogue  — conversation staging. Direction (dialogueItemTarget
        //                above usually claims it first at higher fidelity).
        //   kProcedure — package/scene procedure direction (the Helgen cart's
        //                mechanism). Direction.
        //
        // Reading the slots is a pure data-side array read on HighProcessData —
        // the safest access class in this codebase (no REL::Relocation calls,
        // no engine dispatch). The previous GetHeadtrackTarget() relocation
        // path is retained below as a fallback for actors whose high process
        // data is not yet populated.
        if (auto* high = observer->GetHighProcess())
        {
            // PLAYER PARITY (Kirk directive, September 26 2026): "The Player
            // should be treated the same as the NPC especially when directed
            // to from vanilla Skyrim." Scan ALL direction slots first and
            // prefer a slot that points at the PLAYER over any slot pointing
            // at another NPC. The previous first-match loop took whichever
            // slot came first — in the cart, a slot aimed at a fellow NPC
            // outranked the slot aimed at the player, so the player-directed
            // moments never took hold and the player was ignored.
            bool playerDirected = false;
            RE::Actor* playerDirectedActor = nullptr;
            bool anyDirected = false;
            RE::Actor* directedActor = nullptr;

            for (std::uint32_t slot = 0;
                 slot < static_cast<std::uint32_t>(RE::HighProcessData::HEAD_TRACK_TYPE::kTotal);
                 ++slot)
            {
                if (slot == static_cast<std::uint32_t>(
                                RE::HighProcessData::HEAD_TRACK_TYPE::kDefault) ||
                    slot ==
                        static_cast<std::uint32_t>(RE::HighProcessData::HEAD_TRACK_TYPE::kCombat))
                {
                    continue; // not direction — see the slot model above
                }

                const auto& handle = high->headTrackTarget[slot];
                if (!handle)
                {
                    continue;
                }

                if (auto htPtr = handle.get())
                {
                    if (auto* htActor = htPtr->As<RE::Actor>())
                    {
                        if (htActor != observer && !htActor->IsDead() && !htActor->IsDisabled())
                        {
                            anyDirected = true;
                            if (!directedActor)
                            {
                                directedActor = htActor;
                            }
                            if (htActor == player && !playerDirected)
                            {
                                playerDirected = true;
                                playerDirectedActor = htActor;
                            }
                        }
                    }
                }
            }

            // A player-directed slot WINS over any NPC-directed slot: the
            // player is the scene's focus, and eye-to-eye with the player is
            // what TrueGaze exists to deliver.
            if (playerDirected && playerDirectedActor)
            {
                const auto htHeadPos = GetActorHeadPosition(playerDirectedActor);
                target.targetFormId = playerDirectedActor->GetFormID();
                target.priority = TargetPriority::DialoguePartner;
                target.worldX = htHeadPos.x;
                target.worldY = htHeadPos.y;
                target.worldZ = htHeadPos.z;
                target.distanceMeters = DistanceMeters(observerPos, htHeadPos);
                target.isPlayer = true;
                if (state)
                {
                    state->sceneDeferActive = false;
                    // DIALOGUE PLAYER HOLD: arm the latch so the brief
                    // direction-signal dropouts mid-line do not re-aim the
                    // gaze at a nearer NPC (the 16:43 log showed priority=4
                    // flickering to priority=2 between trace windows).
                    state->dialoguePlayerHoldSec = 3.0f;
                }
                return target;
            }

            if (anyDirected && directedActor)
            {
                // Directed target is another NPC: defer to vanilla scene
                // direction. Signal the engine to stand down for this
                // actor this frame.
                if (state)
                    state->sceneDeferActive = true;
                target.priority = TargetPriority::None;
                target.targetFormId = 0;
                return target;
            }

            // VOICE ADDRESS DETECTION (2026-09-26, "the Player is still being
            // ignored by the NPC during dialogue directed for the player").
            //
            // The log forensics from every cart test show the same thing: during
            // scene-delivered lines ("Hey you, you're finally awake") the
            // headTrackTarget slots and dialogueItemTarget point at OTHER NPCs —
            // the game stages the LOOK direction independently of the SPEECH.
            // But the voice itself cannot lie: HighProcessData carries the
            // actor's live VOICE_STATE (kStart/kContinue = speaking now) and
            // lastSpokenToArray (the refs this actor last delivered dialogue
            // TO). An actor who is SPEAKING and last spoke to the player is
            // addressing the player — "a person addressing you looks at you,
            // full stop" — regardless of what the staged look direction says.
            //
            // Both fields are pure data-side reads on HighProcessData (no
            // relocation calls, no engine dispatch) — the safest access class.
            if (state && state->dialoguePlayerHoldSec <= 0.0f)
            {
                const auto& voice = high->voiceState;
                const bool isSpeaking =
                    voice.underlying() == static_cast<std::uint32_t>(RE::VOICE_STATE::kStart) ||
                    voice.underlying() == static_cast<std::uint32_t>(RE::VOICE_STATE::kContinue);

                if (isSpeaking)
                {
                    for (const auto& spokenHandle : high->lastSpokenToArray)
                    {
                        if (auto spokenPtr = spokenHandle.get())
                        {
                            if (spokenPtr.get() == player)
                            {
                                // This actor is speaking TO the player right now.
                                const auto pHeadPos = GetActorHeadPosition(player);
                                target.targetFormId = player->GetFormID();
                                target.priority = TargetPriority::DialoguePartner;
                                target.worldX = pHeadPos.x;
                                target.worldY = pHeadPos.y;
                                target.worldZ = pHeadPos.z;
                                target.distanceMeters = DistanceMeters(observerPos, pHeadPos);
                                target.isPlayer = true;
                                state->sceneDeferActive = false;
                                state->dialoguePlayerHoldSec = 3.0f; // hold through the line
                                return target;
                            }
                        }
                    }
                }
            }
        }
        else if (auto* process = observer->GetActorRuntimeData().currentProcess)
        {
            if (auto htHandle = process->GetHeadtrackTarget())
            {
                if (auto htPtr = htHandle.get())
                {
                    if (auto* htActor = htPtr->As<RE::Actor>())
                    {
                        if (htActor != observer && !htActor->IsDead() && !htActor->IsDisabled())
                        {
                            // Directed target is the player: TrueGaze takes hold.
                            if (htActor == player)
                            {
                                const auto htHeadPos = GetActorHeadPosition(htActor);
                                target.targetFormId = htActor->GetFormID();
                                target.priority = TargetPriority::DialoguePartner;
                                target.worldX = htHeadPos.x;
                                target.worldY = htHeadPos.y;
                                target.worldZ = htHeadPos.z;
                                target.distanceMeters = DistanceMeters(observerPos, htHeadPos);
                                target.isPlayer = true;
                                if (state)
                                    state->sceneDeferActive = false;
                                return target;
                            }

                            // Directed target is another NPC: defer to vanilla scene
                            // direction. Signal the engine to stand down for this
                            // actor this frame.
                            if (state)
                                state->sceneDeferActive = true;
                            target.priority = TargetPriority::None;
                            target.targetFormId = 0;
                            return target;
                        }
                    }
                }
            }
        }

        // 2. Combat target. Reached through the actor's runtime data as a handle,
        //    not a direct accessor, and validated for liveness before use.
        RE::Actor* combatTarget = nullptr;
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
            if (cDist <= 15.0f &&
                IsInVisualCone(observerPos, observer->GetAngleZ(), combatPos, 75.0f))
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

        // 2b. DIALOGUE PLAYER HOLD — serve the latched player target while the
        // hold timer counts down. Armed whenever a direction slot or the
        // dialogue item resolves to the player; this re-serves the player
        // through the direction signal's mid-line flicker so an NPC speaking
        // TO the player keeps looking at the player for the whole line.
        if (state && state->dialoguePlayerHoldSec > 0.0f)
        {
            state->dialoguePlayerHoldSec -= deltaSeconds;

            if (playerDistanceMeters <= kNearbyRangeMeters)
            {
                target.targetFormId = player->GetFormID();
                target.priority = TargetPriority::DialoguePartner;
                target.worldX = playerHeadPos.x;
                target.worldY = playerHeadPos.y;
                target.worldZ = playerHeadPos.z;
                target.distanceMeters = playerDistanceMeters;
                target.isPlayer = true;
                return target;
            }
            // Player out of range: let the hold expire naturally and fall
            // through to the normal chain.
        }

        // 3. Target Fixation Stability (Dwell Time Hysteresis)
        // If the actor is currently fixating on a valid target, maintain lock for at least 1.5s
        // before switching to avoid rapid micro-flapping between nearby actors.
        if (state && state->trackedTargetFormId != 0)
        {
            if (state->trackedTargetFormId == player->GetFormID())
            {
                if (playerDistanceMeters <= kNearbyRangeMeters &&
                    IsInVisualCone(observerPos, observer->GetAngleZ(), playerPos,
                                   kMaxHoldVisualConeAngleDeg))
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
                auto* heldForm = RE::TESForm::LookupByID(state->trackedTargetFormId);
                auto* heldActor = heldForm ? heldForm->As<RE::Actor>() : nullptr;
                if (heldActor && !heldActor->IsDead() && !heldActor->IsDisabled())
                {
                    const auto hPos = GetActorWorldPosition(heldActor);
                    const float hDist = DistanceMeters(observerPos, hPos);
                    if (hDist <= 5.0f && IsInVisualCone(observerPos, observer->GetAngleZ(), hPos,
                                                        kMaxHoldVisualConeAngleDeg))
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

        // 4. Social Candidate Scan — NPCs AND the player compete fairly on distance.
        //
        // The player was previously EXCLUDED from this scan (`otherActor != player`)
        // and evaluated in a separate, lower-priority block. In tight scenes like the
        // Helgen cart, other NPCs always won the distance comparison and the player
        // was never selected. Fix: the player enters the same scan as everyone else,
        // with a wider visual cone (110° — social peripheral attention) versus the
        // strict 75° for other NPCs. This makes them a fair candidate while still
        // preventing backward neck-snaps for non-player actors.
        //
        // Range: 6m conversational room range for NPCs, kNearbyRangeMeters (12m) for
        // the player (the player is the most socially salient entity in the world).
        RE::Actor* closestCandidate = nullptr;
        float closestCandidateDist = kNearbyRangeMeters; // player can win up to 12m
        bool closestIsPlayer = false;

        // --- Player first (widest cone, socially salient) ---
        // The player is ALWAYS a valid gaze target regardless of their camera mode
        // (1st or 3rd person). In 1st person only the player's own eyes are TrueGaze-
        // controlled; in 3rd person the full skeleton is. But from the NPC's
        // perspective, the player is always a person standing/sitting there whose
        // eyes can be looked into. The eye-anchor projection onto the player's head
        // bone gives NPCs a correct eye-level aim point in either camera mode.
        //
        // PLAYER SALIENCE BIAS: the player is the most socially significant person
        // in any scene. In the Helgen cart every NPC sits at nearly the same distance
        // from each other, so a raw distance comparison let Ralof stare at Lokir even
        // while SPEAKING TO THE PLAYER. The player therefore gets a 1.5m distance
        // advantage — they win every near-tie. When the NPC is actively in dialogue
        // with the player (dialogueItemTarget == player), the bias strengthens to
        // 3m: a person addressing you looks at you, full stop.
        {
            const bool playerInCone = IsInVisualCone(observerPos, observer->GetAngleZ(), playerPos,
                                                     kMaxHoldVisualConeAngleDeg);
            if (playerInCone && playerDistanceMeters < closestCandidateDist)
            {
                float d = playerDistanceMeters;

                // Strong bias when this NPC's active dialogue item targets the player
                // (they are speaking TO the player right now).
                bool speakingToPlayer = false;
                if (auto dlgHandle = observer->GetActorRuntimeData().dialogueItemTarget)
                {
                    if (auto dlgPtr = dlgHandle.get())
                    {
                        speakingToPlayer = (dlgPtr.get() == player);
                    }
                }
                d -= speakingToPlayer ? 3.0f : 1.5f;

                if (state && state->trackedTargetFormId == player->GetFormID())
                {
                    d -= 0.8f; // Hysteresis advantage
                }
                closestCandidateDist = d;
                closestCandidate = player;
                closestIsPlayer = true;
            }
        }

        // --- Other NPCs (strict forward cone, 6m conversational range) ---
        //
        // R14 E2.3 — PER-FRAME POSITION CACHE. The original loop called
        // GetActorWorldPosition (Get3D + world translate) for every candidate,
        // per OBSERVER, per frame. With N nearby actors each ticking, the total
        // cost was O(N^2) scene-graph traversals per frame — the dominant crowd
        // cost in dense cells. The cache below is built ONCE per frame by
        // whichever observer scans first and is reused (read-only) by every
        // subsequent observer in the same frame, tagged with the engine frame
        // counter so a stale cache is rebuilt. Game-thread only, like
        // everything in this module — no locks.
        struct CachedCandidate
        {
            RE::Actor* actor{nullptr};
            RE::NiPoint3 worldPos{};
        };
        static RE::BSTArray<CachedCandidate> s_scanCache;
        static uint64_t s_scanFrameTag = 0;

        const uint64_t currentFrame = GazeEngine::Get().GetFrameCounter();
        if (s_scanFrameTag != currentFrame)
        {
            s_scanCache.clear();
            if (auto* processLists = RE::ProcessLists::GetSingleton())
            {
                s_scanCache.reserve(processLists->highActorHandles.size());
                for (auto& handle : processLists->highActorHandles)
                {
                    if (auto actorPtr = handle.get())
                    {
                        auto* otherActor = actorPtr.get();
                        if (otherActor && otherActor != player && !otherActor->IsDead() &&
                            !otherActor->IsDisabled())
                        {
                            CachedCandidate entry{};
                            entry.actor = otherActor;
                            entry.worldPos = GetActorWorldPosition(otherActor);
                            s_scanCache.push_back(entry);
                        }
                    }
                }
            }
            s_scanFrameTag = currentFrame;
        }

        // Consume the cache: per-observer work is now only cone + distance
        // arithmetic on pre-fetched positions — no scene-graph traversal.
        for (const auto& candidate : s_scanCache)
        {
            auto* otherActor = candidate.actor;
            if (otherActor == observer)
            {
                continue;
            }

            const auto& oPos = candidate.worldPos;

            // STRICT VISUAL CONE: If target is behind the observer (>65 deg),
            // DO NOT select it! A human does not turn their head backward over their shoulder.
            if (!IsInVisualCone(observerPos, observer->GetAngleZ(), oPos, kMaxVisualConeAngleDeg))
            {
                continue;
            }

            float d = DistanceMeters(observerPos, oPos);
            // Cap NPC candidate range at conversational distance
            if (d > 6.0f)
            {
                continue;
            }
            // NOTE: no player bias here — the player's 1.5m/3m salience
            // advantage was already applied on the player's own entry
            // above, so NPCs compete from their raw distance.
            // Hysteresis advantage for previously tracked target
            if (state && state->trackedTargetFormId == otherActor->GetFormID())
            {
                d -= 0.8f;
            }

            if (d < closestCandidateDist)
            {
                closestCandidateDist = d;
                closestCandidate = otherActor;
                closestIsPlayer = (otherActor == player); // always false here
            }
        }

        if (closestCandidate)
        {
            const auto cHeadPos = GetActorHeadPosition(closestCandidate);
            target.targetFormId = closestCandidate->GetFormID();
            target.priority = TargetPriority::NearbyActor;
            target.worldX = cHeadPos.x;
            target.worldY = cHeadPos.y;
            target.worldZ = cHeadPos.z;
            target.distanceMeters = DistanceMeters(observerHeadPos, cHeadPos);
            target.isPlayer = closestIsPlayer;
            if (state)
                state->fixationHoldSec = 0.0f;
            return target;
        }

        // 5. Ambient interest: a point ahead of the observer, oriented along the
        //    actor's heading so they look forward along their own facing angle
        //    rather than staring sideways or snapping backwards.
        if (state)
            state->fixationHoldSec = 0.0f;
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
