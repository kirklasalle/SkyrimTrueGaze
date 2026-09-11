#include "TargetSelector.hpp"
#include <cmath>

namespace TrueGaze::Engine {

TargetSelector::GazeTarget TargetSelector::ResolveTarget(uint32_t observerFormId) noexcept
{
    GazeTarget target{};

    if (observerFormId == 0) {
        return target;
    }

#if __has_include(<RE/Skyrim.h>)
    auto* form = RE::TESForm::LookupByID(observerFormId);
    if (!form) return target;

    auto* observer = form->As<RE::Actor>();
    if (!observer) return target;

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return target;

    // 1. Highest Priority: Active Dialogue Partner
    auto* ui = RE::UI::GetSingleton();
    if (ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
        auto playerPos = player->GetPosition();
        target.targetFormId = player->GetFormID();
        target.priority = TargetPriority::DialoguePartner;
        target.worldX = playerPos.x;
        target.worldY = playerPos.y;
        target.worldZ = playerPos.z + 160.0f; // Eye height offset in Skyrim units (~1.6m)
        target.distanceMeters = observer->GetPosition().GetDistance(playerPos) * 0.01428f; // ~70 units/meter
        target.isPlayer = true;
        return target;
    }

    // 2. Combat Target
    auto* combatTarget = observer->GetCombatTarget().get();
    if (combatTarget) {
        auto pos = combatTarget->GetPosition();
        target.targetFormId = combatTarget->GetFormID();
        target.priority = TargetPriority::CombatTarget;
        target.worldX = pos.x;
        target.worldY = pos.y;
        target.worldZ = pos.z + 160.0f;
        target.distanceMeters = observer->GetPosition().GetDistance(pos) * 0.01428f;
        target.isPlayer = (combatTarget == player);
        return target;
    }

    // 3. Nearby Player / Approaching Actor
    float distUnits = observer->GetPosition().GetDistance(player->GetPosition());
    float distMeters = distUnits * 0.01428f;
    if (distMeters <= 8.0f) {
        auto playerPos = player->GetPosition();
        target.targetFormId = player->GetFormID();
        target.priority = TargetPriority::NearbyActor;
        target.worldX = playerPos.x;
        target.worldY = playerPos.y;
        target.worldZ = playerPos.z + 160.0f;
        target.distanceMeters = distMeters;
        target.isPlayer = true;
        return target;
    }

    // 4. Ambient Interest
    target.priority = TargetPriority::AmbientInterest;
    auto obsPos = observer->GetPosition();
    target.worldX = obsPos.x + 200.0f;
    target.worldY = obsPos.y;
    target.worldZ = obsPos.z + 160.0f;
    target.distanceMeters = 3.0f;
    return target;

#else
    // Standalone fallback: player default
    target.targetFormId = 0x14; // Default player FormID
    target.priority = TargetPriority::NearbyActor;
    target.worldX = 0.0f;
    target.worldY = 150.0f;
    target.worldZ = 160.0f;
    target.distanceMeters = 1.5f;
    target.isPlayer = true;
    return target;
#endif
}

} // namespace TrueGaze::Engine
