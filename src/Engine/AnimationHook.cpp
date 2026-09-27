#include "AnimationHook.hpp"
#include "GazeEngine.hpp"
#include "EyeAimConstraint.hpp"
#include "ConfigManager.hpp"

#include <atomic>

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#include <RE/H/HighProcessData.h>
#include <RE/S/SendHUDMessage.h>
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace TrueGaze::Engine
{

    namespace
    {

#if __has_include(<RE/Skyrim.h>)

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
        /// exactly what the kinematics engine needs, and the same shape of hook the
        /// original design intended.
        ///
        struct ActorTag
        {
        };
        struct CharacterTag
        {
        };
        struct PlayerTag
        {
        };

        static inline std::atomic<uint32_t> s_mainThreadId{0};

        /// SEH fault reporter (Gold Standard, 2026-09-26). Called from the
        /// __except filter in RunGazeFrameSeh. Returns EXCEPTION_EXECUTE_HANDLER
        /// so the frame is abandoned and the game survives; logs the fault code
        /// and the faulting address, which with the PDB localises the crash to
        /// the exact source line. Rate-limited like ReportTickFailure.
        int ReportSehFault(unsigned int code, EXCEPTION_POINTERS *info) noexcept
        {
            static std::atomic<uint64_t> s_faultReports{0};
            const auto report = s_faultReports.fetch_add(1, std::memory_order_relaxed);
            if (report < 5)
            {
                const void *addr = info && info->ExceptionRecord
                                       ? info->ExceptionRecord->ExceptionAddress
                                       : nullptr;
                logger::error("[TrueGaze] SEH fault 0x{:08X} at {} (gaze frame abandoned; game continues). "
                              "Fault #{} — address + PDB localises the line.",
                              code,
                              fmt::ptr(addr),
                              report + 1);
            }
            return EXCEPTION_EXECUTE_HANDLER;
        }

        /// The TrueGaze-owned frame body, extracted so it can be wrapped in a
        /// structured exception handler (see RunGazeFrameSeh below). This
        /// function must not require unwinding semantics at its own top level
        /// (no try/catch of C++ objects with destructors) — MSVC forbids mixing
        /// __try with objects needing unwinding in ONE function; splitting the
        /// frame body from the __try frame is the standard compliant shape.
        static void RunGazeFrameBody(RE::Actor *a_actor, float a_delta)
        {
            s_mainThreadId.store(GetCurrentThreadId(), std::memory_order_relaxed);

            // PlayerCharacter::Update runs reliably once per frame on the main thread.
            if (a_actor && ConfigManager::GetSingleton().enableTrueGaze &&
                a_delta > 0.0f && a_delta < 0.5f)
            {
                // 1. Prepare bone constraints for this frame by restoring previously touched
                // bones to their pristine animated baseline before composing fresh gaze.
                EyeAimConstraint::BeginFrame();

                auto *camera = RE::PlayerCamera::GetSingleton();
                const bool isThirdPerson = camera && camera->IsInThirdPerson();

                static bool s_lastThirdPerson = false;
                if (isThirdPerson != s_lastThirdPerson)
                {
                    s_lastThirdPerson = isThirdPerson;
                    if (ConfigManager::GetSingleton().debugGazeRays)
                    {
                        if (isThirdPerson)
                        {
                            RE::SendHUDMessage::ShowHUDMessage("[TrueGaze] Camera: 3rd Person (Biomechanical Eye Tracking Active)");
                        }
                        else
                        {
                            RE::SendHUDMessage::ShowHUDMessage("[TrueGaze] Camera: 1st Person (Crosshair Aim)");
                        }
                    }
                }

                if (isThirdPerson)
                {
                    // 3rd Person: Player eyes engage TrueGaze normally.
                    GazeEngine::Get().TickActor(a_actor, a_delta);
                }
                else
                {
                    // 1st Person: User control and mouse crosshair direct looking.
                    EyeAimConstraint::WithdrawActor(a_actor->GetFormID());
                }

                // 2. Orchestrate gaze evaluation and bone updates for all active nearby NPCs
                // and creatures on the main game thread, perfectly synchronized with this frame's delta.
                AnimationHook::TickAllActors(a_delta);
            }

            // Anchoring EndFrame here drives actor eviction, bounds memory,
            // and records accurate frame profiling metrics.
            GazeEngine::Get().EndFrame(a_delta);
        }

#ifdef _WIN32
        /// SEH frame for the gaze frame body (Gold Standard, 2026-09-26).
        ///
        /// HISTORY: every hard crash in this project's testing history has been
        /// an SEH access violation (hand-crafted NIFs, GatherTemperament during
        /// load, the console-table lessons). C++ try/catch cannot catch them —
        /// the limitation was documented honestly in the old hook body ("It
        /// does NOT catch access violations"). The Gold Standard contract
        /// changes that: a defect in the kinematics engine must degrade gaze,
        /// never take the game down, for ANY failure class.
        ///
        /// EXCEPTION_EXECUTE_HANDLER on the first few faults: log the code +
        /// address, skip the rest of the frame, keep the process alive. The
        /// address, with the PDB, localises the fault to the exact line — the
        /// 2026-09-25 crashes took hours because no such record existed.
        ///
        /// The game's own _original() update is deliberately NOT inside this
        /// frame (it stays in Hook above): Skyrim's code is not ours to swallow.
        static void RunGazeFrameSeh(RE::Actor *a_actor, float a_delta)
        {
            __try
            {
                RunGazeFrameBody(a_actor, a_delta);
            }
            __except (ReportSehFault(
                GetExceptionCode(),
                static_cast<EXCEPTION_POINTERS *>(GetExceptionInformation())))
            {
                // Frame abandoned. The next frame's BeginFrame() re-baselines
                // bone state; gaze simply does not update for one frame.
            }
        }
#else
        static void RunGazeFrameSeh(RE::Actor *a_actor, float a_delta)
        {
            try
            {
                RunGazeFrameBody(a_actor, a_delta);
            }
            catch (...)
            {
                // C++-only fallback on non-Windows builds.
            }
        }
#endif

        template <class Tag>
        struct ActorUpdateHook
        {
            static void Hook(RE::Actor *a_actor, float a_delta)
            {
                // Advance the player's pristine baseline if on the main thread.
                if constexpr (std::is_same_v<Tag, PlayerTag>)
                {
                    if (a_actor)
                    {
                        EyeAimConstraint::WithdrawActor(a_actor->GetFormID());
                    }
                }

                // Advance the game's own state. This deliberately remains outside
                // our exception guard: it is Skyrim's code, not ours to swallow.
                _original(a_actor, a_delta);

                // First boundary diagnostic after Skyrim's original virtual call.
                // If this never appears, the installed slot is not being dispatched
                // for the active actor type/runtime.
                static std::atomic<uint32_t> s_postUpdateReports{0};
                const auto report = s_postUpdateReports.fetch_add(1, std::memory_order_relaxed);
                if (report == 0)
                {
                    logger::info("[TrueGaze] Actor update hook invoked: form={:08X} delta={:.4f} thread={}",
                                 a_actor ? a_actor->GetFormID() : 0u, a_delta, GetCurrentThreadId());
                }

                // Everything that follows is ours. A defect in the kinematics engine must
                // degrade gaze, not take the game down with it — for ANY failure class
                // (C++ exceptions AND SEH access violations; see RunGazeFrameSeh).
                if constexpr (std::is_same_v<Tag, PlayerTag>)
                {
                    RunGazeFrameSeh(a_actor, a_delta);
                    return;
                }
                else
                {
                    // In Skyrim SE/AE, Actor::Update and Character::Update for NPCs are dispatched
                    // across Havok animation worker threads (where delta is 0.0f). Mutating the
                    // NetImmerse scene graph or querying singletons on worker threads causes race conditions.
                    // All NPC gaze updates are driven cleanly and deterministically on the main
                    // game thread in PlayerTag via AnimationHook::TickAllActors.
                    return;
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
            static inline int _failuresReported{0};
        };

        using ActorHook = ActorUpdateHook<ActorTag>;
        using CharacterHook = ActorUpdateHook<CharacterTag>;
        using PlayerHook = ActorUpdateHook<PlayerTag>;

        template <class Hook>
        void InstallActorUpdateHook(const REL::VariantID &vtable, const char *name)
        {
            REL::Relocation<std::uintptr_t> table{vtable};
            const std::size_t updateSlot = REL::Module::IsVR() ? 0xAF : 0xAD;
            Hook::_original = table.write_vfunc(updateSlot, Hook::Hook);
            logger::info("[TrueGaze] Gaze driver installed on {}::Update (slot 0xAD / VR slot 0xAF, active: 0x{:02X}).", name, updateSlot);
        }

        void TickActorList(RE::BSTArray<RE::ActorHandle> &list,
                           bool allowCreatures,
                           float deltaSeconds,
                           GazeEngine &engine) noexcept
        {
            const auto count = list.size();
            for (uint32_t i = 0; i < count && i < list.size(); ++i)
            {
                auto &handle = list[i];
                auto actorPtr = handle.get();
                if (!actorPtr)
                {
                    continue;
                }

                auto *actor = actorPtr.get();
                if (!actor || actor->IsPlayerRef())
                {
                    continue;
                }

                if (!allowCreatures && !actor->IsHumanoid())
                {
                    continue;
                }

                if (!AnimationHook::IsActorEligibleForGaze(actor))
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
        static bool installed = false;
        if (installed)
        {
            return;
        }

        logger::info("[TrueGaze] Installing per-actor post-update gaze drivers...");

        // Each concrete class owns a distinct vtable even when its slot points to
        // the inherited Actor::Update implementation. Patching VTABLE_Actor alone
        // does not affect Character or PlayerCharacter instances.
        InstallActorUpdateHook<ActorHook>(RE::VTABLE_Actor[0], "Actor");
        InstallActorUpdateHook<CharacterHook>(RE::VTABLE_Character[0], "Character");
        InstallActorUpdateHook<PlayerHook>(RE::VTABLE_PlayerCharacter[0], "PlayerCharacter");
        installed = true;

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

        static std::atomic<uint32_t> s_tickReports{0};
        if (s_tickReports.fetch_add(1, std::memory_order_relaxed) == 0)
        {
            logger::info("[TrueGaze] TickAllActors active on game thread: highActors={} middleHighActors={} delta={:.4f}",
                         processLists->highActorHandles.size(),
                         processLists->middleHighActorHandles.size(),
                         deltaSeconds);
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
        // Retained for API compatibility. Gaze is actor-owned now: each actor's
        // previous pose is restored immediately before its next Actor::Update,
        // not at frame end where restoration would occur before rendering.
#endif
    }

#if __has_include(<RE/Skyrim.h>)
    bool AnimationHook::IsActorEligibleForGaze(RE::Actor *actor) noexcept
    {
        if (!actor)
        {
            return false;
        }

        // 3D must be loaded; without it there is no skeleton to rotate.
        if (!actor->Get3D())
        {
            return false;
        }

        // Only living, enabled actors have a meaningful head pose.
        // Uses the engine's canonical virtual function (SE/AE 0x99, VR 0x9A) and form flags,
        // which are completely version-independent across SE, AE, and VR runtimes.
        if (actor->IsDead() || actor->IsDisabled() || actor->IsDeleted())
        {
            return false;
        }

        return true;
    }
#endif

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

        return IsActorEligibleForGaze(form->As<RE::Actor>());
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
