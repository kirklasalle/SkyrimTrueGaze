#pragma once

// ---------------------------------------------------------------------------
// ConfigParse.hpp — SDK-free INI value parsing helpers.
//
// AUDIT 2026-10-15 F-04: std::strtof accepts "nan", "inf", "infinity" and
// overflowing literals (-> HUGE_VALF). NaN compares false against every range
// bound, so it slipped straight through ConfigManager::Sanitise() into the
// kinematics. Kept free of Windows / SKSE headers so the standalone
// ValidationTests can exercise it directly.
// ---------------------------------------------------------------------------

#include <cmath>
#include <cstdlib>

namespace TrueGaze::Engine::ConfigParse
{
    enum class FloatParse
    {
        Ok,        ///< `out` holds a finite value.
        Empty,     ///< No number at the start of the text; `out` untouched.
        NonFinite, ///< Parsed, but NaN / +-inf / overflow; `out` untouched.
    };

    /// Parse a leading float from `text` (strtof semantics: leading whitespace
    /// skipped, trailing junk ignored). Writes `out` only on FloatParse::Ok.
    [[nodiscard]] inline FloatParse TryParseFiniteFloat(const char *text, float &out) noexcept
    {
        if (!text)
        {
            return FloatParse::Empty;
        }
        char *end = nullptr;
        const float value = std::strtof(text, &end);
        if (end == text)
        {
            return FloatParse::Empty;
        }
        if (!std::isfinite(value))
        {
            return FloatParse::NonFinite;
        }
        out = value;
        return FloatParse::Ok;
    }
} // namespace TrueGaze::Engine::ConfigParse
