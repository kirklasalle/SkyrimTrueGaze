#pragma once

#include "PCH.h"
#include <cstdint>

namespace TrueGaze::Integrations
{

    /// @brief Synchronizes eyelid morph targets with saccades (Saccadic Suppression Blinking).
    /// Interlinks with Expressive Facegen Morphs (EFM) / Expressive Facial Animation (EFA) rigs.
    class EfmBlinkController
    {
    public:
        struct BlinkState
        {
            bool isBlinking{false};
            float blinkElapsedSec{0.0f};
            float blinkTotalDurationSec{0.12f}; // ~120ms micro-blink
            float eyelidCloseWeight{0.0f};      // [0.0f = Open, 1.0f = Fully Closed]
        };

        /// @brief Checks if a major saccade (> 20 degrees) should trigger an organic micro-blink.
        static void OnSaccadeTriggered(BlinkState &state, float saccadeAmplitudeDeg) noexcept
        {
            if (saccadeAmplitudeDeg >= 20.0f && !state.isBlinking)
            {
                state.isBlinking = true;
                state.blinkElapsedSec = 0.0f;
                state.eyelidCloseWeight = 0.0f;
            }
        }

        /// @brief Steps the eyelid blink animation curves and applies them to the character's face morphs.
        static void Update(BlinkState &state, float deltaSeconds) noexcept
        {
            if (!state.isBlinking)
                return;

            state.blinkElapsedSec += deltaSeconds;
            if (state.blinkElapsedSec >= state.blinkTotalDurationSec)
            {
                state.isBlinking = false;
                state.eyelidCloseWeight = 0.0f;
                return;
            }

            // Parabolic dip: 0 -> 1 -> 0
            float t = state.blinkElapsedSec / state.blinkTotalDurationSec;
            state.eyelidCloseWeight = 4.0f * t * (1.0f - t);
        }

        /// @brief Applies eyelid morph weights and eye-direction morphs (EFA / EFM / Vanilla)
        /// to an actor's FaceGen modifier keyframes.
        ///
        /// On vanilla humanoid rigs the eyeballs have NO bones — they are driven ONLY by
        /// the FaceGen Look* modifier morphs. This function is therefore the entire "the
        /// eyes move" path for the overwhelming majority of NPCs (Helgen cart included).
        ///
        /// @param eyeYawDeg/eyePitchDeg  Solved ocular deflection (head-relative), degrees.
        /// @param fullScaleDeg           Deflection that maps to a fully-driven morph.
        /// @param gain                   Visible gain multiplier (result clamped to 1.0).
        static void ApplyGazeMorphs(uint32_t actorFormId, float eyelidWeight,
                                    float eyeYawDeg, float eyePitchDeg,
                                    float fullScaleDeg = 22.0f, float gain = 1.6f) noexcept;

        /// @brief Legacy wrapper for eyelid blink morph application.
        static void ApplyMorphs(uint32_t actorFormId, float eyelidWeight) noexcept
        {
            ApplyGazeMorphs(actorFormId, eyelidWeight, 0.0f, 0.0f);
        }
    };

} // namespace TrueGaze::Integrations
