#include "AnimationHook.hpp"
#include "GazeEngine.hpp"
#include "EyeAimConstraint.hpp"
#include "ConfigManager.hpp"

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#endif

namespace TrueGaze::Engine
{

    namespace
    {

#if __has_include(<RE/Skyrim.h>)

        /// The main per-frame driver.
        ///
        /// ## Why this is hooked rather than an animation function
        ///
        /// The original design intended to relocate a post-Havok animation update.
        /// That was never implemented, and an unverified address is precisely the
        /// failure mode this project has already been burned by: if the relocation
        /// does not bind, the plugin loads cleanly, logs success, and does nothing.
        ///
        /// This hooks the main update loop instead, via a vtable function whose
        /// index is stable across SE, AE and VR. That gives three properties the
        /// address-based approach cannot:
        ///
        ///   1. It runs on the game thread, which skeleton mutation requires.
        ///   2. It runs once per frame, after animation evaluation for that frame,
        ///      so the deflection composes on top of the posed skeleton.
        ///   3. If the vtable slot were ever wrong, it would fault immediately and
        ///      visibly rather than silently doing nothing.
        ///
        /// The deflection is idempotent within a frame (see EyeAimConstraint), so
        /// running more than once per frame is harmless.
        struct MainUpdateHook
        {
            static void Hook(RE::Main *a_main, float a_delta)
            {
                _original(a_main, a_delta);

                if (!a_main || !ConfigManager::GetSingleton().enableTrueGaze)
                {
                    return;
                }

                // 1. Apply gaze on top of this frame's posed skeleton.
                EyeAimConstraint::BeginFrame();
                AnimationHook::TickAllActors(a_delta);
                AnimationHook::FrameEnd();
            }

            static inline REL::Relocation<decltype(Hook)> _original;
            static inline bool _installed{false};
        };

        void TickActorList(RE::BSTArray<RE::ActorHandle> &list,
                           bool allowCreatures,
                           float deltaSeconds,
                           GazeEngine &engine) noexcept
        {
            for (auto &handle : list)
            {
                auto actorPtr = handle.get();
                if (!actorPtr)
                {
                    continue;
                }

                auto *actor = actorPtr.get();
                if (!actor)
                {
                    continue;
                }

                if (!allowCreatures && !actor->IsHumanoid())
                {
                    continue;
                }

                engine.TickActor(actor, deltaSeconds);
            }
        }

#endif

    } // namespace

    void AnimationHook::Install() noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (MainUpdateHook::_installed)
        {
            return;
        }

        logger::info("[TrueGaze] Installing gaze driver on the main update loop...");

        REL::Relocation<std::uintptr_t> mainVtbl{RE::VTABLE_Main[0]};
        MainUpdateHook::_original = mainVtbl.write_vfunc(0x05, MainUpdateHook::Hook);
        MainUpdateHook::_installed = true;

        logger::info("[TrueGaze] Gaze driver installed.");
#else
        logger::info("[TrueGaze] Standalone mode: gaze driver not installed.");
#endif
    }

    void AnimationHook::TickAllActors(float deltaSeconds) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        auto *processLists = RE::ProcessLists::GetSingleton();
        if (!processLists)
        {
            return;
        }

        auto &engine = GazeEngine::Get();
        const bool creatures = ConfigManager::GetSingleton().enableCreatures;

        TickActorList(processLists->highActorHandles, creatures, deltaSeconds, engine);
        TickActorList(processLists->middleHighActorHandles, creatures, deltaSeconds, engine);
#else
        (void)deltaSeconds;
#endif
    }

    void AnimationHook::FrameEnd() noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        auto &engine = GazeEngine::Get();
        engine.ReleaseBones();
        engine.EndFrame(RE::GetSecondsSinceLastFrame());
#endif
    }

    bool AnimationHook::IsActorEligibleForGaze(uint32_t actorFormId) noexcept
    {
        if (actorFormId == 0)
        {
            return false;
        }

#if __has_include(<RE/Skyrim.h>)
        auto *form = RE::TESForm::LookupByID(actorFormId);
        if (!form)
        {
            return false;
        }

        auto *actor = form->As<RE::Actor>();
        if (!actor)
        {
            return false;
        }

        // 3D must be loaded; without it there is no skeleton to rotate.
        if (!actor->Get3D())
        {
            return false;
        }

        // Only living actors have a meaningful head pose. kAlive covers the resting
        // case; anything else (dead, bleedout, essential-down, reanimate) is out.
        if (actor->GetLifeState() != RE::ACTOR_LIFE_STATE::kAlive)
        {
            return false;
        }

        // Unconscious actors (paralysis, sleep, knockout) should not track.
        if (auto *actorState = actor->AsActorState())
        {
            if (actorState->IsUnconscious())
            {
                return false;
            }
        }

        if (actor->IsInRagdollState())
        {
            return false;
        }

        return true;
#else
        return true;
#endif
    }

    float AnimationHook::GetActorGazeWeight(uint32_t actorFormId) noexcept
    {
        if (!IsActorEligibleForGaze(actorFormId))
        {
            return 0.0f;
        }

#if __has_include(<RE/Skyrim.h>)
        auto *form = RE::TESForm::LookupByID(actorFormId);
        if (!form)
        {
            return 0.0f;
        }

        auto *actor = form->As<RE::Actor>();
        if (!actor)
        {
            return 0.0f;
        }

        // An active dialogue partner gets full attention.
        auto *ui = RE::UI::GetSingleton();
        if (ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME))
        {
            return 1.0f;
        }

        // Combat reduces leisurely gaze exploration.
        if (actor->IsInCombat())
        {
            return 0.45f;
        }

        return 1.0f;
#else
        return 1.0f;
#endif
    }

} // namespace TrueGaze::Engine
