#include "VisualEffectsManager.hpp"

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#include <RE/B/BSModelDB.h>
#endif

#include <cmath>

namespace TrueGaze::Visuals
{

    namespace
    {

        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kDegToRad = kPi / 180.0f;

        /// Beam radius as a fraction of its length. A fixed radius would be invisible
        /// on a long beam and a blob on a short one, so the light scales with reach.
        constexpr float kBeamRadiusFraction = 0.04f;
        constexpr float kMinBeamRadius = 4.0f; // Skyrim units

        /// Name given to the attached light nodes. Prefixed so they are obvious in a
        /// scene-graph dump and cannot collide with skeleton bone names.
        constexpr const char *kPupilLightName = "TrueGaze_PupilLight";
        constexpr const char *kTerminusLightName = "TrueGaze_TerminusLight";
        // The beam path is configurable through VisualTuning::beamModelPath so an
        // extracted or original asset can be tested without a rebuild. The default
        // remains the current development candidate.

#if __has_include(<RE/Skyrim.h>)
        RE::NiPoint3 Cross(const RE::NiPoint3 &a_lhs, const RE::NiPoint3 &a_rhs) noexcept
        {
            return RE::NiPoint3{a_lhs.y * a_rhs.z - a_lhs.z * a_rhs.y,
                                a_lhs.z * a_rhs.x - a_lhs.x * a_rhs.z,
                                a_lhs.x * a_rhs.y - a_lhs.y * a_rhs.x};
        }

        RE::NiMatrix3 BeamRotation(const RE::NiPoint3 &a_forward) noexcept
        {
            RE::NiPoint3 forward = a_forward;
            (void)forward.Unitize();
            RE::NiPoint3 referenceUp{0.0f, 0.0f, 1.0f};
            if (std::abs(forward.z) > 0.98f)
            {
                referenceUp = RE::NiPoint3{0.0f, 1.0f, 0.0f};
            }
            RE::NiPoint3 right = Cross(forward, referenceUp);
            (void)right.Unitize();
            RE::NiPoint3 up = Cross(right, forward);
            (void)up.Unitize();
            return RE::NiMatrix3(right, forward, up);
        }
#endif

#if __has_include(<RE/Skyrim.h>)

        /// Build a world-space direction from the head basis and an eye deflection.
        ///
        /// Basis convention (NiMatrix3 columns): X = right, Y = forward, Z = up.
        /// The deflection is gimbal-style: yaw about up, pitch about right.
        RE::NiPoint3 GazeDirection(const RE::NiMatrix3 &a_basis,
                                   float a_yawRad,
                                   float a_pitchRad) noexcept
        {
            const RE::NiPoint3 right = a_basis.GetVectorX();
            const RE::NiPoint3 forward = a_basis.GetVectorY();
            const RE::NiPoint3 up = a_basis.GetVectorZ();

            const float cy = std::cos(a_yawRad);
            const float sy = std::sin(a_yawRad);
            const float cp = std::cos(a_pitchRad);
            const float sp = std::sin(a_pitchRad);

            RE::NiPoint3 dir{
                forward.x * cy * cp + right.x * sy * cp + up.x * sp,
                forward.y * cy * cp + right.y * sy * cp + up.y * sp,
                forward.z * cy * cp + right.z * sy * cp + up.z * sp};

            (void)dir.Unitize();
            return dir;
        }

#endif // __has_include(<RE/Skyrim.h>)

    } // namespace

    // ---------------------------------------------------------------------------
    // Lifecycle
    // ---------------------------------------------------------------------------

    void VisualEffectsManager::SetTuning(const VisualTuning &a_tuning) noexcept
    {
        _tuning = a_tuning;

        // If the subsystem has just been switched off, tear everything down so no
        // orphaned lights are left glowing in the world.
        if (!_tuning.enableInGameVisuals || !_tuning.gazeRaysEnabled)
        {
            Reset();
        }

        logger::info("[TrueGaze] Visual tuning: visuals={} rays={} mode={} length={:.1f}m "
                     "colour=#{:06X} opacity={:.2f} terminus={}",
                     _tuning.enableInGameVisuals ? "on" : "off",
                     _tuning.gazeRaysEnabled ? "on" : "off",
                     _tuning.rayRenderMode,
                     _tuning.gazeRayLengthMeters,
                     _tuning.gazeRayColour & 0x00FFFFFF,
                     _tuning.gazeRayOpacity,
                     _tuning.gazeRaysTerminus ? "yes" : "no");
    }

    void VisualEffectsManager::Reset() noexcept
    {
        // Dropping the map releases the NiPointers, but the light nodes are still
        // attached to the skeletons. Detach them first so the scene graph does not
        // hold dangling children.
        for (auto &kv : _emitters)
        {
            DetachAll(kv.second);
        }

        _emitters.clear();
        _attachedLights = 0;
        logger::info("[TrueGaze] In-game visual emitters cleared.");
    }

    void VisualEffectsManager::RemoveActor(uint32_t a_formId) noexcept
    {
        auto it = _emitters.find(a_formId);
        if (it == _emitters.end())
        {
            return;
        }

        DetachAll(it->second);
        _emitters.erase(it);
    }

    // ---------------------------------------------------------------------------
    // Per-actor update
    // ---------------------------------------------------------------------------

    void VisualEffectsManager::UpdateActor(RE::Actor *a_actor,
                                           RE::NiAVObject *a_headBone,
                                           RE::NiAVObject *a_eyeL,
                                           RE::NiAVObject *a_eyeR,
                                           float a_eyeYawDeg,
                                           float a_eyePitchDeg,
                                           uint8_t a_gazeRegion,
                                           bool a_isPlayer,
                                           bool a_isHumanoid) noexcept
    {
        (void)a_gazeRegion; // reserved: developer state colour coding (phase V3)

#if __has_include(<RE/Skyrim.h>)
        if (!a_actor)
        {
            return;
        }

        ++_updateCalls;

        const uint32_t formId = a_actor->GetFormID();

        // When visuals are disabled, make sure any emitters this actor previously had
        // are removed - the toggle must take effect without a reload.
        if (!_tuning.enableInGameVisuals || !_tuning.gazeRaysEnabled)
        {
            RemoveActor(formId);
            return;
        }

        // Eligibility filters. The master simulation switch has already been checked
        // by the caller, so only the visual-specific filters live here.
        if (a_isPlayer && !_tuning.gazeRaysOnPlayer)
        {
            RemoveActor(formId);
            return;
        }
        if (!a_isPlayer && a_isHumanoid && !_tuning.gazeRaysOnNPCs)
        {
            RemoveActor(formId);
            return;
        }
        if (!a_isPlayer && !a_isHumanoid && !_tuning.gazeRaysOnCreatures)
        {
            RemoveActor(formId);
            return;
        }

        ++_frameCounter;

        // The head bone is the attachment anchor. Without it we cannot place the
        // emitter; without emitters there is nothing to withdraw.
        RE::NiAVObject *anchor = a_headBone;
        if (!_tuning.gazeRaysAttachHead || !anchor)
        {
            anchor = a_actor->Get3D();
        }
        if (!anchor)
        {
            ++_anchorFailures;
            RemoveActor(formId);
            return;
        }

        RE::NiPoint3 originWorld{};
        RE::NiPoint3 dirWorld{};
        ResolvePupil(a_headBone, a_eyeL, a_eyeR, a_eyeYawDeg, a_eyePitchDeg, originWorld, dirWorld);

        // Find or create the emitter record. A fresh entry has parent == nullptr, which
        // is what triggers attachment below.
        auto it = _emitters.find(formId);
        if (it == _emitters.end())
        {
            if (_emitters.size() >= kMaxEmitterActors)
            {
                // Bound the emitter count independently of the simulation cap so a
                // large cell cannot multiply the visual cost without limit.
                return;
            }
            it = _emitters.emplace(formId, ActorEmitters{}).first;
        }

        ActorEmitters &emitters = it->second;
        emitters.lastFrame = _frameCounter;

        // (Re)parent when the anchor changed - e.g. the actor's 3D was rebuilt on a
        // cell change, which does not fire our reset path. Detach from the OLD parent
        // first, then attach to the new one.
        if (emitters.parent.get() != anchor)
        {
            DetachAll(emitters);
            AttachEmitters(emitters, anchor);
        }

        const bool wantLight = _tuning.UseLightEmitters();

        if (_tuning.UseGeometry())
        {
            EnsureBeamGeometry(emitters, anchor);
        }

        if (wantLight)
        {
            EnsureLight(emitters, anchor, true);

            // The terminus light is optional; EnsureLight retires it when disabled.
            if (_tuning.gazeRaysTerminus)
            {
                EnsureLight(emitters, anchor, false);
            }
            else if (emitters.terminusLight)
            {
                DetachLights(emitters);
                EnsureLight(emitters, anchor, true);
            }

            // Brightness folds the user's opacity and the pupil-glow multiplier so the
            // two keys compose instead of one silently winning.
            const float scale = std::clamp(_tuning.gazeRayOpacity * _tuning.pupilGlowIntensity,
                                           0.0f, 1.0f);
            ApplyLightColour(emitters.pupilLight.get(),
                             _tuning.ColourR() * scale,
                             _tuning.ColourG() * scale,
                             _tuning.ColourB() * scale);
            ApplyLightColour(emitters.terminusLight.get(),
                             _tuning.ColourR() * scale,
                             _tuning.ColourG() * scale,
                             _tuning.ColourB() * scale);
        }
        else if (emitters.pupilLight || emitters.terminusLight)
        {
            // GeometryOnly mode: retire any light emitters.
            DetachLights(emitters);
        }

        // --- Geometry path -----------------------------------------------------
        // The geometry path uses a verified vanilla beam model. Report only once so a
        // failed archive lookup cannot flood the log.
        if (_tuning.UseGeometry() && !_geometryModeReported)
        {
            _geometryModeReported = true;
            if (_tuning.rayRenderMode == 2)
            {
                logger::info("[TrueGaze] iRayRenderMode=2 (Geometry only): using beam asset '{}'.",
                             _tuning.beamModelPath);
            }
            else
            {
                logger::info("[TrueGaze] Using beam geometry '{}' plus light emitters.",
                             _tuning.beamModelPath);
            }
        }

        // --- Position ----------------------------------------------------------
        // The attached lights sit at the pupil and never move, so their LOCAL offset is
        // what places them. Compute it in the anchor's frame and write it. The gaze
        // *direction* is not a light property; it is expressed by the terminus light's
        // position, which necessarily follows the direction.
        const RE::NiTransform anchorInverse = anchor->world.Invert();
        const RE::NiPoint3 localPupil = anchorInverse * originWorld;

        if (emitters.pupilLight)
        {
            emitters.pupilLight->local.translate = localPupil;
        }

        if (emitters.terminusLight)
        {
            const RE::NiPoint3 terminusWorld{
                originWorld.x + dirWorld.x * _tuning.LengthUnits(),
                originWorld.y + dirWorld.y * _tuning.LengthUnits(),
                originWorld.z + dirWorld.z * _tuning.LengthUnits()};
            emitters.terminusLight->local.translate = anchorInverse * terminusWorld;
        }

        if (emitters.geometry)
        {
            // The vanilla beam model is authored along local +Y. Reposition and
            // orient its root every update so it visibly follows the solved gaze.
            const RE::NiPoint3 localDirection = anchorInverse.rotate * dirWorld;
            emitters.geometry->local.translate = localPupil;
            emitters.geometry->local.rotate = BeamRotation(localDirection);
            emitters.geometry->local.scale = _tuning.LengthUnits();
        }
#else
        (void)a_actor;
        (void)a_headBone;
        (void)a_eyeL;
        (void)a_eyeR;
        (void)a_eyeYawDeg;
        (void)a_eyePitchDeg;
        (void)a_isPlayer;
        (void)a_isHumanoid;
#endif
    }

    // ---------------------------------------------------------------------------
    // Emitter management
    // ---------------------------------------------------------------------------

#if __has_include(<RE/Skyrim.h>)

    void VisualEffectsManager::EnsureLight(ActorEmitters &a_emitters,
                                           RE::NiAVObject *a_anchor,
                                           bool a_pupil) noexcept
    {
        auto *slot = a_pupil ? &a_emitters.pupilLight : &a_emitters.terminusLight;
        if (*slot)
        {
            return; // idempotent
        }

        auto *node = a_anchor ? a_anchor->AsNode() : nullptr;
        if (!node)
        {
            return;
        }

        auto *light = RE::NiPointLight::Create();
        if (!light)
        {
            ++_lightCreateFailures;
            logger::warn("[TrueGaze] Visual emitter creation failed: {} light for anchor.",
                         a_pupil ? "pupil" : "terminus");
            return;
        }

        light->name = RE::BSFixedString(a_pupil ? kPupilLightName : kTerminusLightName);

        // The terminus glow is deliberately smaller than the pupil glow: it marks the
        // landing point without competing with the source.
        const float radiusScale = a_pupil ? 1.0f : 0.6f;
        const float radius = std::max(kMinBeamRadius,
                                      _tuning.LengthUnits() * kBeamRadiusFraction * radiusScale);
        light->SetLightAttenuation(radius);

        light->local.rotate = RE::NiMatrix3();
        light->local.scale = 1.0f;

        node->AttachChild(light, false);
        slot->reset(light);
        ++_attachedLights;
        ++_lightsCreated;
        if (_lightsCreated <= 2)
        {
            logger::info("[TrueGaze] Visual emitter attached: {} NiPointLight (light-only mode; "
                         "no visible beam geometry)",
                         a_pupil ? "pupil" : "terminus");
        }
    }

    void VisualEffectsManager::DetachLights(ActorEmitters &a_emitters) noexcept
    {
        auto *node = a_emitters.parent.get() ? a_emitters.parent->AsNode() : nullptr;
        if (!node)
        {
            return;
        }

        if (a_emitters.pupilLight)
        {
            node->DetachChild(a_emitters.pupilLight.get());
            a_emitters.pupilLight = nullptr;
            if (_attachedLights > 0)
            {
                --_attachedLights;
            }
        }
        if (a_emitters.terminusLight)
        {
            node->DetachChild(a_emitters.terminusLight.get());
            a_emitters.terminusLight = nullptr;
            if (_attachedLights > 0)
            {
                --_attachedLights;
            }
        }
    }

    void VisualEffectsManager::AttachEmitters(ActorEmitters &a_emitters,
                                              RE::NiAVObject *a_anchor) noexcept
    {
        a_emitters.parent.reset(a_anchor);
    }

    void VisualEffectsManager::DetachAll(ActorEmitters &a_emitters) noexcept
    {
        DetachLights(a_emitters);

        auto *node = a_emitters.parent.get() ? a_emitters.parent->AsNode() : nullptr;
        if (node && a_emitters.geometry)
        {
            node->DetachChild(a_emitters.geometry.get());
        }
        a_emitters.geometry = nullptr;
        a_emitters.parent = nullptr;
    }

    void VisualEffectsManager::EnsureBeamGeometry(ActorEmitters &a_emitters,
                                                  RE::NiAVObject *a_anchor) noexcept
    {
        if (a_emitters.geometry || !a_anchor || !a_anchor->AsNode())
        {
            return;
        }

        // Demand may complete asynchronously. Retry at a bounded cadence while the
        // actor remains active, rather than probing the model database every update.
        // A known-missing path backs off more aggressively so a bad asset cannot add
        // a resource lookup to every actor tick.
        const uint64_t retryFrames = a_emitters.geometryState == ActorEmitters::GeometryState::Missing
                                         ? 300
                                         : 30;
        if (a_emitters.geometryAttempts > 0 &&
            _frameCounter - a_emitters.lastGeometryAttemptFrame < retryFrames)
        {
            return;
        }

        ++a_emitters.geometryAttempts;
        ++_geometryAttempts;
        a_emitters.lastGeometryAttemptFrame = _frameCounter;
        a_emitters.geometryState = ActorEmitters::GeometryState::Pending;

        RE::NiPointer<RE::NiNode> model;
        RE::BSModelDB::DBTraits::ArgsType args{};
        const auto result = RE::BSModelDB::Demand(_tuning.beamModelPath, model, args);
        if (result != RE::BSResource::ErrorCode::kNone || !model)
        {
            a_emitters.geometryState = result == RE::BSResource::ErrorCode::kNotExist
                                           ? ActorEmitters::GeometryState::Missing
                                           : ActorEmitters::GeometryState::Invalid;
            if (a_emitters.geometryAttempts == 1 || result != RE::BSResource::ErrorCode::kNotExist)
            {
                logger::warn("[TrueGaze] Beam geometry unavailable: '{}' result={}",
                             _tuning.beamModelPath, static_cast<unsigned>(result));
            }
            return;
        }

        a_anchor->AsNode()->AttachChild(model.get(), false);
        a_emitters.geometry.reset(model.get());
        a_emitters.geometryState = ActorEmitters::GeometryState::Loaded;
        ++_geometryCreated;
        logger::info("[TrueGaze] Visible beam geometry attached from '{}' and will follow solved gaze",
                     _tuning.beamModelPath);
    }

#else

    void VisualEffectsManager::EnsureLight(ActorEmitters &, RE::NiAVObject *, bool) noexcept {}
    void VisualEffectsManager::DetachLights(ActorEmitters &) noexcept {}
    void VisualEffectsManager::AttachEmitters(ActorEmitters &, RE::NiAVObject *) noexcept {}
    void VisualEffectsManager::DetachAll(ActorEmitters &) noexcept {}

#endif // __has_include(<RE/Skyrim.h>)

    // ---------------------------------------------------------------------------
    // Geometry helpers
    // ---------------------------------------------------------------------------

    void VisualEffectsManager::ResolvePupil(const RE::NiAVObject *a_headBone,
                                            const RE::NiAVObject *a_eyeL,
                                            const RE::NiAVObject *a_eyeR,
                                            float a_eyeYawDeg,
                                            float a_eyePitchDeg,
                                            RE::NiPoint3 &a_originOut,
                                            RE::NiPoint3 &a_dirOut) const noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        // Prefer a real eye bone when the rig has one (XP32/XPMSSE, some creatures).
        // Its world transform already accounts for the animation and the head turn.
        const RE::NiAVObject *eye = a_eyeL ? a_eyeL : a_eyeR;

        const RE::NiMatrix3 basis = a_headBone ? a_headBone->world.rotate
                                               : (eye ? eye->world.rotate : RE::NiMatrix3());

        const float yawRad = a_eyeYawDeg * kDegToRad;
        const float pitchRad = a_eyePitchDeg * kDegToRad;

        if (eye)
        {
            a_originOut = eye->world.translate;
        }
        else if (a_headBone)
        {
            // Vanilla rig: derive the socket from the head's world basis.
            const RE::NiPoint3 forward = basis.GetVectorY();
            const RE::NiPoint3 up = basis.GetVectorZ();
            const float fOff = _tuning.ForwardOffsetUnits();
            const float uOff = _tuning.UpOffsetUnits();
            a_originOut = RE::NiPoint3{
                a_headBone->world.translate.x + forward.x * fOff + up.x * uOff,
                a_headBone->world.translate.y + forward.y * fOff + up.y * uOff,
                a_headBone->world.translate.z + forward.z * fOff + up.z * uOff};
        }
        else
        {
            a_originOut = RE::NiPoint3{};
        }

        a_dirOut = GazeDirection(basis, yawRad, pitchRad);
#else
        (void)a_headBone;
        (void)a_eyeL;
        (void)a_eyeR;
        (void)a_eyeYawDeg;
        (void)a_eyePitchDeg;
        a_originOut = RE::NiPoint3{};
        a_dirOut = RE::NiPoint3{0.0f, 1.0f, 0.0f};
#endif
    }

    void VisualEffectsManager::ApplyLightColour(RE::NiPointLight *a_light,
                                                float a_r, float a_g, float a_b) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (!a_light)
        {
            return;
        }

        auto &data = a_light->GetLightRuntimeData();
        data.diffuse = RE::NiColor(a_r, a_g, a_b);
        data.ambient = RE::NiColor(0.0f, 0.0f, 0.0f);
#else
        (void)a_light;
        (void)a_r;
        (void)a_g;
        (void)a_b;
#endif
    }

} // namespace TrueGaze::Visuals