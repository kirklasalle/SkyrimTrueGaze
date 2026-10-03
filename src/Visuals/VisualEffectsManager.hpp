#pragma once

#include "PCH.h"
#include "Visuals/VisualTuning.hpp"

#include <unordered_map>

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#endif

namespace TrueGaze::Visuals
{

    /// @brief Renders the solved gaze as in-game 3D visuals.
    ///
    /// ## "Superman Laser Eyes" & HCEP Floating Diagram Panel
    ///
    /// TrueGaze provides two primary visual diagnostic tools for developers and players:
    ///
    /// 1. **Superman Laser Eyes**: Twin laser beams and glowing pupil sockets
    ///    originating at the left and right pupil positions, projecting along the
    ///    actor's real-time solved eyeline to the gaze terminus.
    /// 2. **HCEP Floating Diagram Panel**: A glowing constellation of NiPointLights
    ///    positioned in the visual field in front of the head, representing the
    ///    11 HCEP gaze regions. As the actor looks around (e.g. fixating on the
    ///    partner's mouth or looking away into peripheral cognitive thought), the
    ///    laser eyes point directly at the active region node, which flares bright.
    ///
    /// ## Crash-Proof Architecture
    ///
    /// Built entirely on engine-native NiPointLight primitives and proven Bethesda
    /// mesh assets. Completely avoids custom Python-generated binary NIFs which can
    /// cause delayed SEH renderer access violations (0xC0000005).
    ///
    /// ## Threading
    ///
    /// Game thread only, driven from `GazeEngine::TickActor` / `ApplyToSkeleton`.
    class VisualEffectsManager
    {
    public:
        static VisualEffectsManager& Get() noexcept
        {
            static VisualEffectsManager instance;
            return instance;
        }

        VisualEffectsManager(const VisualEffectsManager&) = delete;
        VisualEffectsManager& operator=(const VisualEffectsManager&) = delete;

        /// @brief Replaces the active tuning snapshot. Called on load and on reload.
        void SetTuning(const VisualTuning& a_tuning) noexcept;

        [[nodiscard]] const VisualTuning& Tuning() const noexcept { return _tuning; }

        /// @brief Updates the in-world visuals for one actor.
        void UpdateActor(RE::Actor* a_actor, RE::NiAVObject* a_headBone, RE::NiAVObject* a_eyeL,
                         RE::NiAVObject* a_eyeR, float a_eyeYawDeg, float a_eyePitchDeg,
                         uint8_t a_gazeRegion, bool a_isPlayer, bool a_isHumanoid) noexcept;

        /// @brief Detaches every emitter for one actor. Call before state eviction.
        void RemoveActor(uint32_t a_formId) noexcept;

        /// @brief Detaches every emitter. Call on cell change, game load, or toggle.
        void Reset() noexcept;

        // --- Diagnostics -------------------------------------------------------

        /// @brief Number of actors currently holding live emitters.
        [[nodiscard]] size_t ActiveActorCount() const noexcept { return _emitters.size(); }

        /// @brief Number of light objects currently attached to skeletons.
        [[nodiscard]] size_t AttachedLightCount() const noexcept { return _attachedLights; }

        /// @brief True while any in-game visuals are enabled by the active tuning.
        [[nodiscard]] bool EmittersActive() const noexcept
        {
            return _tuning.enableInGameVisuals &&
                   (_tuning.gazeRaysEnabled || _tuning.showHcepPanel);
        }

        /// @brief True once the geometry-mode notice has been emitted.
        [[nodiscard]] bool GeometryModeReported() const noexcept { return _geometryModeReported; }

        [[nodiscard]] uint64_t UpdateCalls() const noexcept { return _updateCalls; }
        [[nodiscard]] uint64_t AnchorFailures() const noexcept { return _anchorFailures; }
        [[nodiscard]] uint64_t LightCreateFailures() const noexcept { return _lightCreateFailures; }
        [[nodiscard]] uint64_t LightsCreated() const noexcept { return _lightsCreated; }
        [[nodiscard]] uint64_t GeometryAttempts() const noexcept { return _geometryAttempts; }
        [[nodiscard]] uint64_t GeometryCreated() const noexcept { return _geometryCreated; }

    private:
        VisualEffectsManager() = default;

        /// Per-actor emitter bookkeeping.
        struct ActorEmitters
        {
            enum class GeometryState : uint8_t
            {
                Unknown,
                Pending,
                Loaded,
                Missing,
                Invalid
            };

            // Superman Laser Eyes: Twin Pupil Lights (Left & Right) + Terminus Light
            RE::NiPointer<RE::NiPointLight> pupilLightL{};
            RE::NiPointer<RE::NiPointLight> pupilLightR{};
            RE::NiPointer<RE::NiPointLight> terminusLight{};

            // Twin Beam Geometries (Left & Right)
            RE::NiPointer<RE::NiAVObject> geometryL{};
            RE::NiPointer<RE::NiAVObject> geometryR{};

            uint32_t geometryAttempts{0};
            GeometryState geometryState{GeometryState::Unknown};
            uint64_t lastGeometryAttemptFrame{0};
            bool usingFallbackMesh{false};
            bool isMarkerArrow{false};

            // HCEP Floating Diagram Panel: Real 3D planar quad mesh
            RE::NiPointer<RE::NiAVObject> hcepPanel{};
            uint32_t panelAttempts{0};
            GeometryState panelState{GeometryState::Unknown};
            uint64_t lastPanelAttemptFrame{0};

            uint8_t lastGazeRegion{0xFF};

            RE::NiPointer<RE::NiAVObject> parent{};
            uint64_t lastFrame{0};
        };

        /// Compute twin pupil origins (Left and Right) and gaze direction in world space.
        void ResolvePupils(const RE::NiAVObject* a_headBone, const RE::NiAVObject* a_eyeL,
                           const RE::NiAVObject* a_eyeR, const RE::NiAVObject* a_anchor,
                           float a_eyeYawDeg, float a_eyePitchDeg, RE::NiPoint3& a_originLOut,
                           RE::NiPoint3& a_originROut, RE::NiPoint3& a_dirOut) const noexcept;

        /// Read a light's mutable runtime data so its colour can be set.
        static void ApplyLightColour(RE::NiPointLight* a_light, float a_r, float a_g,
                                     float a_b) noexcept;

        // --- Emitter lifecycle helpers ---

        void EnsureLights(ActorEmitters& a_emitters, RE::NiAVObject* a_anchor) noexcept;
        void DetachLights(ActorEmitters& a_emitters) noexcept;

        void EnsureBeamGeometry(ActorEmitters& a_emitters, RE::NiAVObject* a_anchor) noexcept;
        void EnsureBeamGeometryInner(ActorEmitters& a_emitters, RE::NiAVObject* a_anchor) noexcept;

        void EnsureHcepPanel(ActorEmitters& a_emitters, RE::NiAVObject* a_anchor) noexcept;
        void EnsureHcepPanelInner(ActorEmitters& a_emitters, RE::NiAVObject* a_anchor) noexcept;
        void UpdateHcepPanel(ActorEmitters& a_emitters, RE::NiAVObject* a_anchor,
                             uint8_t a_gazeRegion) noexcept;
        void DetachPanel(ActorEmitters& a_emitters) noexcept;

        void AttachEmitters(ActorEmitters& a_emitters, RE::NiAVObject* a_anchor) noexcept;
        void DetachAll(ActorEmitters& a_emitters) noexcept;

        std::unordered_map<uint32_t, ActorEmitters> _emitters;
        VisualTuning _tuning{};

        size_t _attachedLights{0};
        bool _geometryModeReported{false};

        uint64_t _frameCounter{0};
        uint64_t _updateCalls{0};
        uint64_t _anchorFailures{0};
        uint64_t _lightCreateFailures{0};
        uint64_t _lightsCreated{0};
        uint64_t _geometryAttempts{0};
        uint64_t _geometryCreated{0};

        static constexpr size_t kMaxEmitterActors = 64;
    };

} // namespace TrueGaze::Visuals