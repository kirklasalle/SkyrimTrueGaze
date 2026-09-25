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
            float vertexOffsetXDeg{-1.8f};    // Offset relative to face center
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

            // Blend: at r = 1 the canonical step still leads slightly (0.45) because
            // humans DO favour eye-to-eye transitions, but lateral moves (0.35) and
            // same-point re-fixations (0.20) break every predictable loop.
            const float pCanonical = (1.0f - r) + r * 0.45f;
            const float pOther = r * 0.35f;

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
                state.fixationDurationSec = durDist(state.rng);

                // Weighted-random next vertex: favours eye-to-eye transitions the way
                // humans do, but lateral jumps and re-fixations destroy the fixed orbit.
                state.currentVertex = NextTriangleVertex(state, state.currentVertex, pathRandomness);

                ComputeVertexOffset(state, faceDistanceMeters);
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
                state.fixationDurationSec = durDist(state.rng);

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
                    state.fixationDurationSec = std::uniform_real_distribution<float>(0.40f, 0.70f)(state.rng);
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
                    state.fixationDurationSec = std::uniform_real_distribution<float>(0.35f, 0.70f)(state.rng);
                }
                else
                {
                    // Brief face fixation before next aversion (150-350ms)
                    state.fixationDurationSec = std::uniform_real_distribution<float>(0.15f, 0.35f)(state.rng);
                }

                ComputeVertexOffset(state, faceDistanceMeters);
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
            // any scanpath.
            state.currentVertex = Vertex::RightEye;
            state.fixationTimerSec = 0.0f;
            state.fixationDurationSec = std::uniform_real_distribution<float>(0.30f, 0.60f)(state.rng);
            ComputeVertexOffset(state, faceDistanceMeters);

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
