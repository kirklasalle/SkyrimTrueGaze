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
    class SocialTriangle
    {
    public:
        enum class Vertex : uint8_t
        {
            LeftEye = 0,
            RightEye = 1,
            Mouth = 2
        };

        struct TriangleState
        {
            Vertex currentVertex{Vertex::LeftEye};
            float fixationTimerSec{0.0f};
            float fixationDurationSec{0.35f}; // Typical fixation ~350ms
            float vertexOffsetXDeg{-1.8f};    // Offset relative to face center
            float vertexOffsetYDeg{1.0f};
        };

        /// @brief Steps the social triangle state, advancing to the next facial vertex on expiration.
        static void Update(TriangleState &state, float deltaSeconds, float faceDistanceMeters = 1.5f) noexcept
        {
            if (deltaSeconds <= 0.0f)
                return;

            state.fixationTimerSec += deltaSeconds;

            if (state.fixationTimerSec >= state.fixationDurationSec)
            {
                state.fixationTimerSec = 0.0f;

                // Random fixation duration for organic variability (250ms to 450ms)
                static thread_local std::mt19937 rng{42};
                std::uniform_real_distribution<float> durDist(0.25f, 0.45f);
                state.fixationDurationSec = durDist(rng);

                // Cycle: Left Eye -> Right Eye -> Mouth -> Left Eye
                switch (state.currentVertex)
                {
                case Vertex::LeftEye:
                    state.currentVertex = Vertex::RightEye;
                    break;
                case Vertex::RightEye:
                    state.currentVertex = Vertex::Mouth;
                    break;
                case Vertex::Mouth:
                    state.currentVertex = Vertex::LeftEye;
                    break;
                }

                // Calculate angular separation adjusted for distance
                // Interpupillary distance ~6.5cm, Eye-mouth distance ~7.0cm
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
                }
            }
        }
    };

} // namespace TrueGaze::Kinematics
