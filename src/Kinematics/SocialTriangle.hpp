#pragma once

#include <cstdint>
#include <random>
#include <algorithm>
#include <cmath>

namespace TrueGaze::Kinematics
{

    /// @brief Social Triangle Scanpath Generator for HCEP AFFECT Mode.
    /// Based on Argyle, Ingham, Alkema & McCallin (1973): during empathetic dialogue, humans cycle
    /// gaze between three facial vertices: Left Eye -> Right Eye -> Mouth.
    ///
    /// Enhanced with the full HCEP Enhanced Diagram (hcep-02):
    /// - Core Social Triangle: LeftEye ↔ RightEye ↔ Mouth (AFFECT / conversation)
    /// - Third-Eye / Forehead fixation point (SPIRIT / deep contact)
    /// - Chest / Heart / Sternum empathic resonance point (HEART mode)
    /// - Cognitive Gaze Aversion (CGA) peripheral regions (THINK mode):
    ///   Upper-Left (positivity/hope), Upper-Right (memory/constructive thought),
    ///   Lower-Left (tiredness/negativity), Lower-Right (shyness/fear/deception)
    class SocialTriangle
    {
    public:
        /// Extended vertex set covering the full HCEP Enhanced Diagram.
        enum class Vertex : uint8_t
        {
            LeftEye = 0,
            RightEye = 1,
            Mouth = 2,
            ThirdEye = 3,           // Forehead / spiritual focus (between + above eyes)
            Chest = 4,              // Heart / sternum empathic resonance
            UpperLeftAversion = 5,  // CGA: positivity, happiness, hope
            UpperRightAversion = 6, // CGA: search for memories, constructive thought
            LowerLeftAversion = 7,  // CGA: tiredness, negativity, sadness
            LowerRightAversion = 8  // CGA: shyness, fear, deception
        };

        struct TriangleState
        {
            Vertex currentVertex{Vertex::LeftEye};
            float fixationTimerSec{0.0f};
            float fixationDurationSec{0.35f}; // Typical fixation ~350ms

            /// CALM/COMBAT SPEED MODEL (Kirk directive, September 26 2026):
            /// multiplier applied to every fixation-duration draw. 1.0 =
            /// biological cadence (combat); 2.0 = calm baseline — fixations
            /// last twice as long, so the eyes jump between regions HALF as
            /// often. Set by GazeEngine each tick from the frame's speed model.
            float cadenceScale{1.0f};

            /// EYE-TO-EYE DOMINANCE (Kirk directive, September 26 2026):
            /// "looking into character eyes is important and must last longer
            /// before shifting." Multiplier applied to fixation durations WHEN
            /// the current vertex is an EYE (LeftEye/RightEye) — holds on the
            /// eyes outlast every other region. GazeEngine sets 2.5 baseline
            /// and 4.0 during dialogue (eye contact is the dialogue contract).
            float eyeDwellScale{1.0f};

            float vertexOffsetXDeg{-1.8f}; // Offset relative to face center
            float vertexOffsetYDeg{1.0f};

            /// RNG state for organic variability, seeded per-actor from FormID.
            std::mt19937 rng{42};
            bool rngSeeded{false};
        };

        /// @brief Seeds the triangle RNG for a specific actor (call once at init).
        static void SeedRng(TriangleState &state, uint32_t actorFormId) noexcept
        {
            state.rng.seed(actorFormId != 0u ? actorFormId : 0x9E3779B9u);
            state.rngSeeded = true;
        }

        /// @brief Weighted-random next triangle vertex.
        ///
        /// A perfectly repeating orbit (LeftEye → RightEye → Mouth → LeftEye) reads
        /// as robotic within minutes. Real listeners do not cycle facial features
        /// in order: they favour one eye, jump laterally, dip to the mouth, and
        /// occasionally re-fixate the same point. This helper blends between the
        /// classic deterministic cycle (r = 0) and that organic wandering (r = 1).
        ///
        /// @param r Path randomness 0..1 (from [Social] fTrianglePathRandomness).
        static Vertex NextTriangleVertex(TriangleState &state, Vertex current, float r) noexcept
        {
            r = std::clamp(r, 0.0f, 1.0f);

            Vertex canonical = Vertex::LeftEye;
            Vertex other = Vertex::Mouth;

            switch (current)
            {
            case Vertex::LeftEye:
                canonical = Vertex::RightEye;
                other = Vertex::Mouth;
                break;
            case Vertex::RightEye:
                canonical = Vertex::Mouth;
                other = Vertex::LeftEye;
                break;
            case Vertex::Mouth:
            default:
                canonical = Vertex::LeftEye;
                other = Vertex::RightEye;
                break;
            }

            if (r <= 0.0f)
            {
                return canonical; // classic fixed orbit
            }

            // Blend: at r = 1 the canonical step still leads strongly (0.60)
            // because humans DO favour eye-to-eye transitions — EYE-LOCK BIAS
            // (Kirk fine-tuning, 2026-09-26: "the priority to focus and lock
            // onto the player/NPC/creature eyes should happen more often").
            // Lateral moves (0.25) and same-point re-fixations (0.15) still
            // break every predictable loop, but the eyes win more often.
            const float pCanonical = (1.0f - r) + r * 0.60f;
            const float pOther = r * 0.25f;

            const float roll = std::uniform_real_distribution<float>(0.0f, 1.0f)(state.rng);
            if (roll < pCanonical)
            {
                return canonical;
            }
            if (roll < pCanonical + pOther)
            {
                return other;
            }
            return current; // re-fixation: same feature, freshly scattered landing
        }

        /// @brief Character-profile-weighted vertex selection.
        ///
        /// The Character Gaze Profile projects temperament onto the HCEP-02 diagram
        /// as per-vertex weights (1.0 = neutral). This variant multiplies each
        /// candidate's probability by its weight, then normalises — so a shy NPC's
        /// LowerRight aversion region draws more visits, a lover's Chest point
        /// draws more, etc. Weights of exactly 1.0 reproduce the unweighted
        /// distribution (parity contract).
        ///
        /// @param weights Per-vertex weights indexed by Vertex (0..8), all 1.0 = neutral.
        static Vertex NextWeightedVertex(TriangleState &state, Vertex current,
                                         const float *weights /*[9]*/) noexcept
        {
            if (!weights)
            {
                return NextTriangleVertex(state, current, 0.6f);
            }

            // Candidate set: the three core triangle vertices plus the extended
            // diagram points reachable from the current position. Keeping the
            // candidate set small preserves the human eye-to-eye bias while
            // letting weights shift the balance.
            static constexpr Vertex kCandidates[] = {
                Vertex::LeftEye, Vertex::RightEye, Vertex::Mouth,
                Vertex::ThirdEye, Vertex::Chest,
                Vertex::UpperLeftAversion, Vertex::UpperRightAversion,
                Vertex::LowerLeftAversion, Vertex::LowerRightAversion};
            constexpr int kCount = 9;

            float total = 0.0f;
            float w[kCount];
            for (int i = 0; i < kCount; ++i)
            {
                // The current vertex gets a reduced weight (re-fixation is possible
                // but not favoured), matching the unweighted distribution's shape.
                const float base = (kCandidates[i] == current) ? 0.20f : 1.0f;

                // EYE-LOCK BIAS (Kirk fine-tuning, 2026-09-26): eye vertices are
                // structurally favoured — 2.0x weight — so gaze locks onto the
                // target's eyes more often than any other region, independent of
                // the character profile's weights (which multiply on top).
                const bool isEye = kCandidates[i] == Vertex::LeftEye ||
                                   kCandidates[i] == Vertex::RightEye;
                const float eyeBias = isEye ? 2.0f : 1.0f;

                w[i] = base * eyeBias * std::max(0.01f, weights[i]);
                total += w[i];
            }

            if (total <= 0.0f)
            {
                return NextTriangleVertex(state, current, 0.6f);
            }

            float roll = std::uniform_real_distribution<float>(0.0f, total)(state.rng);
            for (int i = 0; i < kCount; ++i)
            {
                roll -= w[i];
                if (roll <= 0.0f)
                {
                    return kCandidates[i];
                }
            }
            return Vertex::LeftEye;
        }

        /// @brief Steps the core social triangle with an organic, non-repeating path.
        /// Active during AFFECT mode and as the always-on baseline eye scanning.
        ///
        /// @param pathRandomness 0 = classic fixed orbit, 1 = free wandering
        ///                       ([Social] fTrianglePathRandomness, default 0.6).
        static void Update(TriangleState &state, float deltaSeconds,
                           float faceDistanceMeters = 1.5f,
                           float pathRandomness = 0.6f) noexcept
        {
            if (deltaSeconds <= 0.0f)
                return;

            state.fixationTimerSec += deltaSeconds;

            if (state.fixationTimerSec >= state.fixationDurationSec)
            {
                state.fixationTimerSec = 0.0f;

                // Random fixation duration for organic variability (200-550ms)
                std::uniform_real_distribution<float> durDist(0.20f, 0.55f);
                state.fixationDurationSec = durDist(state.rng) * state.cadenceScale;

                // Weighted-random next vertex: favours eye-to-eye transitions the way
                // humans do, but lateral jumps and re-fixations destroy the fixed orbit.
                state.currentVertex = NextTriangleVertex(state, state.currentVertex, pathRandomness);

                ComputeVertexOffset(state, faceDistanceMeters);
                ApplyEyeDwell(state);
            }
        }

        /// @brief EYE-TO-EYE DOMINANCE: after a vertex lands on an eye, stretch
        /// its fixation duration by eyeDwellScale. A hold on the eyes is the
        /// socially meaningful state — it must outlast mouth/forehead/chest
        /// visits by a wide margin, and dominate during dialogue.
        static void ApplyEyeDwell(TriangleState &state) noexcept
        {
            if (state.currentVertex == Vertex::LeftEye ||
                state.currentVertex == Vertex::RightEye)
            {
                state.fixationDurationSec *= std::max(1.0f, state.eyeDwellScale);
            }
        }

        /// @brief Steps the core social triangle with profile-weighted vertex selection.
        /// Identical to Update() except the next vertex is drawn from the
        /// character-profile weights (see NextWeightedVertex). Weights of 1.0
        /// reproduce the organic distribution.
        static void UpdateWeighted(TriangleState &state, float deltaSeconds,
                                   float faceDistanceMeters,
                                   float /*pathRandomness*/,
                                   const float *weights /*[9]*/) noexcept
        {
            if (deltaSeconds <= 0.0f)
                return;

            state.fixationTimerSec += deltaSeconds;

            if (state.fixationTimerSec >= state.fixationDurationSec)
            {
                state.fixationTimerSec = 0.0f;

                std::uniform_real_distribution<float> durDist(0.20f, 0.55f);
                state.fixationDurationSec = durDist(state.rng) * state.cadenceScale;

                state.currentVertex = NextWeightedVertex(state, state.currentVertex, weights);
                ComputeVertexOffset(state, faceDistanceMeters);
                ApplyEyeDwell(state);
            }
        }

        /// @brief Steps the extended HCEP diagram scanpath including Third-Eye and Chest.
        /// Active during SPIRIT and HEART modes. The scanpath weaves between the social
        /// triangle vertices and the mode-specific extended point.
        static void UpdateExtended(TriangleState &state, float deltaSeconds,
                                   float faceDistanceMeters, uint8_t hcepMode) noexcept
        {
            if (deltaSeconds <= 0.0f)
                return;

            state.fixationTimerSec += deltaSeconds;

            if (state.fixationTimerSec >= state.fixationDurationSec)
            {
                state.fixationTimerSec = 0.0f;

                // Longer fixation dwells for extended points (400-700ms)
                std::uniform_real_distribution<float> durDist(0.30f, 0.55f);
                state.fixationDurationSec = durDist(state.rng) * state.cadenceScale;

                // Determine the extended vertex based on HCEP mode
                Vertex extendedTarget = (hcepMode == 2) ? Vertex::ThirdEye : Vertex::Chest;

                // Scanpath: Triangle vertices with periodic visits to the extended point.
                // ~25% of fixations land on the extended point for a natural weave.
                // Transitions after the extended point and out of the mouth are
                // randomised so the weave never repeats identically.
                switch (state.currentVertex)
                {
                case Vertex::LeftEye:
                    state.currentVertex = Vertex::RightEye;
                    break;
                case Vertex::RightEye:
                    state.currentVertex = extendedTarget;
                    state.fixationDurationSec = std::uniform_real_distribution<float>(0.40f, 0.70f)(state.rng) * state.cadenceScale;
                    break;
                case Vertex::ThirdEye:
                case Vertex::Chest:
                {
                    // Leaving the extended point: land on a random triangle vertex
                    // rather than always the mouth.
                    std::uniform_int_distribution<int> tri(0, 2);
                    state.currentVertex = static_cast<Vertex>(tri(state.rng));
                    break;
                }
                case Vertex::Mouth:
                {
                    // From the mouth, choose either eye — no fixed handedness.
                    state.currentVertex =
                        std::uniform_real_distribution<float>(0.0f, 1.0f)(state.rng) < 0.5f
                            ? Vertex::LeftEye
                            : Vertex::RightEye;
                    break;
                }
                default:
                    state.currentVertex = Vertex::LeftEye;
                    break;
                }

                ComputeVertexOffset(state, faceDistanceMeters);
                ApplyEyeDwell(state);
            }
        }

        /// @brief Cognitive Gaze Aversion (CGA) scanpath from the HCEP Enhanced Diagram.
        /// During THINK mode, gaze breaks away to peripheral regions in the pattern shown
        /// in the CGA diagram: figure-8 / infinity-loop saccade vectors sweeping through
        /// upper-left, upper-right, lower-left, lower-right aversion quadrants, then
        /// briefly returning to the face (dominant eye) before averting again.
        ///
        /// CGA cycle: Face(brief) → UpperLeft → UpperRight → Face(brief) →
        ///            LowerRight → LowerLeft → Face(brief) → repeat
        /// This models the Vestibulo-Ocular Reflex counter-rotation arc and saccade
        /// vectors shown in the HCEP-02 enhanced diagram.
        static void UpdateCGA(TriangleState &state, float deltaSeconds,
                              float faceDistanceMeters) noexcept
        {
            if (deltaSeconds <= 0.0f)
                return;

            state.fixationTimerSec += deltaSeconds;

            if (state.fixationTimerSec >= state.fixationDurationSec)
            {
                state.fixationTimerSec = 0.0f;

                // CGA aversion regions have longer dwell (350-700ms for aversion,
                // 150-350ms for brief face returns)
                bool isAversionVertex = false;

                switch (state.currentVertex)
                {
                // Brief face return → avert to upper-left (positivity, hope)
                case Vertex::LeftEye:
                case Vertex::RightEye:
                {
                    // From face, pick an aversion direction
                    std::uniform_int_distribution<int> dir(0, 3);
                    int d = dir(state.rng);
                    switch (d)
                    {
                    case 0:
                        state.currentVertex = Vertex::UpperLeftAversion;
                        break;
                    case 1:
                        state.currentVertex = Vertex::UpperRightAversion;
                        break;
                    case 2:
                        state.currentVertex = Vertex::LowerLeftAversion;
                        break;
                    case 3:
                        state.currentVertex = Vertex::LowerRightAversion;
                        break;
                    }
                    isAversionVertex = true;
                    break;
                }
                // From aversion regions, return briefly to face (dominant eye)
                case Vertex::UpperLeftAversion:
                    state.currentVertex = Vertex::RightEye; // brief face return
                    break;
                case Vertex::UpperRightAversion:
                    state.currentVertex = Vertex::LeftEye;
                    break;
                case Vertex::LowerLeftAversion:
                    state.currentVertex = Vertex::RightEye;
                    break;
                case Vertex::LowerRightAversion:
                    state.currentVertex = Vertex::LeftEye;
                    break;
                default:
                    // ThirdEye, Mouth, Chest — return to dominant eye for CGA cycle
                    state.currentVertex = Vertex::LeftEye;
                    break;
                }

                if (isAversionVertex)
                {
                    state.fixationDurationSec = std::uniform_real_distribution<float>(0.35f, 0.70f)(state.rng) * state.cadenceScale;
                }
                else
                {
                    // Brief face fixation before next aversion (150-350ms)
                    state.fixationDurationSec = std::uniform_real_distribution<float>(0.15f, 0.35f)(state.rng) * state.cadenceScale;
                }

                ComputeVertexOffset(state, faceDistanceMeters);
                ApplyEyeDwell(state);
            }
        }

        /// @brief Dialogue-synced CGA return. Forces the gaze back to the dominant eye
        /// (face return) and exits the CGA aversion cycle. Called when dialogue begins
        /// and the NPC should snap attention to the speaker.
        ///
        /// Returns true if a CGA aversion was active and was interrupted (i.e., the
        /// return was meaningful). Returns false if the NPC was already looking at face.
        static bool ReturnToFace(TriangleState &state, float faceDistanceMeters) noexcept
        {
            const bool wasAverting =
                state.currentVertex == Vertex::UpperLeftAversion ||
                state.currentVertex == Vertex::UpperRightAversion ||
                state.currentVertex == Vertex::LowerLeftAversion ||
                state.currentVertex == Vertex::LowerRightAversion;

            // Snap to the dominant eye (face centre) and reset the fixation timer
            // so the NPC holds on the face for a normal fixation before resuming
            // any scanpath. EYE-TO-EYE DOMINANCE: the return lands on an eye, so
            // the eye dwell scale applies — during dialogue this makes the
            // post-return eye hold the longest fixation in the cycle, which is
            // exactly the dialogue contract.
            state.currentVertex = Vertex::RightEye;
            state.fixationTimerSec = 0.0f;
            state.fixationDurationSec = std::uniform_real_distribution<float>(0.30f, 0.60f)(state.rng) * state.cadenceScale;
            ComputeVertexOffset(state, faceDistanceMeters);
            ApplyEyeDwell(state);

            return wasAverting;
        }

        /// @brief Checks whether the current vertex is a CGA peripheral aversion region.
        [[nodiscard]] static bool IsAversionVertex(Vertex v) noexcept
        {
            return v == Vertex::UpperLeftAversion ||
                   v == Vertex::UpperRightAversion ||
                   v == Vertex::LowerLeftAversion ||
                   v == Vertex::LowerRightAversion;
        }

    private:
        /// @brief Computes the angular offset for the current vertex relative to face center.
        static void ComputeVertexOffset(TriangleState &state, float faceDistanceMeters) noexcept
        {
            float dist = std::max(0.5f, faceDistanceMeters);
            float eyeSeparationDeg = (0.065f / dist) * (180.0f / 3.14159f);
            float eyeMouthDeg = (0.070f / dist) * (180.0f / 3.14159f);

            switch (state.currentVertex)
            {
            case Vertex::LeftEye:
                state.vertexOffsetXDeg = -eyeSeparationDeg * 0.5f;
                state.vertexOffsetYDeg = eyeMouthDeg * 0.4f;
                break;
            case Vertex::RightEye:
                state.vertexOffsetXDeg = eyeSeparationDeg * 0.5f;
                state.vertexOffsetYDeg = eyeMouthDeg * 0.4f;
                break;
            case Vertex::Mouth:
                state.vertexOffsetXDeg = 0.0f;
                state.vertexOffsetYDeg = -eyeMouthDeg * 0.6f;
                break;
            case Vertex::ThirdEye:
                // Third-eye / forehead: centered, above eyes (~3cm above brow line)
                state.vertexOffsetXDeg = 0.0f;
                state.vertexOffsetYDeg = eyeMouthDeg * 1.2f; // above eyes
                break;
            case Vertex::Chest:
                // Heart / sternum: centered, well below face (~30cm below chin)
                state.vertexOffsetXDeg = 0.0f;
                state.vertexOffsetYDeg = -eyeMouthDeg * 4.0f; // chest level
                break;

            // CGA Peripheral Aversion Regions (from HCEP Enhanced Diagram)
            // These are OFF-FACE: the gaze breaks away from the target's face
            // into the peripheral visual field, modelling cognitive processing.
            case Vertex::UpperLeftAversion:
                // "Upper region" — positivity, happiness, hope
                state.vertexOffsetXDeg = -18.0f;
                state.vertexOffsetYDeg = 15.0f;
                break;
            case Vertex::UpperRightAversion:
                // "Far Upper regions" — search for memories, constructive thought
                state.vertexOffsetXDeg = 18.0f;
                state.vertexOffsetYDeg = 15.0f;
                break;
            case Vertex::LowerLeftAversion:
                // "Lower region" — tiredness, negativity, sadness
                state.vertexOffsetXDeg = -18.0f;
                state.vertexOffsetYDeg = -12.0f;
                break;
            case Vertex::LowerRightAversion:
                // "Far Lower regions" — shyness, fear, deception
                state.vertexOffsetXDeg = 18.0f;
                state.vertexOffsetYDeg = -12.0f;
                break;
            }

            // Per-visit scatter: repeated fixations on the same vertex must never land
            // on an identical angle. A fraction of a degree is enough that the orbit
            // cannot look like a machine tracing a fixed polygon. The scatter scales
            // with the offset magnitude so peripheral aversion points wander further
            // than the tight facial features.
            const float offsetMag = std::max(std::abs(state.vertexOffsetXDeg),
                                             std::abs(state.vertexOffsetYDeg));
            const float scatter = std::max(0.18f, 0.12f * offsetMag);
            std::uniform_real_distribution<float> scatterX(-scatter, scatter);
            std::uniform_real_distribution<float> scatterY(-scatter, scatter);
            state.vertexOffsetXDeg += scatterX(state.rng);
            state.vertexOffsetYDeg += scatterY(state.rng);
        }
    };

} // namespace TrueGaze::Kinematics
