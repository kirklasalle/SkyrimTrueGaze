#include "AnimationHook.hpp"

namespace TrueGaze::Engine {

namespace {

#if __has_include(<RE/Skyrim.h>)
    // Trampoline hook target for actor animation updates
    struct ActorAnimationHook
    {
        static void Hook(RE::Actor* a_actor, float a_delta)
        {
            _original(a_actor, a_delta);

            if (!a_actor || !AnimationHook::IsActorEligibleForGaze(a_actor->GetFormID())) {
                return;
            }

            // Procedural additive bone rotation would be applied here after Havok evaluation
        }

        static inline REL::Relocation<decltype(Hook)> _original;
    };
#endif

} // namespace

void AnimationHook::Install() noexcept
{
#if __has_include(<RE/Skyrim.h>)
    logger::info("[TrueGaze] Installing post-Havok animation hooks...");
    // Future: Relocation installation when running within Skyrim process address space
#else
    spdlog::info("[TrueGaze] Standalone mode: AnimationHook compiled with abstract engine interface.");
#endif
}

bool AnimationHook::IsActorEligibleForGaze(uint32_t actorFormId) noexcept
{
    if (actorFormId == 0) {
        return false;
    }

#if __has_include(<RE/Skyrim.h>)
    auto* form = RE::TESForm::LookupByID(actorFormId);
    if (!form) return false;

    auto* actor = form->As<RE::Actor>();
    if (!actor) return false;

    // Check if 3D root is loaded
    if (!actor->Get3D()) return false;

    // Check life and conscious state
    if (actor->IsDead() || actor->IsSleeping() || actor->IsParalyzed()) {
        return false;
    }

    // Check ragdoll state
    if (actor->IsInRagdollState()) {
        return false;
    }

    return true;
#else
    return true;
#endif
}

float AnimationHook::GetActorGazeWeight(uint32_t actorFormId) noexcept
{
    if (!IsActorEligibleForGaze(actorFormId)) {
        return 0.0f;
    }

#if __has_include(<RE/Skyrim.h>)
    auto* form = RE::TESForm::LookupByID(actorFormId);
    if (!form) return 0.0f;

    auto* actor = form->As<RE::Actor>();
    if (!actor) return 0.0f;

    // Active dialogue partner gets full attention
    auto* ui = RE::UI::GetSingleton();
    if (ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
        return 1.0f;
    }

    // Combat state reduces leisurely gaze exploration
    if (actor->IsInCombat()) {
        return 0.45f;
    }

    return 1.0f;
#else
    return 1.0f;
#endif
}

} // namespace TrueGaze::Engine
