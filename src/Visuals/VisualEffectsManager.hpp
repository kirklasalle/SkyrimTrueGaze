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
    /// ## Why this exists
    ///
    /// Before this, TrueGaze computed a full per-actor gaze solution every frame and
    /// rendered nothing in-world. The only "debug rays" were text: a throttled
    /// spdlog line, a console Print, and a HUD message string. There was no geometry,
    /// no light, and no way to *see* the solver working. See
    /// `docs/Implementation Plan - In-Game 3D Visual System & Gaze Ray Assets.md`.
    ///
    /// This class is a **pure consumer** of `Engine::GazeEngine` state. It never
    /// recomputes gaze; it subscribes to `lastYawDeg` / `lastPitchDeg` and the cached
    /// skeleton bones. That guarantees what you see is *the solver's own answer*, not
    /// a re-derivation that could drift from it.
    ///
    /// ## Rendering strategy
    ///
    /// The plugin hooks `Actor::Update` — a logic tick, not a render hook. It can
    /// therefore attach and orient scene-graph objects, but it must not issue draw
    /// calls. Two engine-native strategies do the actual drawing:
    ///
    ///   * **NiPointLight** emitters - a real light at the pupil and (optionally) at
    ///     the gaze terminus. Costs no art assets, so it is the V1 path and the
    ///     bare-bones mode.
    ///   * **Branded NIF geometry** - attached under the head bone, when the beam
    ///     assets exist. Added in a later phase; this class discovers them lazily.
    ///
    /// ## Threading
    ///
    /// Game thread only, driven from `GazeEngine::TickActor` / `ApplyToSkeleton`,
    /// exactly like the rest of the kinematics engine. No locks: the emitter map is touched
    /// only from the game thread.
    class VisualEffectsManager
    {
    public:
        static VisualEffectsManager &Get() noexcept
        {
            static VisualEffectsManager instance;
            return instance;
        }

        VisualEffectsManager(const VisualEffectsManager &) = delete;
        VisualEffectsManager &operator=(const VisualEffectsManager &) = delete;

        /// @brief Replaces the active tuning snapshot. Called on load and on reload.
        void SetTuning(const VisualTuning &a_tuning) noexcept;

        [[nodiscard]] const VisualTuning &Tuning() const noexcept { return _tuning; }

        /// @brief Updates the in-world visuals for one actor.
        ///
        /// Called after the actor's skeleton has been posed for the frame, so the
        /// head bone transform is current. Creates the emitters on first call for an
        /// actor and reuses them thereafter.
        ///
        /// @param a_actor        The actor whose gaze is being visualised.
        /// @param a_headBone     Resolved head bone, or nullptr if the rig lacks one.
        /// @param a_eyeL/a_eyeR  Resolved eye bones when the rig exposes them, else nullptr.
        /// @param a_eyeYawDeg/a_eyePitchDeg
        ///        The **eye residual** deflection - what the eyes carry beyond the
        ///        already-posed head. This is deliberate: the head bone's world transform
        ///        already includes the head's share of the turn, so applying the *total*
        ///        deflection here would double-count it and the beam would overshoot.
        /// @param a_gazeRegion   Classified region id (reserved for developer colour coding).
        /// @param a_isPlayer     True for the player character.
        /// @param a_isHumanoid   True when the actor is a humanoid (creature filter).
        void UpdateActor(RE::Actor *a_actor,
                         RE::NiAVObject *a_headBone,
                         RE::NiAVObject *a_eyeL,
                         RE::NiAVObject *a_eyeR,
                         float a_eyeYawDeg,
                         float a_eyePitchDeg,
                         uint8_t a_gazeRegion,
                         bool a_isPlayer,
                         bool a_isHumanoid) noexcept;

        /// @brief Detaches every emitter for one actor. Call before state eviction.
        void RemoveActor(uint32_t a_formId) noexcept;

        /// @brief Detaches every emitter. Call on cell change and game load.
        void Reset() noexcept;

        // --- Diagnostics -------------------------------------------------------

        /// @brief Number of actors currently holding live emitters.
        [[nodiscard]] size_t ActiveActorCount() const noexcept { return _emitters.size(); }

        /// @brief Number of light objects currently attached to skeletons.
        [[nodiscard]] size_t AttachedLightCount() const noexcept { return _attachedLights; }

        /// @brief True while the gaze emitters are enabled by the active tuning.
        /// Used by the console status command so the reported state is the engine's,
        /// not a second reading of the config file.
        [[nodiscard]] bool EmittersActive() const noexcept
        {
            return _tuning.enableInGameVisuals && (_tuning.gazeRaysEnabled || _tuning.showHcepPanel);
        }

        /// @brief True once the geometry-mode notice has been emitted (success or not).
        [[nodiscard]] bool GeometryModeReported() const noexcept { return _geometryModeReported; }

        /// Diagnostic lifecycle counters. Lights are intentionally separate from
        /// visible beam geometry; these counters make that distinction explicit.
        [[nodiscard]] uint64_t UpdateCalls() const noexcept { return _updateCalls; }
        [[nodiscard]] uint64_t AnchorFailures() const noexcept { return _anchorFailures; }
        [[nodiscard]] uint64_t LightCreateFailures() const noexcept { return _lightCreateFailures; }
        [[nodiscard]] uint64_t LightsCreated() const noexcept { return _lightsCreated; }
        [[nodiscard]] uint64_t GeometryAttempts() const noexcept { return _geometryAttempts; }
        [[nodiscard]] uint64_t GeometryCreated() const noexcept { return _geometryCreated; }

    private:
        VisualEffectsManager() = default;

        /// Per-actor emitter bookkeeping. Owns the light objects, which are detached
        /// from the skeleton on RemoveActor / Reset.
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

            RE::NiPointer<RE::NiPointLight> pupilLight{};
            RE::NiPointer<RE::NiPointLight> terminusLight{};
            RE::NiPointer<RE::NiAVObject> geometry{};
            uint32_t geometryAttempts{0};
            GeometryState geometryState{GeometryState::Unknown};
            uint64_t lastGeometryAttemptFrame{0};
            bool usingFallbackMesh{false}; ///< true when marker_arrow.nif is in use

            // HCEP Floating Diagram Panel
            RE::NiPointer<RE::NiAVObject> hcepPanel{};
            uint32_t panelAttempts{0};
            GeometryState panelState{GeometryState::Unknown};
            uint64_t lastPanelAttemptFrame{0};
            uint8_t lastGazeRegion{0xFF};

            /// The parent the emitters were attached to, so detach targets the right node.
            RE::NiPointer<RE::NiAVObject> parent{};

            /// Last frame this actor's emitters were refreshed. Recorded for the per-actor
            /// update-rate decimation planned in phase V5; nothing reads it yet, and it is
            /// documented as such rather than described as an active throttle.
            uint64_t lastFrame{0};
        };

        /// Compute the pupil origin and gaze direction in world space.
        ///
        /// Vanilla humanoid rigs expose no eye bones (eyes are FaceGen morphs), so the
        /// origin is derived geometrically from the head bone: its world transform
        /// columns give the forward/up basis, and the configured offsets place the
        /// pupil in the socket. Custom rigs with real eye bones pass them in and are
        /// used directly.
        void ResolvePupil(const RE::NiAVObject *a_headBone,
                          const RE::NiAVObject *a_eyeL,
                          const RE::NiAVObject *a_eyeR,
                          float a_eyeYawDeg,
                          float a_eyePitchDeg,
                          RE::NiPoint3 &a_originOut,
                          RE::NiPoint3 &a_dirOut) const noexcept;

        /// Read a light's mutable runtime data so its colour can be set.
        static void ApplyLightColour(RE::NiPointLight *a_light,
                                     float a_r, float a_g, float a_b) noexcept;

        // --- Emitter lifecycle helpers -----------------------------------------

        /// Create the pupil (and, when configured, terminus) light under an anchor.
        /// Idempotent: an existing light is left in place.
        void EnsureLight(ActorEmitters &a_emitters, RE::NiAVObject *a_anchor, bool a_pupil) noexcept;

        /// Load and attach the visible beam model. The model is deliberately loaded
        /// through Skyrim's BSModelDB so loose files and BSA assets both work.
        void EnsureBeamGeometry(ActorEmitters &a_emitters, RE::NiAVObject *a_anchor) noexcept;

        /// Load and attach the floating HCEP diagram panel.
        void EnsureHcepPanel(ActorEmitters &a_emitters, RE::NiAVObject *a_anchor) noexcept;

        /// Update the floating HCEP diagram panel's position, orientation, and active region highlight.
        void UpdateHcepPanel(ActorEmitters &a_emitters, RE::NiAVObject *a_anchor, uint8_t a_gazeRegion) noexcept;

        /// Detach the HCEP diagram panel.
        void DetachPanel(ActorEmitters &a_emitters) noexcept;

        /// Detach and release both light emitters. Safe when none exist.
        void DetachLights(ActorEmitters &a_emitters) noexcept;

        /// Record a new attachment anchor. Detaching from the old one is the caller's
        /// responsibility (see DetachAll).
        void AttachEmitters(ActorEmitters &a_emitters, RE::NiAVObject *a_anchor) noexcept;

        /// Detach every emitter from its anchor and clear the record.
        void DetachAll(ActorEmitters &a_emitters) noexcept;

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

        /// Bound on actors holding visual emitters, independent of the kinematics engine's
        /// own cap, so a large cell cannot multiply the visual cost without limit.
        static constexpr size_t kMaxEmitterActors = 64;
    };

} // namespace TrueGaze::Visuals