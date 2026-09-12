#include "TargetSelector.hpp"
#include <cmath>

namespace TrueGaze::Engine
{

    namespace
    {

        /// Skyrim world units per metre. These were bare literals (0.01428f, 160.0f)
        /// repeated throughout the original implementation; a single wrong digit in any
        /// one of them would have produced a silently wrong distance calculation.
        constexpr float kUnitsPerMeter = 70.0f;
        constexpr float kUnitsToMeters = 1.0f / kUnitsPerMeter;

        /// Approximate eye height above an actor's origin, in Skyrim units (~1.6 m).
        constexpr float kEyeHeightOffsetUnits = 160.0f;

        /// An approaching actor is worth tracking out to this range.
        constexpr float kNearbyRangeMeters = 8.0f;

        /// Distance to an ambient focus point placed ahead of the observer.
        constexpr float kAmbientForwardUnits = 200.0f;
        constexpr float kAmbientNominalMeters = 3.0f;

        /// Distance in metres between two world positions.
        float DistanceMeters(const RE::NiPoint3 &a, const RE::NiPoint3 &b) noexcept
        {
            return a.GetDistance(b) * kUnitsToMeters;
        }

    } // namespace

    TargetSelector::GazeTarget TargetSelector::ResolveTarget(uint32_t observerFormId) noexcept
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

        const auto observerPos = observer->GetPosition();
        const auto playerPos = player->GetPosition();

        // 1. Highest priority: the active dialogue partner.
        // UI::IsMenuOpen is non-const, so the singleton must not be captured as const.
        auto *ui = RE::UI::GetSingleton();
        if (ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME))
        {
            target.targetFormId = player->GetFormID();
            target.priority = TargetPriority::DialoguePartner;
            target.worldX = playerPos.x;
            target.worldY = playerPos.y;
            target.worldZ = playerPos.z + kEyeHeightOffsetUnits;
            target.distanceMeters = DistanceMeters(observerPos, playerPos);
            target.isPlayer = true;
            return target;
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
            const auto combatPos = combatTarget->GetPosition();
            target.targetFormId = combatTarget->GetFormID();
            target.priority = TargetPriority::CombatTarget;
            target.worldX = combatPos.x;
            target.worldY = combatPos.y;
            target.worldZ = combatPos.z + kEyeHeightOffsetUnits;
            target.distanceMeters = DistanceMeters(observerPos, combatPos);
            target.isPlayer = (combatTarget == player);
            return target;
        }

        // 3. Nearby player, or an approaching actor.
        const float playerDistanceMeters = DistanceMeters(observerPos, playerPos);
        if (playerDistanceMeters <= kNearbyRangeMeters)
        {
            target.targetFormId = player->GetFormID();
            target.priority = TargetPriority::NearbyActor;
            target.worldX = playerPos.x;
            target.worldY = playerPos.y;
            target.worldZ = playerPos.z + kEyeHeightOffsetUnits;
            target.distanceMeters = playerDistanceMeters;
            target.isPlayer = true;
            return target;
        }

        // 4. Ambient interest: a point ahead of the observer, so the actor has a
        //    plausible focus instead of snapping back to dead-forward.
        target.priority = TargetPriority::AmbientInterest;
        target.worldX = observerPos.x + kAmbientForwardUnits;
        target.worldY = observerPos.y;
        target.worldZ = observerPos.z + kEyeHeightOffsetUnits;
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
