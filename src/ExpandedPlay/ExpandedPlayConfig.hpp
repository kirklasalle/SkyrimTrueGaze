#pragma once

#include "ExpandedPlay/AwakeningRitual.hpp"
#include "ExpandedPlay/ExpandedPlayTypes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace TrueGaze::ExpandedPlay
{
    /// @brief Runtime tuning snapshot for the optional Expanded Play gameplay layer.
    /// Defaults to disabled (`enabled = false`) so TrueGaze preserves exact classic
    /// visual-only parity unless explicitly opted into.
    struct ExpandedPlayConfig
    {
        bool enabled{false};
        UnlockMode unlockMode{UnlockMode::Ritual};
        DemeanorMode demeanorMode{DemeanorMode::Hybrid};
        Demeanor activeDemeanor{Demeanor::Courteous};

        bool moduleSocialReactions{true};
        bool moduleRelationshipMemory{true};
        bool moduleBookStudy{true};
        bool moduleMatureReactions{false};
        bool modulePresenceProgression{true};
        bool enableKindredSight{true};

        bool enableReticle{true};
        float reticleOpacity{0.22f};
        float reticleMagnetismStrength{0.35f};
        int colorBlindMode{0};
        bool immersiveHideReticle{false};

        bool enableEyeContactInitiate{true};
        bool enableImpressionWhispers{true};
        float thirdPersonEfficacy{0.80f};
        bool emitAiNpcBridgeEvents{true};

        /// @brief Guards against NaN/Inf and out-of-range configuration values.
        void Sanitise() noexcept
        {
            const auto sanitiseFloat = [](float value, float lo, float hi, float fallback) noexcept
            {
                if (!std::isfinite(value))
                {
                    return fallback;
                }
                return std::clamp(value, lo, hi);
            };

            if (static_cast<uint8_t>(unlockMode) > static_cast<uint8_t>(UnlockMode::Sandbox))
            {
                unlockMode = UnlockMode::Ritual;
            }
            if (static_cast<uint8_t>(demeanorMode) > static_cast<uint8_t>(DemeanorMode::Hybrid))
            {
                demeanorMode = DemeanorMode::Hybrid;
            }
            if (static_cast<uint8_t>(activeDemeanor) >= static_cast<uint8_t>(Demeanor::Count))
            {
                activeDemeanor = Demeanor::Courteous;
            }

            reticleOpacity = sanitiseFloat(reticleOpacity, 0.0f, 1.0f, 0.22f);
            reticleMagnetismStrength =
                sanitiseFloat(reticleMagnetismStrength, 0.0f, 1.0f, 0.35f);
            colorBlindMode = std::clamp(colorBlindMode, 0, 3);
            thirdPersonEfficacy = sanitiseFloat(thirdPersonEfficacy, 0.50f, 1.00f, 0.80f);
        }
    };
} // namespace TrueGaze::ExpandedPlay
