#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace TrueGaze::ExpandedPlay
{
    /// @brief Why a player look was (or was not) perceived by an observer.
    enum class PerceptionReason : uint8_t
    {
        Perceived     = 0,
        Unaware       = 1, // dead, unconscious, ragdolled, asleep
        InCombat      = 2,
        OutOfRange    = 3,
        NoLineOfSight = 4,
        BehindObserver = 5,
        TooDark       = 6,
        InvalidInput  = 7
    };

    [[nodiscard]] constexpr const char* PerceptionReasonName(PerceptionReason r) noexcept
    {
        switch (r)
        {
        case PerceptionReason::Perceived: return "Perceived";
        case PerceptionReason::Unaware: return "Unaware";
        case PerceptionReason::InCombat: return "InCombat";
        case PerceptionReason::OutOfRange: return "OutOfRange";
        case PerceptionReason::NoLineOfSight: return "NoLineOfSight";
        case PerceptionReason::BehindObserver: return "BehindObserver";
        case PerceptionReason::TooDark: return "TooDark";
        case PerceptionReason::InvalidInput: return "InvalidInput";
        default: return "Unknown";
        }
    }

    /// @brief Game-side facts sampled for one observer; filled by the SKSE layer (WP-6).
    struct PerceptionInput
    {
        bool observerAlive{true};
        bool observerConscious{true};
        bool observerSleeping{false};
        bool observerRagdolled{false};
        bool observerInCombat{false};
        bool observerInScene{false};

        float distanceMeters{0.0f};
        bool hasLineOfSight{true};

        // Unit-ish vectors; normalised internally.
        float observerForwardX{0.0f}, observerForwardY{1.0f}, observerForwardZ{0.0f};
        float dirToPlayerX{0.0f}, dirToPlayerY{1.0f}, dirToPlayerZ{0.0f};

        float lightLevel01{1.0f};      // 0 = pitch dark, 1 = full daylight
        bool observerHasNightEye{false}; // Khajiit / vampire / Dunmer in dim rooms
        bool playerFullFaceHelmet{false};
        bool playerHooded{false};
    };

    struct PerceptionResult
    {
        bool perceived{false};
        bool regionSpecific{false};  // false beyond 8 m: presence only, no face-region read
        float intensity{0.0f};       // [0..1] multiplier for interpretation strength
        float facialBondScale{1.0f}; // scales Warmth / Attraction / Recognition only
        PerceptionReason reason{PerceptionReason::InvalidInput};
    };

    /// @brief Pure perception gate: can the observer legitimately see the player's look?
    class PerceptionGate
    {
    public:
        static constexpr float kRegionRangeMeters   = 8.0f;
        static constexpr float kPresenceRangeMeters = 15.0f;
        static constexpr float kHemisphereDot       = -0.15f; // ~197 deg awareness arc
        static constexpr float kSceneMultiplier     = 0.50f;
        static constexpr float kHelmetBondScale     = 0.35f;
        static constexpr float kHoodBondScale       = 0.70f;
        static constexpr float kNightEyeLightFloor  = 0.85f;
        static constexpr float kMinLightFactor      = 0.25f;

        [[nodiscard]] static PerceptionResult Evaluate(const PerceptionInput& in) noexcept
        {
            PerceptionResult out{};

            if (!std::isfinite(in.distanceMeters) || in.distanceMeters < 0.0f)
            {
                out.reason = PerceptionReason::InvalidInput;
                return out;
            }

            if (!in.observerAlive || !in.observerConscious || in.observerSleeping ||
                in.observerRagdolled)
            {
                out.reason = PerceptionReason::Unaware;
                return out;
            }

            if (in.observerInCombat)
            {
                out.reason = PerceptionReason::InCombat;
                return out;
            }

            if (in.distanceMeters > kPresenceRangeMeters)
            {
                out.reason = PerceptionReason::OutOfRange;
                return out;
            }

            if (!in.hasLineOfSight)
            {
                out.reason = PerceptionReason::NoLineOfSight;
                return out;
            }

            float dot = 0.0f;
            if (!NormalisedDot(in.observerForwardX, in.observerForwardY, in.observerForwardZ,
                               in.dirToPlayerX, in.dirToPlayerY, in.dirToPlayerZ, dot))
            {
                out.reason = PerceptionReason::InvalidInput;
                return out;
            }
            if (dot <= kHemisphereDot)
            {
                out.reason = PerceptionReason::BehindObserver;
                return out;
            }

            float light = std::isfinite(in.lightLevel01) ? std::clamp(in.lightLevel01, 0.0f, 1.0f)
                                                         : 1.0f;
            if (in.observerHasNightEye)
            {
                light = std::max(light, kNightEyeLightFloor);
            }
            const float lightFactor = std::max(kMinLightFactor, light);

            // Facing factor: full strength inside ~60 deg, tapering to 0.4 at the arc edge.
            const float facing = std::clamp(0.4f + 0.6f * ((dot - kHemisphereDot) /
                                                           (0.5f - kHemisphereDot)),
                                            0.4f, 1.0f);

            // Distance falloff: full inside 3 m, tapering to 0.5 at 15 m.
            const float distanceFactor =
                in.distanceMeters <= 3.0f
                    ? 1.0f
                    : std::clamp(1.0f - 0.5f * ((in.distanceMeters - 3.0f) /
                                                (kPresenceRangeMeters - 3.0f)),
                                 0.5f, 1.0f);

            float intensity = lightFactor * facing * distanceFactor;
            if (in.observerInScene)
            {
                intensity *= kSceneMultiplier;
            }

            out.perceived = true;
            out.regionSpecific = in.distanceMeters <= kRegionRangeMeters;
            out.intensity = std::clamp(intensity, 0.0f, 1.0f);
            out.facialBondScale = in.playerFullFaceHelmet ? kHelmetBondScale
                                  : in.playerHooded       ? kHoodBondScale
                                                          : 1.0f;
            out.reason = PerceptionReason::Perceived;
            return out;
        }

    private:
        [[nodiscard]] static bool NormalisedDot(float ax, float ay, float az, float bx, float by,
                                                float bz, float& out) noexcept
        {
            const float la = std::sqrt(ax * ax + ay * ay + az * az);
            const float lb = std::sqrt(bx * bx + by * by + bz * bz);
            if (!std::isfinite(la) || !std::isfinite(lb) || la < 1e-6f || lb < 1e-6f)
            {
                return false;
            }
            out = (ax * bx + ay * by + az * bz) / (la * lb);
            if (!std::isfinite(out))
            {
                return false;
            }
            out = std::clamp(out, -1.0f, 1.0f);
            return true;
        }
    };
} // namespace TrueGaze::ExpandedPlay
