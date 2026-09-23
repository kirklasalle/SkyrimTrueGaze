#include "VisualEffectsManager.hpp"

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#include <RE/B/BSModelDB.h>
#include <RE/B/BSEffectShaderProperty.h>
#include <RE/B/BSEffectShaderMaterial.h>
#include <RE/B/BSLightingShaderProperty.h>
#include <RE/B/BSLightingShaderMaterial.h>
#include <RE/N/NiAlphaProperty.h>
#endif

#include <cmath>
#include <vector>

namespace TrueGaze::Visuals
{

    namespace
    {

        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kDegToRad = kPi / 180.0f;

        /// Fixed pupil glow radius. The light marks the beam origin, not the beam
        /// itself, so a small constant produces the correct "laser source" look.
        /// Sized up from 6/4 after in-game testing showed the glows were invisible
        /// at normal viewing distance in a lit interior.
        constexpr float kPupilGlowRadius = 8.0f;    // ~11 cm subtle pupil glow
        constexpr float kTerminusGlowRadius = 6.0f; // small landing dot

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

        RE::NiMatrix3 BeamTransform(const RE::NiPoint3 &a_forward,
                                    float a_crossSectionRadius,
                                    float a_beamLength) noexcept
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

            // Encode non-uniform scale directly into the rotation matrix columns:
            // Column 0 (Right / X) scaled to beam cross-section radius
            // Column 1 (Forward / Y) scaled to target reach length
            // Column 2 (Up / Z) scaled to beam cross-section radius
            RE::NiPoint3 scaledRight{right.x * a_crossSectionRadius,
                                     right.y * a_crossSectionRadius,
                                     right.z * a_crossSectionRadius};
            RE::NiPoint3 scaledForward{forward.x * a_beamLength,
                                       forward.y * a_beamLength,
                                       forward.z * a_beamLength};
            RE::NiPoint3 scaledUp{up.x * a_crossSectionRadius,
                                  up.y * a_crossSectionRadius,
                                  up.z * a_crossSectionRadius};

            return RE::NiMatrix3(scaledRight, scaledForward, scaledUp);
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

        /// Recolour and de-blinding the loaded beam geometry at runtime.
        ///
        /// The fallback marker_arrow.nif is an opaque white debug mesh with a
        /// BSLightingShaderProperty; scaled to beam length it reads as a blinding
        /// white wall. The custom GazeBeam.nif carries a BSEffectShaderProperty
        /// whose baked gold colour may also need to track the INI colour/opacity.
        ///
        /// For BSEffectShaderProperty: set baseColor + baseColorScale directly.
        /// For BSLightingShaderProperty (fallback arrow): zero the emissive colour,
        /// kill specular, and drop materialAlpha so the arrow renders as a dim
        /// gold ghost instead of a floodlight.
        ///
        /// Returns true if any shader property was adjusted.
        bool TintBeamGeometry(RE::NiNode *a_root,
                              const VisualTuning &a_tuning) noexcept
        {
            if (!a_root)
            {
                return false;
            }

            bool adjusted = false;
            const float alpha = std::clamp(a_tuning.gazeRayOpacity, 0.0f, 1.0f);

            // Walk the cloned subtree; the geometry may be the root itself or a child.
            std::vector<RE::NiPointer<RE::NiAVObject>> stack;
            stack.push_back(RE::NiPointer<RE::NiAVObject>(a_root));
            while (!stack.empty())
            {
                RE::NiPointer<RE::NiAVObject> obj = stack.back();
                stack.pop_back();

                if (auto *geo = obj->AsGeometry())
                {
                    auto *shaderProp = geo->GetGeometryRuntimeData().shaderProperty.get();
                    if (auto *effect = netimmerse_cast<RE::BSEffectShaderProperty *>(shaderProp))
                    {
                        if (auto *material = effect->GetMaterial())
                        {
                            material->baseColor = RE::NiColorA(a_tuning.ColourR(),
                                                               a_tuning.ColourG(),
                                                               a_tuning.ColourB(),
                                                               alpha);
                            material->baseColorScale = 1.0f;
                            adjusted = true;
                        }
                    }
                    else if (auto *lighting = netimmerse_cast<RE::BSLightingShaderProperty *>(shaderProp))
                    {
                        // Kill the white floodlight look: no emissive, no specular,
                        // alpha from the INI. The arrow keeps its shape but stops
                        // blinding the player.
                        if (lighting->emissiveColor)
                        {
                            *lighting->emissiveColor = RE::NiColor(0.0f, 0.0f, 0.0f);
                        }
                        lighting->emissiveMult = 0.0f;
                        if (auto *material = static_cast<RE::BSLightingShaderMaterialBase *>(lighting->GetBaseMaterial()))
                        {
                            material->specularColor = RE::NiColor(0.0f, 0.0f, 0.0f);
                            material->specularColorScale = 0.0f;
                            material->materialAlpha = alpha;
                            adjusted = true;
                        }
                    }
                }

                if (auto *node = obj->AsNode())
                {
                    for (auto &child : node->GetChildren())
                    {
                        if (child)
                        {
                            stack.push_back(child);
                        }
                    }
                }
            }
            return adjusted;
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
        // orphaned lights or panels are left glowing in the world.
        if (!_tuning.enableInGameVisuals || (!_tuning.gazeRaysEnabled && !_tuning.showHcepPanel))
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
        if (!_tuning.enableInGameVisuals || (!_tuning.gazeRaysEnabled && !_tuning.showHcepPanel))
        {
            RemoveActor(formId);
            return;
        }

        // Eligibility filters for gaze rays and HCEP panel.
        bool allowRays = _tuning.gazeRaysEnabled;
        if (a_isPlayer && !_tuning.gazeRaysOnPlayer)
            allowRays = false;
        if (!a_isPlayer && a_isHumanoid && !_tuning.gazeRaysOnNPCs)
            allowRays = false;
        if (!a_isPlayer && !a_isHumanoid && !_tuning.gazeRaysOnCreatures)
            allowRays = false;

        bool allowPanel = _tuning.showHcepPanel;
        if (!_tuning.hcepPanelAllActors && !a_isPlayer)
            allowPanel = false;

        if (!allowRays && !allowPanel)
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
        if (_updateCalls <= 3)
        {
            logger::info("[TrueGaze] Visual UpdateActor entered for {:08X}: anchor={} allowRays={} allowPanel={}",
                         formId, anchor ? "yes" : "null", allowRays, allowPanel);
        }

        auto it = _emitters.find(formId);
        if (it == _emitters.end())
        {
            if (_emitters.size() >= kMaxEmitterActors)
            {
                // Bound the emitter count independently of the runtime actor cap so a
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
            if (_updateCalls <= 3)
            {
                logger::info("[TrueGaze] Visual: attaching emitters to anchor for {:08X}", formId);
            }
            DetachAll(emitters);
            AttachEmitters(emitters, anchor);
        }

        const bool wantLight = allowRays && _tuning.UseLightEmitters();

        if (allowRays && _tuning.UseGeometry())
        {
            if (_updateCalls <= 3)
            {
                logger::info("[TrueGaze] Visual: calling EnsureBeamGeometry for {:08X}", formId);
            }
            EnsureBeamGeometry(emitters, anchor);
        }
        else if (!allowRays && emitters.geometry)
        {
            auto *node = emitters.parent.get() ? emitters.parent->AsNode() : nullptr;
            if (node)
            {
                node->DetachChild(emitters.geometry.get());
            }
            emitters.geometry = nullptr;
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
            // Lights disabled or GeometryOnly: retire any light emitters.
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

        if (allowRays && emitters.geometry)
        {
            // The beam model is authored along local +Y. Reposition and
            // orient its root every update so it visibly follows the solved gaze.
            const RE::NiPoint3 localDirection = anchorInverse.rotate * dirWorld;

            emitters.geometry->local.translate = localPupil;

            if (emitters.usingFallbackMesh)
            {
                // marker_arrow.nif is a 3D debug marker (~35 units long) with fins
                // and an opaque white material. Non-uniform matrix scaling distorts
                // it badly. Instead, use UNIFORM local.scale to shrink the entire
                // mesh to a small pencil-sized indicator, and let BeamTransform
                // handle rotation only (unit-length columns).
                //
                // Desired visual length: ~LengthUnits (175 units at 2.5m).
                // Arrow native length: ~35 units.
                // Uniform scale = desired_length / native_length.
                // This makes the arrow proportionally correct (thin enough at
                // its native aspect ratio) while being the right reach.
                static constexpr float kArrowNativeLength = 35.0f;
                float uniformScale = _tuning.LengthUnits() / kArrowNativeLength;
                // Cap the scale so it doesn't become absurdly large
                uniformScale = std::clamp(uniformScale, 0.1f, 10.0f);

                // Rotation only (unit-length basis vectors)
                emitters.geometry->local.rotate = BeamTransform(localDirection, 1.0f, 1.0f);
                emitters.geometry->local.scale = uniformScale;
            }
            else
            {
                // Proper beam NIF (unit cylinder): encode cross-section and length
                // directly into the rotation matrix columns as non-uniform scale.
                float beamThickness = _tuning.ThicknessUnits();
                float beamLength = _tuning.LengthUnits();
                emitters.geometry->local.rotate = BeamTransform(localDirection,
                                                                beamThickness,
                                                                beamLength);
                emitters.geometry->local.scale = 1.0f;
            }

            RE::NiUpdateData updateData;
            updateData.time = 0.0f;
            updateData.flags = RE::NiUpdateData::Flag::kDirty;
            emitters.geometry->world = anchor->world * emitters.geometry->local;
            emitters.geometry->UpdateDownwardPass(updateData, 0);
        }

        // --- HCEP Floating Diagram Panel ---------------------------------------
        if (allowPanel)
        {
            if (_updateCalls <= 3)
            {
                logger::info("[TrueGaze] Visual: calling EnsureHcepPanel for {:08X}", formId);
            }
            EnsureHcepPanel(emitters, anchor);
            UpdateHcepPanel(emitters, anchor, a_gazeRegion);
        }
        else if (emitters.hcepPanel)
        {
            DetachPanel(emitters);
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

        if (_lightsCreated < 4)
        {
            logger::info("[TrueGaze] EnsureLight: creating {} light (anchor node children={})",
                         a_pupil ? "pupil" : "terminus",
                         node->GetChildren().size());
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
        // landing point without competing with the source. Both radii are fixed
        // constants — a length-proportional radius produced floodlight-sized glows.
        const float radius = a_pupil ? kPupilGlowRadius : kTerminusGlowRadius;
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
        DetachPanel(a_emitters);

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

        logger::info("[TrueGaze] EnsureBeamGeometry: attempting to load '{}' (attempt {})",
                     _tuning.beamModelPath, a_emitters.geometryAttempts);

        RE::NiPointer<RE::NiNode> model;
        RE::BSModelDB::DBTraits::ArgsType args;
        RE::BSResource::ErrorCode result = RE::BSResource::ErrorCode::kInvalidPath;
        const char *effectivePath = _tuning.beamModelPath;

        try
        {
            result = RE::BSModelDB::Demand(_tuning.beamModelPath, model, args);

            // If the primary standalone mesh path is missing, try the secondary fallback path
            if ((result != RE::BSResource::ErrorCode::kNone || !model) && _tuning.beamModelFallbackPath)
            {
                result = RE::BSModelDB::Demand(_tuning.beamModelFallbackPath, model, args);
                if (result == RE::BSResource::ErrorCode::kNone && model)
                {
                    effectivePath = _tuning.beamModelFallbackPath;
                }
            }
        }
        catch (const std::exception &e)
        {
            logger::error("[TrueGaze] Exception demanding beam model: {}", e.what());
            result = RE::BSResource::ErrorCode::kInvalidPath;
            model = nullptr;
        }
        catch (...)
        {
            logger::error("[TrueGaze] Unknown exception demanding beam model.");
            result = RE::BSResource::ErrorCode::kInvalidPath;
            model = nullptr;
        }

        if (result != RE::BSResource::ErrorCode::kNone || !model)
        {
            a_emitters.geometryState = result == RE::BSResource::ErrorCode::kNotExist
                                           ? ActorEmitters::GeometryState::Missing
                                           : ActorEmitters::GeometryState::Invalid;
            if (a_emitters.geometryAttempts == 1 || result != RE::BSResource::ErrorCode::kNotExist)
            {
                logger::info("[TrueGaze] Beam geometry not found ('{}'); utilizing verified NiPointLight emitter fallback.",
                             _tuning.beamModelPath);
            }
            // Ensure verified light emitter fallback is active when geometry is absent
            EnsureLight(a_emitters, a_anchor, true);
            return;
        }

        auto clonedObject = model->Clone();
        auto *clonedNode = clonedObject ? clonedObject->AsNode() : nullptr;
        if (!clonedNode)
        {
            logger::warn("[TrueGaze] Failed to clone beam model '{}'.", effectivePath);
            return;
        }

        a_anchor->AsNode()->AttachChild(clonedNode, false);
        a_emitters.geometry.reset(clonedNode);
        a_emitters.geometryState = ActorEmitters::GeometryState::Loaded;
        // The arrow mesh is NOT unit-sized (~35u long, ~2u radius) and needs
        // dimension compensation in BeamTransform regardless of which slot it
        // was loaded from (primary or fallback).
        a_emitters.usingFallbackMesh =
            (effectivePath == _tuning.beamModelFallbackPath) ||
            (std::strstr(effectivePath, "marker_arrow") != nullptr);
        ++_geometryCreated;

        // Recolour the freshly cloned geometry so the fallback arrow stops
        // rendering as a blinding white debug marker and the custom beam NIF
        // tracks the INI colour/opacity without a rebuild.
        const bool tinted = TintBeamGeometry(clonedNode, _tuning);
        logger::info("[TrueGaze] Visible beam geometry attached from '{}'{} and will follow solved gaze{}",
                     effectivePath,
                     a_emitters.usingFallbackMesh ? " (fallback)" : "",
                     tinted ? " (shader tinted)" : "");
    }

    void VisualEffectsManager::EnsureHcepPanel(ActorEmitters &a_emitters,
                                               RE::NiAVObject *a_anchor) noexcept
    {
        if (a_emitters.hcepPanel || !a_anchor || !a_anchor->AsNode())
        {
            return;
        }

        const uint64_t retryFrames = a_emitters.panelState == ActorEmitters::GeometryState::Missing
                                         ? 300
                                         : 30;
        if (a_emitters.panelAttempts > 0 &&
            _frameCounter - a_emitters.lastPanelAttemptFrame < retryFrames)
        {
            return;
        }

        ++a_emitters.panelAttempts;
        a_emitters.lastPanelAttemptFrame = _frameCounter;
        a_emitters.panelState = ActorEmitters::GeometryState::Pending;

        logger::info("[TrueGaze] EnsureHcepPanel: attempting to load '{}'",
                     _tuning.hcepPanelModelPath);

        RE::NiPointer<RE::NiNode> model;
        RE::BSModelDB::DBTraits::ArgsType args;
        RE::BSResource::ErrorCode result = RE::BSResource::ErrorCode::kInvalidPath;

        try
        {
            result = RE::BSModelDB::Demand(_tuning.hcepPanelModelPath, model, args);
        }
        catch (const std::exception &e)
        {
            logger::error("[TrueGaze] Exception demanding HCEP panel model: {}", e.what());
            result = RE::BSResource::ErrorCode::kInvalidPath;
            model = nullptr;
        }
        catch (...)
        {
            logger::error("[TrueGaze] Unknown exception demanding HCEP panel model.");
            result = RE::BSResource::ErrorCode::kInvalidPath;
            model = nullptr;
        }

        if (result != RE::BSResource::ErrorCode::kNone || !model)
        {
            a_emitters.panelState = result == RE::BSResource::ErrorCode::kNotExist
                                        ? ActorEmitters::GeometryState::Missing
                                        : ActorEmitters::GeometryState::Invalid;
            if (a_emitters.panelAttempts == 1)
            {
                logger::info("[TrueGaze] HCEP panel geometry not found ('{}'); will retry.",
                             _tuning.hcepPanelModelPath);
            }
            return;
        }

        auto clonedObject = model->Clone();
        auto *clonedNode = clonedObject ? clonedObject->AsNode() : nullptr;
        if (!clonedNode)
        {
            logger::warn("[TrueGaze] Failed to clone HCEP panel model '{}'.", _tuning.hcepPanelModelPath);
            return;
        }

        a_anchor->AsNode()->AttachChild(clonedNode, false);
        a_emitters.hcepPanel.reset(clonedNode);
        a_emitters.panelState = ActorEmitters::GeometryState::Loaded;
        logger::info("[TrueGaze] Attached HCEP floating diagram panel from '{}'.",
                     _tuning.hcepPanelModelPath);
    }

    void VisualEffectsManager::UpdateHcepPanel(ActorEmitters &a_emitters,
                                               RE::NiAVObject *a_anchor,
                                               uint8_t a_gazeRegion) noexcept
    {
        if (!a_emitters.hcepPanel || !a_anchor)
        {
            return;
        }

        try
        {
            // Panel floats directly in front of the head at HcepPanelForwardOffsetUnits (~24.5 units)
            // Mesh is authored in XZ plane (vertical billboard), normal facing +Y (toward viewer).
            // Translate along head local +Y (forward), no additional rotation needed.
            a_emitters.hcepPanel->local.translate = RE::NiPoint3{
                0.0f,
                _tuning.HcepPanelForwardOffsetUnits(),
                0.0f};
            a_emitters.hcepPanel->local.rotate = RE::NiMatrix3();
            a_emitters.hcepPanel->local.scale = _tuning.hcepPanelScale;

            RE::NiUpdateData updateData;
            updateData.time = 0.0f;
            updateData.flags = RE::NiUpdateData::Flag::kDirty;
            a_emitters.hcepPanel->world = a_anchor->world * a_emitters.hcepPanel->local;
            a_emitters.hcepPanel->UpdateDownwardPass(updateData, 0);

            // Active region highlight
            if (a_emitters.lastGazeRegion != a_gazeRegion)
            {
                a_emitters.lastGazeRegion = a_gazeRegion;
                auto *node = a_emitters.hcepPanel->AsNode();
                if (node && !node->GetChildren().empty())
                {
                    auto &child = node->GetChildren().front();
                    if (child)
                    {
                        auto *geo = child->AsGeometry();
                        if (geo)
                        {
                            auto *shaderProp = geo->GetGeometryRuntimeData().shaderProperty.get();
                            auto *effectShader = netimmerse_cast<RE::BSEffectShaderProperty *>(shaderProp);
                            if (effectShader)
                            {
                                auto *material = effectShader->GetMaterial();
                                if (material)
                                {
                                    float r = 1.0f, g = 1.0f, b = 1.0f;
                                    switch (a_gazeRegion)
                                    {
                                    case 0:
                                        r = 0.85f;
                                        g = 0.40f;
                                        b = 1.00f;
                                        break; // Third-eye: purple/violet
                                    case 1:
                                        r = 0.20f;
                                        g = 0.80f;
                                        b = 1.00f;
                                        break; // Upper: cyan
                                    case 2:
                                        r = 1.00f;
                                        g = 0.60f;
                                        b = 0.15f;
                                        break; // Right eye: orange
                                    case 3:
                                        r = 0.20f;
                                        g = 1.00f;
                                        b = 0.35f;
                                        break; // Left eye: green
                                    case 4:
                                        r = 1.00f;
                                        g = 0.35f;
                                        b = 0.75f;
                                        break; // Mouth: pink
                                    case 5:
                                        r = 1.00f;
                                        g = 0.15f;
                                        b = 0.20f;
                                        break; // Chest: crimson red
                                    case 6:
                                        r = 0.40f;
                                        g = 0.95f;
                                        b = 1.00f;
                                        break; // Far Upper: bright cyan
                                    case 7:
                                        r = 0.65f;
                                        g = 0.70f;
                                        b = 0.80f;
                                        break; // Far Lower: silver
                                    case 8:
                                        r = 0.15f;
                                        g = 0.35f;
                                        b = 0.90f;
                                        break; // Lower: blue
                                    default:
                                        r = 1.00f;
                                        g = 1.00f;
                                        b = 1.00f;
                                        break;
                                    }
                                    material->baseColor = RE::NiColorA(r, g, b, 1.0f);
                                    material->baseColorScale = 1.3f;
                                }
                            }
                        }
                    }
                }
            }
        }
        catch (...)
        {
            // Panel update failure must be non-fatal
        }
    }

    void VisualEffectsManager::DetachPanel(ActorEmitters &a_emitters) noexcept
    {
        auto *node = a_emitters.parent.get() ? a_emitters.parent->AsNode() : nullptr;
        if (node && a_emitters.hcepPanel)
        {
            node->DetachChild(a_emitters.hcepPanel.get());
        }
        a_emitters.hcepPanel = nullptr;
    }

#else

    void VisualEffectsManager::EnsureHcepPanel(ActorEmitters &, RE::NiAVObject *) noexcept {}
    void VisualEffectsManager::UpdateHcepPanel(ActorEmitters &, RE::NiAVObject *, uint8_t) noexcept {}
    void VisualEffectsManager::DetachPanel(ActorEmitters &) noexcept {}
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