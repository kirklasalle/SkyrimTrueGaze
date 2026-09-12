#include "AnimationHook.hpp"
#include "GazeEngine.hpp"
#include "EyeAimConstraint.hpp"
#include "ConfigManager.hpp"

#include <chrono>

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#endif

namespace TrueGaze::Engine
{

    namespace
    {

#if __has_include(<RE/Skyrim.h>)

        /// Tracks frame boundaries without needing a second hook.
        ///
        /// `Actor::Update` is called once per actor per frame, in sequence. A gap
        /// of more than FRAME_GAP_SEC between consecutive calls therefore marks the
        /// boundary between one frame's actor updates and the next. That is enough
        /// to pair `EyeAimConstraint::BeginFrame()` with `Withdraw()` without
        /// introducing another, riskier hook.
        ///
        /// Declared before ActorUpdateHook because the hook calls into it.
        struct GameFrame
        {
            static constexpr float FRAME_GAP_SEC = 0.05f; // 50 ms, i.e. below 20 FPS

            static void NoteActorUpdate(float a_delta) noexcept
            {
                const auto now = std::chrono::steady_clock::now();

                if (_started && std::chrono::duration<float>(now - _lastUpdate).count() > FRAME_GAP_SEC)
                {
                    // The previous frame's actor updates have finished.
                    FinishFrame();
                }

                _started = true;
                _lastUpdate = now;

                if (a_delta > 0.0f && a_delta < 0.5f)
                {
                    _pendingDelta = a_delta;
                }
            }

            /// Called when the burst of actor updates for one frame has ended.
            static void FinishFrame() noexcept
            {
                auto &engine = GazeEngine::Get();

                // Restore the previous frame's bones before composing this frame's
                // deflection, so the deflection is always applied to the animated
                // pose rather than on top of the previous deflection.
                engine.ReleaseBones();

                EyeAimConstraint::BeginFrame();
                AnimationHook::TickAllActors(_pendingDelta);
                AnimationHook::FrameEnd();
            }

            static inline std::chrono::steady_clock::time_point _lastUpdate{};
            static inline float _pendingDelta{1.0f / 60.0f};
            static inline bool _started{false};
        };

        /// The per-frame driver.
        ///
        /// ## IMPORTANT: why this hooks Actor::Update and not RE::Main
        ///
        /// An earlier revision of this file hooked `RE::VTABLE_Main[0]` at vtable
        /// slot `0x05`. **That was a bug, and a crash-class one.**
        ///
        /// `RE::Main` declares exactly three virtual functions: the destructor
        /// (slot 0), `ProcessEvent` for `PositionPlayerEvent` (slot 1), and
        /// `ProcessEvent` for `BSGamerProfileEvent` (also slot 1 under the other
        /// ABI). **Slot 0x05 does not exist.** Writing a function pointer there
        /// would have corrupted five unrelated vtable entries, and the next
        /// virtual call through any of them would have jumped into our hook with
        /// a mismatched signature. It also meant the gaze engine never ran at all.
        ///
        /// This is precisely the failure mode the September 2026 audit identified:
        /// code that compiles, logs success, and does not do the thing. It is
        /// worth recording that the *second* attempt at this hook also got it
        /// wrong, and that only a direct read of the SDK header caught it.
        /// A vtable index cannot be guessed reliably from memory.
        ///
        /// ## What this hooks instead
        ///
        /// `RE::Actor::Update(float)`, vtable slot `0xAD`. Verified against the
        /// vendored SDK:
        ///
        ///   * `Actor::Update(float a_delta)` is declared at slot `0xAD`
        ///     (RE/A/Actor.h:388).
        ///   * `RE::Character` does **not** override it (no `Update` in
        ///     Character.h).
        ///   * `RE::PlayerCharacter` does **not** override it either.
        ///
        /// So every actor and the player share the single inherited implementation
        /// at `Actor::Update`. Patching that one vtable entry gives one call per
        /// actor per frame, with the game's own delta time, on the game thread —
        /// exactly what the simulation needs, and the same shape of hook the
        /// original design intended.
        ///
        /// `RE::VTABLE_Actor` has 10 entries (RE/Offsets_VTABLE.h:2142), so index
        /// `0xAD` is well inside the real vtable.
        struct ActorUpdateHook
        {
            static void Hook(RE::Actor *a_actor, float a_delta)
            {
                // Advance the game's own state FIRST. Any early return below would
                // otherwise skip the base implementation and break the actor.
                //
                // This call is deliberately OUTSIDE the try/catch: it is the game's
                // own code, and it must behave exactly as it would without us. If it
                // throws, that is the game's business, not ours to swallow.
                _original(a_actor, a_delta);

                // Everything that follows is ours. A defect in the simulation must
                // degrade gaze, not take the game down with it.
                //
                // LIMITATION (stated honestly): this catches C++ exceptions only.
                // It does NOT catch access violations (SEH), which are the most
                // likely failure mode for a bad bone or null dereference. Those
                // still terminate the process. This is a mitigation, not immunity.
                try
                {
                    if (!a_actor || !ConfigManager::GetSingleton().enableTrueGaze)
                    {
                        return;
                    }

                    GameFrame::NoteActorUpdate(a_delta);
                }
                catch (const std::exception &e)
                {
                    ReportTickFailure(e.what());
                }
                catch (...)
                {
                    ReportTickFailure("unknown exception");
                }
            }

            /// Logs the first few failures loudly, then goes quiet.
            ///
            /// A per-actor hook that fails thousands of times a second would
            /// otherwise fill the log with identical lines and make the real
            /// problem harder to find. The first failures are the informative ones.
            static void ReportTickFailure(const char *a_what) noexcept
            {
                constexpr int kMaxReports = 5;
                if (_failuresReported < kMaxReports)
                {
                    logger::error("[TrueGaze] Gaze tick threw ({}). Frame skipped.{}",
                                  a_what,
                                  _failuresReported + 1 == kMaxReports
                                      ? " Further occurrences will be silenced."
                                      : "");
                }
                ++_failuresReported;
            }

            static inline REL::Relocation<decltype(Hook)> _original;
            static inline bool _installed{false};
            static inline int _failuresReported{0};
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
        if (ActorUpdateHook::_installed)
        {
            return;
        }

        logger::info("[TrueGaze] Installing gaze driver on Actor::Update (vtable slot 0xAD)...");

        REL::Relocation<std::uintptr_t> actorVtbl{RE::VTABLE_Actor[0]};
        ActorUpdateHook::_original = actorVtbl.write_vfunc(0xAD, ActorUpdateHook::Hook);
        ActorUpdateHook::_installed = true;

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
