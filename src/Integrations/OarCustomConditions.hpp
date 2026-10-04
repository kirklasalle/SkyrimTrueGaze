#pragma once

// SKSE-only. Compiled into the plugin, never into the standalone test targets.

namespace TrueGaze::Integrations::OarCustomConditions
{
    /// @brief Requests OAR's Conditions API (interface V3) and registers
    /// TrueGaze_IsMode, TrueGaze_IsMutualGaze and TrueGaze_GetGazeRegion.
    ///
    /// Logs the result for each condition. Must run during SKSE kPostLoad, with
    /// OpenAnimationReplacer.dll already confirmed loaded by the caller.
    /// @return true only if OAR accepted all three conditions.
    bool RegisterAll();
} // namespace TrueGaze::Integrations::OarCustomConditions
