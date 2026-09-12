#pragma once

#include "PCH.h"

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#endif

namespace TrueGaze::Integrations
{

    /// @brief Binds the TrueGaze native functions declared in TrueGaze.psc.
    ///
    /// ## Why this exists
    ///
    /// `skyrim/scripts/source/TrueGaze.psc` declared six `global native` functions,
    /// but no `SKSE::GetPapyrusInterface()->Register(...)` call existed anywhere, and
    /// the declared signatures did not match the exported symbols in TrueGazeAPI.cpp.
    /// Any mod author calling them got a Papyrus VM error. See
    /// docs/AUDIT_REPORT_2026-09-11.md finding C-4.
    ///
    /// ## Signature contract
    ///
    /// The names and parameter lists below must match TrueGaze.psc exactly. Papyrus
    /// `int` is `std::int32_t`, `float` is `float`, `bool` is `bool`, `Actor` is
    /// `RE::Actor*`, and a returned Form is `RE::TESForm*` (an implicit return, i.e.
    /// the object is registered as the function's result, not passed as an argument).
    class PapyrusInterface
    {
    public:
        /// @brief Registers all TrueGaze Papyrus functions with the script VM.
        /// @return true if every binding was accepted.
        static bool Register(RE::BSScript::IVirtualMachine *a_vm) noexcept;

        /// @brief Obtains the Papyrus interface and registers. Call from SKSEPluginLoad.
        /// @return true on success; false (logged) if the VM is unavailable.
        static bool RegisterFunctions() noexcept;

        // Script API version. Bump when a signature changes so Papyrus callers can
        // detect an out-of-date plugin rather than faulting at the call site.
        static constexpr std::int32_t kScriptApiVersion = 1;
    };

} // namespace TrueGaze::Integrations