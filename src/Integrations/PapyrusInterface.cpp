#include "PapyrusInterface.hpp"
#include "Engine/GazeEngine.hpp"
#include "Engine/ConfigManager.hpp"
#include "Integrations/OarConditions.hpp"

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
// IVirtualMachine::RegisterFunction is a template defined in NativeFunction.h.
// Without this include the compiler sees only the declaration and reports
// "use of undefined type 'RE::NativeFunction<...>'".
#include <RE/N/NativeFunction.h>
#endif

namespace TrueGaze::Integrations
{

#if __has_include(<RE/Skyrim.h>)

    namespace
    {

        /// Resolve a Papyrus Actor argument to live simulation state.
        Engine::ActorGazeRuntime *StateFor(RE::Actor *a_actor) noexcept
        {
            if (!a_actor)
            {
                return nullptr;
            }

            const uint32_t formId = a_actor->GetFormID();
            if (formId == 0)
            {
                return nullptr;
            }

            auto *state = Engine::GazeEngine::Get().FindActor(formId);
            if (!state || !state->initialised)
            {
                return nullptr;
            }
            return state;
        }

        // ---------------------------------------------------------------------------
        // Bindings
        //
        // These are FREE functions, not static member functions. CommonLibSSE's
        // NativeFunction specialisation is constrained on `is_valid_short_sig_v`, which
        // accepts a plain function pointer. A pointer-to-static-member-function has a
        // different type and fails to match, producing
        // "use of undefined type 'RE::NativeFunction<...>'".
        // ---------------------------------------------------------------------------

        std::int32_t GetVersion(RE::StaticFunctionTag *)
        {
            return PapyrusInterface::kScriptApiVersion;
        }

        bool IsHcepConnected(RE::StaticFunctionTag *)
        {
            return Engine::GazeEngine::Get().IsBridgeConnected();
        }

        bool IsMutualGaze(RE::StaticFunctionTag *, RE::Actor *a_actor,
                          float a_durationThreshold)
        {
            auto *state = StateFor(a_actor);
            return state && state->mutualGazeHoldSec >= a_durationThreshold;
        }

        std::int32_t GetActorMode(RE::StaticFunctionTag *, RE::Actor *a_actor)
        {
            auto *state = StateFor(a_actor);
            return state ? static_cast<std::int32_t>(state->hcepMode) : -1;
        }

        std::int32_t GetGazeRegion(RE::StaticFunctionTag *, RE::Actor *a_actor)
        {
            auto *state = StateFor(a_actor);
            return state ? static_cast<std::int32_t>(state->gazeRegion) : -1;
        }

        RE::TESObjectREFR *GetGazeTarget(RE::StaticFunctionTag *, RE::Actor *a_actor)
        {
            auto *state = StateFor(a_actor);
            if (!state || state->trackedTargetFormId == 0)
            {
                return nullptr;
            }

            // Return a real reference so Papyrus callers can use it directly. The script
            // signature declares ObjectReference; an Actor is also an ObjectReference, so
            // this is valid for the common case where the target is the player or an NPC.
            auto *form = RE::TESForm::LookupByID(state->trackedTargetFormId);
            return form ? form->As<RE::TESObjectREFR>() : nullptr;
        }

        void OverrideActorMode(RE::StaticFunctionTag *, RE::Actor *a_actor,
                               std::int32_t a_mode, float a_durationSec)
        {
            (void)a_durationSec;

            auto *state = StateFor(a_actor);
            if (!state || a_mode < 0 || a_mode > 4)
            {
                return;
            }

            state->hcepMode = static_cast<uint8_t>(a_mode);
            OarConditions::PublishActorState(a_actor->GetFormID(), state->hcepMode,
                                             state->gazeRegion, state->mutualGazeHoldSec);
        }

        bool IsSaccadeActive(RE::StaticFunctionTag *, RE::Actor *a_actor)
        {
            auto *state = StateFor(a_actor);
            return state && state->saccade.isBallistic;
        }

        float GetGazeYaw(RE::StaticFunctionTag *, RE::Actor *a_actor)
        {
            auto *state = StateFor(a_actor);
            return state ? state->lastYawDeg : 0.0f;
        }

        float GetGazePitch(RE::StaticFunctionTag *, RE::Actor *a_actor)
        {
            auto *state = StateFor(a_actor);
            return state ? state->lastPitchDeg : 0.0f;
        }

    } // namespace

    // ---------------------------------------------------------------------------
    // Registration
    // ---------------------------------------------------------------------------

    bool PapyrusInterface::Register(RE::BSScript::IVirtualMachine *a_vm) noexcept
    {
        if (!a_vm)
        {
            logger::error("[TrueGaze] Papyrus registration failed: null VM.");
            return false;
        }

        // The script name must match `scriptName TrueGaze` in TrueGaze.psc, and every
        // function name must match its declaration there exactly.
        constexpr std::string_view kScript = "TrueGaze"sv;

        // The final argument controls whether the function may be called from a
        // tasklet. These are all cheap reads of in-memory state, so allowing it is
        // safe and lets quest scripts query gaze without a latent call.
        constexpr bool kTasklet = true;

        a_vm->RegisterFunction("GetVersion"sv, kScript, GetVersion, kTasklet);
        a_vm->RegisterFunction("IsHcepConnected"sv, kScript, IsHcepConnected, kTasklet);
        a_vm->RegisterFunction("IsMutualGaze"sv, kScript, IsMutualGaze, kTasklet);
        a_vm->RegisterFunction("GetActorMode"sv, kScript, GetActorMode, kTasklet);
        a_vm->RegisterFunction("GetGazeRegion"sv, kScript, GetGazeRegion, kTasklet);
        a_vm->RegisterFunction("GetGazeTarget"sv, kScript, GetGazeTarget, kTasklet);
        a_vm->RegisterFunction("OverrideActorMode"sv, kScript, OverrideActorMode, kTasklet);
        a_vm->RegisterFunction("IsSaccadeActive"sv, kScript, IsSaccadeActive, kTasklet);
        a_vm->RegisterFunction("GetGazeYaw"sv, kScript, GetGazeYaw, kTasklet);
        a_vm->RegisterFunction("GetGazePitch"sv, kScript, GetGazePitch, kTasklet);

        logger::info("[TrueGaze] Registered 10 Papyrus functions on script '{}'.", kScript);
        return true;
    }

    bool PapyrusInterface::RegisterFunctions() noexcept
    {
        auto *papyrus = SKSE::GetPapyrusInterface();
        if (!papyrus)
        {
            logger::error("[TrueGaze] Papyrus interface unavailable; script bindings not registered.");
            return false;
        }

        if (!papyrus->Register(Register))
        {
            logger::error("[TrueGaze] Papyrus rejected the TrueGaze function registration.");
            return false;
        }

        return true;
    }

#else // !__has_include(<RE/Skyrim.h>)

    bool PapyrusInterface::Register(RE::BSScript::IVirtualMachine *) noexcept { return false; }

    bool PapyrusInterface::RegisterFunctions() noexcept
    {
        logger::info("[TrueGaze] Standalone mode: Papyrus registration skipped.");
        return false;
    }

#endif

} // namespace TrueGaze::Integrations