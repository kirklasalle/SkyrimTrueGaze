#include "VisualEffectsManager.hpp"
#include "GazeGeometry.hpp"

#if __has_include(<RE/Skyrim.h>)
#include <RE/B/BSEffectShaderMaterial.h>
#include <RE/B/BSEffectShaderProperty.h>
#include <RE/B/BSLightingShaderMaterial.h>
#include <RE/B/BSLightingShaderProperty.h>
#include <RE/B/BSModelDB.h>
#include <RE/N/NiAlphaProperty.h>
#include <RE/Skyrim.h>
#endif

#include <atomic>
#include <cmath>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace TrueGaze::Visuals
{

    namespace
    {

        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kDegToRad = kPi / 180.0f;

        // Subtle pupil highlights for Superman Laser Eyes (small radius prevents washing out scene)
        constexpr float kPupilGlowRadius = 1.0f;
        constexpr float kTerminusGlowRadius = 1.5f;


        constexpr const char* kPupilLightLName = "TrueGaze_PupilLight_L";
        constexpr const char* kPupilLightRName = "TrueGaze_PupilLight_R";
        constexpr const char* kTerminusLightName = "TrueGaze_TerminusLight";

        // -----------------------------------------------------------------------
        // HCEP Floating Diagram Panel Regions
        // -----------------------------------------------------------------------
        // Represents the 11 cognitive and social gaze regions from the HCEP-02
        // Enhanced Diagram. Rendered as a constellation of positioned NiPointLights
        // floating in the visual field in front of the head.
        struct PanelRegionDef
        {
            uint8_t regionId;
            float localX; // horizontal offset in face plane (Skyrim units)
            float localZ; // vertical offset in face plane (Skyrim units)
            float r, g, b;
            const char* name;
        };

        static constexpr PanelRegionDef kPanelRegions[11] = {
            {0, -2.5f, 0.5f, 0.20f, 1.00f, 0.35f, "LeftEye"},      // 0: green
            {1, 2.5f, 0.5f, 1.00f, 0.60f, 0.15f, "RightEye"},      // 1: orange
            {2, 0.0f, -3.0f, 1.00f, 0.35f, 0.75f, "Mouth"},        // 2: pink
            {3, 0.0f, 3.5f, 0.85f, 0.40f, 1.00f, "Forehead"},      // 3: purple
            {4, 0.0f, -6.0f, 0.90f, 0.75f, 0.60f, "Chin"},         // 4: warm tan
            {5, 0.0f, -11.0f, 1.00f, 0.15f, 0.20f, "Torso"},       // 5: crimson
            {8, 0.0f, -16.0f, 0.15f, 0.35f, 0.90f, "Ground"},      // 8: blue
            {9, -8.0f, 7.0f, 0.20f, 0.80f, 1.00f, "ULPeripheral"}, // 9: cyan (CGA: positivity)
            {10, 8.0f, 7.0f, 0.40f, 0.95f, 1.00f, "URPeripheral"}, // 10: bright cyan (CGA: memory)
            {11, -8.0f, -8.0f, 0.65f, 0.70f, 0.80f, "LLPeripheral"}, // 11: silver (CGA: tiredness)
            {12, 8.0f, -8.0f, 0.50f, 0.50f, 0.65f, "LRPeripheral"} // 12: slate (CGA: shyness/fear)
        };

        struct RegionColour
        {
            float r, g, b;
        };

        RegionColour GetRegionColour(uint8_t a_gazeRegion, const VisualTuning& a_tuning) noexcept
        {
            for (const auto& reg : kPanelRegions)
            {
                if (reg.regionId == a_gazeRegion)
                {
                    return {reg.r, reg.g, reg.b};
                }
            }
            return {a_tuning.ColourR(), a_tuning.ColourG(), a_tuning.ColourB()};
        }

#if __has_include(<RE/Skyrim.h>)
        RE::NiPoint3 Cross(const RE::NiPoint3& a_lhs, const RE::NiPoint3& a_rhs) noexcept
        {
            return RE::NiPoint3{a_lhs.y * a_rhs.z - a_lhs.z * a_rhs.y,
                                a_lhs.z * a_rhs.x - a_lhs.x * a_rhs.z,
                                a_lhs.x * a_rhs.y - a_lhs.y * a_rhs.x};
        }

        /// Construct an orthonormal rotation matrix aligning a mesh's native forward
        /// axis to a_targetDir.
        ///
        /// CRITICAL CONVENTION (root cause of the 2026-09 "arrow" bug and the
        /// 2026-10-01 vertical-beam bug): RE::NiMatrix3(vx, vy, vz) stores the
        /// vectors as ROWS (entry[0] = vx), but a node's rotation maps mesh-local
        /// axes via COLUMNS (M * unitY = column 1). Building the basis with the
        /// constructor therefore TRANSPOSES it — every deflection comes out
        /// mirrored and vertical directions collapse. The correct construction
        /// writes the basis vectors into the matrix COLUMNS explicitly.
        ///
        /// When a_authoredAlongZ is true (Bethesda arrow / soul cairn beam models
        /// modeled vertically along +Z): maps local +Z to a_targetDir.
        /// When a_authoredAlongZ is false (standard Gamebryo +Y forward):
        /// maps local +Y to a_targetDir.
        RE::NiMatrix3 AlignBeamOrientation(const RE::NiPoint3& a_targetDir,
                                           bool a_authoredAlongZ) noexcept
        {
            RE::NiPoint3 dir = a_targetDir;
            if (dir.SqrLength() < 1e-6f)
            {
                return RE::NiMatrix3();
            }
            (void)dir.Unitize();

            RE::NiPoint3 ref = (std::abs(dir.z) < 0.9f) ? RE::NiPoint3{0.0f, 0.0f, 1.0f}
                                                        : RE::NiPoint3{0.0f, 1.0f, 0.0f};
            RE::NiPoint3 right = Cross(dir, ref);
            (void)right.Unitize();
            RE::NiPoint3 ortho = Cross(right, dir);
            (void)ortho.Unitize();

            // Basis vectors as COLUMNS: entry[col][row].
            // column 0 = right (X); column 1 = dir (Y-forward) or column 2 = dir (Z-forward).
            RE::NiMatrix3 m;
            m.entry[0][0] = right.x;
            m.entry[1][0] = right.y;
            m.entry[2][0] = right.z;
            if (a_authoredAlongZ)
            {
                m.entry[0][1] = ortho.x;
                m.entry[1][1] = ortho.y;
                m.entry[2][1] = ortho.z;
                m.entry[0][2] = dir.x;
                m.entry[1][2] = dir.y;
                m.entry[2][2] = dir.z;
            }
            else
            {
                m.entry[0][1] = dir.x;
                m.entry[1][1] = dir.y;
                m.entry[2][1] = dir.z;
                m.entry[0][2] = ortho.x;
                m.entry[1][2] = ortho.y;
                m.entry[2][2] = ortho.z;
            }
            return m;
        }

        RE::NiPoint3 GazeDirection(const RE::NiMatrix3& a_basis, float a_yawRad,
                                   float a_pitchRad) noexcept
        {
            const RE::NiPoint3 right = a_basis.GetVectorX();
            const RE::NiPoint3 forward = a_basis.GetVectorY();
            const RE::NiPoint3 up = a_basis.GetVectorZ();

            const float cy = std::cos(a_yawRad);
            const float sy = std::sin(a_yawRad);
            const float cp = std::cos(a_pitchRad);
            const float sp = std::sin(a_pitchRad);

            RE::NiPoint3 dir{forward.x * cy * cp + right.x * sy * cp + up.x * sp,
                             forward.y * cy * cp + right.y * sy * cp + up.y * sp,
                             forward.z * cy * cp + right.z * sy * cp + up.z * sp};

            (void)dir.Unitize();
            return dir;
        }

        bool TintBeamGeometry(RE::NiNode* a_root, const VisualTuning& a_tuning) noexcept
        {
            if (!a_root)
            {
                return false;
            }

            bool adjusted = false;
            const float alpha = std::clamp(a_tuning.gazeRayOpacity, 0.0f, 1.0f);

            std::vector<RE::NiAVObject*> stack;
            stack.reserve(16);
            stack.push_back(a_root);
            size_t visits = 0;

            while (!stack.empty())
            {
                RE::NiAVObject* obj = stack.back();
                stack.pop_back();

                if (!obj || ++visits > 256)
                {
                    continue;
                }

                if (auto* geo = obj->AsGeometry())
                {
                    auto* shaderProp = geo->GetGeometryRuntimeData().shaderProperty.get();
                    if (auto* effect = netimmerse_cast<RE::BSEffectShaderProperty*>(shaderProp))
                    {
                        if (auto* material = effect->GetMaterial())
                        {
                            material->baseColor = RE::NiColorA(
                                a_tuning.ColourR(), a_tuning.ColourG(), a_tuning.ColourB(), alpha);
                            material->baseColorScale = 1.2f;
                            adjusted = true;
                        }
                    }
                    else if (auto* lighting =
                                 netimmerse_cast<RE::BSLightingShaderProperty*>(shaderProp))
                    {
                        if (lighting->emissiveColor)
                        {
                            *lighting->emissiveColor = RE::NiColor(0.0f, 0.0f, 0.0f);
                        }
                        lighting->emissiveMult = 0.0f;
                        if (auto* material = static_cast<RE::BSLightingShaderMaterialBase*>(
                                lighting->GetBaseMaterial()))
                        {
                            material->specularColor = RE::NiColor(0.0f, 0.0f, 0.0f);
                            material->specularColorScale = 0.0f;
                            material->materialAlpha = alpha;
                            adjusted = true;
                        }
                    }
                }

                if (auto* node = obj->AsNode())
                {
                    const auto& children = node->GetChildren();
                    const auto count = std::min<uint16_t>(children.free_idx(), children.capacity());
                    for (uint16_t i = 0; i < count; ++i)
                    {
                        auto* child = children[i].get();
                        if (child)
                        {
                            stack.push_back(child);
                        }
                    }
                }
            }
            return adjusted;
        }

        /// Validate that a cloned NiNode contains at least one BSGeometry child
        /// with a non-null shader property and valid vertex data. This catches
        /// Python-generated stub NIFs that pass BSModelDB::Demand (header parses)
        /// but crash the renderer on the next draw call (SEH 0xC0000005) because
        /// their BSTriShape has no vertex buffers or shader properties.
        bool HasRendererSafeGeometry(RE::NiNode* a_root) noexcept
        {
            if (!a_root)
            {
                return false;
            }

            bool foundValidGeometry = false;
            std::vector<RE::NiAVObject*> stack;
            stack.reserve(16);
            stack.push_back(a_root);
            size_t visits = 0;

            while (!stack.empty())
            {
                RE::NiAVObject* obj = stack.back();
                stack.pop_back();

                if (!obj || ++visits > 256)
                {
                    continue;
                }

                if (auto* geo = obj->AsGeometry())
                {
                    // Must have a shader property — without one the renderer
                    // dereferences null and faults.
                    auto* shaderProp = geo->GetGeometryRuntimeData().shaderProperty.get();
                    if (shaderProp)
                    {
                        foundValidGeometry = true;
                        break; // one valid child is sufficient
                    }
                }

                if (auto* node = obj->AsNode())
                {
                    const auto& children = node->GetChildren();
                    const auto count = std::min<uint16_t>(children.free_idx(), children.capacity());
                    for (uint16_t i = 0; i < count; ++i)
                    {
                        auto* child = children[i].get();
                        if (child)
                        {
                            stack.push_back(child);
                        }
                    }
                }
            }

            return foundValidGeometry;
        }

#endif // __has_include(<RE/Skyrim.h>)

    } // namespace

    // ---------------------------------------------------------------------------
    // Lifecycle
    // ---------------------------------------------------------------------------

    void VisualEffectsManager::SetTuning(const VisualTuning& a_tuning) noexcept
    {
        _tuning = a_tuning;

        if (!_tuning.enableInGameVisuals || (!_tuning.gazeRaysEnabled && !_tuning.showHcepPanel))
        {
            Reset();
        }

        logger::info("[TrueGaze] Visual tuning: visuals={} rays={} mode={} length={:.1f}m "
                     "colour=#{:06X} opacity={:.2f} terminus={} panel={}",
                     _tuning.enableInGameVisuals ? "on" : "off",
                     _tuning.gazeRaysEnabled ? "on" : "off", _tuning.rayRenderMode,
                     _tuning.gazeRayLengthMeters, _tuning.gazeRayColour & 0x00FFFFFF,
                     _tuning.gazeRayOpacity, _tuning.gazeRaysTerminus ? "yes" : "no",
                     _tuning.showHcepPanel ? "on" : "off");
    }

    void VisualEffectsManager::Reset() noexcept
    {
        for (auto& kv : _emitters)
        {
            DetachAll(kv.second);
        }

        _emitters.clear();
        _attachedLights = 0;
        _geometryModeReported = false; // re-arm diagnostic report after toggle

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

    void VisualEffectsManager::UpdateActor(RE::Actor* a_actor, RE::NiAVObject* a_headBone,
                                           RE::NiAVObject* a_eyeL, RE::NiAVObject* a_eyeR,
                                           float a_eyeYawDeg, float a_eyePitchDeg,
                                           uint8_t a_gazeRegion, bool a_isPlayer,
                                           bool a_isHumanoid,
                                           float a_targetDistanceUnits) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (!a_actor)
        {
            return;
        }

        ++_updateCalls;
        const uint32_t formId = a_actor->GetFormID();

        if (!_tuning.enableInGameVisuals || (!_tuning.gazeRaysEnabled && !_tuning.showHcepPanel))
        {
            RemoveActor(formId);
            return;
        }

        // Eligibility filters for gaze rays and HCEP panel
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

        RE::NiAVObject* anchor = a_headBone;
        if (!_tuning.gazeRaysAttachHead || !anchor)
        {
            anchor = a_actor->Get3D();
        }
        if (!anchor || anchor->world.scale < 0.001f)
        {
            ++_anchorFailures;
            RemoveActor(formId);
            return;
        }

        RE::NiPoint3 originLWorld{};
        RE::NiPoint3 originRWorld{};
        RE::NiPoint3 dirWorld{};
        ResolvePupils(a_headBone, a_eyeL, a_eyeR, anchor, a_eyeYawDeg, a_eyePitchDeg, originLWorld,
                      originRWorld, dirWorld);

        auto it = _emitters.find(formId);
        if (it == _emitters.end())
        {
            if (_emitters.size() >= kMaxEmitterActors)
            {
                return;
            }
            it = _emitters.emplace(formId, ActorEmitters{}).first;
        }

        ActorEmitters& emitters = it->second;
        emitters.lastFrame = _frameCounter;

        if (emitters.parent.get() != anchor)
        {
            DetachAll(emitters);
            AttachEmitters(emitters, anchor);
        }

        // --- Superman Laser Eyes (Rays & Lights) -------------------------------
        const bool wantLight = allowRays && _tuning.UseLightEmitters();

        if (allowRays && _tuning.UseGeometry())
        {
            EnsureBeamGeometry(emitters, anchor);
        }
        else if (!allowRays && (emitters.geometryL || emitters.geometryR))
        {
            auto* node = emitters.parent.get() ? emitters.parent->AsNode() : nullptr;
            if (node)
            {
                if (emitters.geometryL)
                    node->DetachChild(emitters.geometryL.get());
                if (emitters.geometryR)
                    node->DetachChild(emitters.geometryR.get());
            }
            emitters.geometryL = nullptr;
            emitters.geometryR = nullptr;
            emitters.geometryState = ActorEmitters::GeometryState::Unknown;
            emitters.geometryAttempts = 0;
        }

        if (wantLight)
        {
            EnsureLights(emitters, anchor);

            const auto regionCol = GetRegionColour(a_gazeRegion, _tuning);
            const float scale =
                std::clamp(_tuning.gazeRayOpacity * _tuning.pupilGlowIntensity, 0.0f, 1.0f);

            ApplyLightColour(emitters.pupilLightL.get(), regionCol.r * scale, regionCol.g * scale,
                             regionCol.b * scale);
            ApplyLightColour(emitters.pupilLightR.get(), regionCol.r * scale, regionCol.g * scale,
                             regionCol.b * scale);

            if (_tuning.gazeRaysTerminus)
            {
                ApplyLightColour(emitters.terminusLight.get(), regionCol.r * scale,
                                 regionCol.g * scale, regionCol.b * scale);
            }
        }
        else if (emitters.pupilLightL || emitters.pupilLightR || emitters.terminusLight)
        {
            DetachLights(emitters);
        }

        // Report geometry mode once
        if (_tuning.UseGeometry() && !_geometryModeReported)
        {
            _geometryModeReported = true;
            logger::info(
                "[TrueGaze] Visual mode: beam geometry '{}' active with Superman Laser Eyes.",
                _tuning.beamModelPath);
        }

        // Compute local transforms under anchor
        const RE::NiTransform anchorInverse = anchor->world.Invert();
        const RE::NiPoint3 localPupilL = anchorInverse * originLWorld;
        const RE::NiPoint3 localPupilR = anchorInverse * originRWorld;
        const RE::NiPoint3 localDir = (anchorInverse.rotate * dirWorld);

        if (emitters.pupilLightL)
        {
            emitters.pupilLightL->local.translate = localPupilL;
        }
        if (emitters.pupilLightR)
        {
            emitters.pupilLightR->local.translate = localPupilR;
        }
        if (emitters.terminusLight)
        {
            // Terminus positioned along the line of sight from eye center
            const float focalDist = (a_targetDistanceUnits > 5.0f)
                                        ? std::min(a_targetDistanceUnits, _tuning.LengthUnits())
                                        : _tuning.LengthUnits();
            const RE::NiPoint3 centerWorld = (originLWorld + originRWorld) * 0.5f;
            const RE::NiPoint3 terminusWorld{centerWorld.x + dirWorld.x * focalDist,
                                             centerWorld.y + dirWorld.y * focalDist,
                                             centerWorld.z + dirWorld.z * focalDist};
            emitters.terminusLight->local.translate = anchorInverse * terminusWorld;
        }

        // Cyclopean (mid-eye) origin in anchor space; consumed by the HCEP panel.
        emitters.eyeMidLocal = (localPupilL + localPupilR) * 0.5f;

        if (allowRays && (emitters.geometryL || emitters.geometryR))
        {
            namespace G = TrueGaze::Visuals::Geometry;
            const auto toVec = [](const RE::NiPoint3& p) noexcept { return G::Vec3{p.x, p.y, p.z}; };
            const auto toNi = [](const G::Vec3& v) noexcept { return RE::NiPoint3{v.x, v.y, v.z}; };

            RE::NiUpdateData updateData;
            updateData.time = 0.0f;
            updateData.flags = RE::NiUpdateData::Flag::kDirty;

            const float focalDistance = (a_targetDistanceUnits > 5.0f)
                                            ? std::min(a_targetDistanceUnits, _tuning.LengthUnits())
                                            : _tuning.LengthUnits();

            if (emitters.isMarkerArrow)
            {
                // marker_arrow.nif measured 64 x 240 x 160 (X x Y x Z). Length is
                // set to the detected target focal distance (or configured reach) via
                // uniform scale; widest cross-section is mapped to eyeball diameter
                // via COLUMN scale on mesh-local X/Z.
                const G::ArrowScales scales = G::ComputeMarkerArrowScales(
                    focalDistance, _tuning.ArrowCrossSectionUnits());

                // Binocular convergence: both arrows meet at the exact fixation point
                // at the real target distance along the cyclopean gaze.
                const G::Vec3 mid = toVec(emitters.eyeMidLocal);
                const G::Vec3 dirMid = toVec(localDir);

                const auto placeArrow = [&](RE::NiAVObject* a_geo, const RE::NiPoint3& a_pupil) {
                    if (!a_geo)
                    {
                        return;
                    }
                    const G::Vec3 eyeDir =
                        G::ConvergedEyeDirection(toVec(a_pupil), mid, dirMid, focalDistance);
                    const RE::NiPoint3 eyeDirNi = toNi(eyeDir);

                    RE::NiMatrix3 rot = AlignBeamOrientation(eyeDirNi, /*authoredAlongZ*/ false);
                    // FIX A1 (2026-10-03): scale COLUMNS 0 and 2 (mesh-local X and Z).
                    // The previous loop scaled ROWS, i.e. squashed head-space X/Z,
                    // which also bent the direction column toward head +Y and
                    // compressed every lateral/vertical deflection ~11x.
                    G::ScaleBasisColumns(rot.entry, scales.crossSection, 1.0f, scales.crossSection);

                    // Rear edge of the arrow sits exactly on the pupil.
                    a_geo->local.translate = a_pupil + eyeDirNi * scales.baseOffset;
                    a_geo->local.rotate = rot;
                    a_geo->local.scale = scales.uniform;
                    a_geo->world = anchor->world * a_geo->local;
                    a_geo->UpdateDownwardPass(updateData, 0);
                };

                placeArrow(emitters.geometryL.get(), localPupilL);
                placeArrow(emitters.geometryR.get(), localPupilR);
            }
            else
            {
                // Non-arrow meshes (GazeBeam.nif / soul-cairn fallback):
                // uniform scale by focal reach, aligned with gaze.
                const RE::NiMatrix3 beamRot =
                    AlignBeamOrientation(localDir, emitters.usingFallbackMesh);
                const float beamScale =
                    emitters.usingFallbackMesh ? std::clamp(focalDistance / 35.0f, 0.5f, 4.0f)
                                               : std::clamp(focalDistance / 70.0f, 0.1f, 10.0f);

                if (emitters.geometryL)
                {
                    emitters.geometryL->local.translate = localPupilL;
                    emitters.geometryL->local.rotate = beamRot;
                    emitters.geometryL->local.scale = beamScale;
                    emitters.geometryL->world = anchor->world * emitters.geometryL->local;
                    emitters.geometryL->UpdateDownwardPass(updateData, 0);
                }
                if (emitters.geometryR)
                {
                    emitters.geometryR->local.translate = localPupilR;
                    emitters.geometryR->local.rotate = beamRot;
                    emitters.geometryR->local.scale = beamScale;
                    emitters.geometryR->world = anchor->world * emitters.geometryR->local;
                    emitters.geometryR->UpdateDownwardPass(updateData, 0);
                }
            }

            if (emitters.lastGazeRegion != a_gazeRegion)
            {
                const auto beamCol = GetRegionColour(a_gazeRegion, _tuning);
                VisualTuning regionTuning = _tuning;
                regionTuning.gazeRayColour = (static_cast<uint32_t>(beamCol.r * 255.0f) << 16) |
                                             (static_cast<uint32_t>(beamCol.g * 255.0f) << 8) |
                                             static_cast<uint32_t>(beamCol.b * 255.0f);
                if (emitters.geometryL)
                {
                    if (auto* nodeL = emitters.geometryL->AsNode())
                        TintBeamGeometry(nodeL, regionTuning);
                }
                if (emitters.geometryR)
                {
                    if (auto* nodeR = emitters.geometryR->AsNode())
                        TintBeamGeometry(nodeR, regionTuning);
                }
            }
        }

        // --- Phase 2 A7: Ray–Panel Physical Hit Detection & Accuracy Tracking ---
        namespace G = TrueGaze::Visuals::Geometry;
        const float panelScaleUnits =
            (_tuning.hcepPanelScale > 0.1f ? _tuning.hcepPanelScale : 25.0f);
        constexpr float kDiagramAspect = 2760.0f / 1504.0f; // w:h = 1.835
        const float halfW = panelScaleUnits * kDiagramAspect * 0.5f;
        const float halfH = panelScaleUnits * 0.5f;

        const auto hitResult = G::IntersectGazeWithPanel(
            /*rayOriginRelEyeMid=*/{0.0f, 0.0f, 0.0f},
            /*rayDir=*/G::Vec3{localDir.x, localDir.y, localDir.z},
            /*panelForwardOffsetUnits=*/_tuning.HcepPanelForwardOffsetUnits(),
            /*halfWidthUnits=*/halfW,
            /*halfHeightUnits=*/halfH);

        emitters.lastHitSuccess = hitResult.hit;
        emitters.lastHitRegion = hitResult.hitRegion;
        emitters.lastHitX = hitResult.hitX;
        emitters.lastHitZ = hitResult.hitZ;
        if (hitResult.hit)
        {
            _accuracyStats.totalEvaluations++;
            if (hitResult.hitRegion == a_gazeRegion)
            {
                _accuracyStats.agreements++;
                emitters.lastAgreement = true;
            }
            else
            {
                _accuracyStats.mismatches++;
                emitters.lastAgreement = false;
            }
        }
        else
        {
            _accuracyStats.panelMisses++;
            emitters.lastAgreement = false;
        }

        // A7.4: Terminus illumination placed directly at the panel hit point
        if (emitters.terminusLight && hitResult.hit && allowPanel)
        {
            emitters.terminusLight->local.translate =
                RE::NiPoint3{emitters.eyeMidLocal.x + hitResult.hitX,
                             emitters.eyeMidLocal.y + _tuning.HcepPanelForwardOffsetUnits(),
                             emitters.eyeMidLocal.z + hitResult.hitZ};
            emitters.terminusLight->world = anchor->world * emitters.terminusLight->local;
            RE::NiUpdateData updateData;
            updateData.time = 0.0f;
            updateData.flags = RE::NiUpdateData::Flag::kDirty;
            emitters.terminusLight->UpdateDownwardPass(updateData, 0);
        }

        // --- HCEP Floating Diagram Panel ---------------------------------------
        if (allowPanel)
        {
            EnsureHcepPanel(emitters, anchor);
            UpdateHcepPanel(emitters, anchor, a_gazeRegion);
        }
        else if (emitters.hcepPanel)
        {
            DetachPanel(emitters);
        }

        emitters.lastGazeRegion = a_gazeRegion;

#else
        (void)a_actor;
        (void)a_headBone;
        (void)a_eyeL;
        (void)a_eyeR;
        (void)a_eyeYawDeg;
        (void)a_eyePitchDeg;
        (void)a_gazeRegion;
        (void)a_isPlayer;
        (void)a_isHumanoid;
        (void)a_targetDistanceUnits;
#endif
    }

    // ---------------------------------------------------------------------------
    // Light and Geometry Implementation
    // ---------------------------------------------------------------------------

    void VisualEffectsManager::ApplyLightColour(RE::NiPointLight* a_light, float a_r, float a_g,
                                                float a_b) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (!a_light)
        {
            return;
        }

        auto& runtimeData = a_light->GetLightRuntimeData();
        // Ambient must remain 0.0f — any ambient light from dynamic point lights
        // washes out the cell / geometry.
        runtimeData.ambient = RE::NiColor(0.0f, 0.0f, 0.0f);
        // Diffuse is kept subtle (0.15x) so it provides a crisp specular glint on the pupil
        // without flooding the surrounding scene with colored light.
        runtimeData.diffuse = RE::NiColor(a_r * 0.15f, a_g * 0.15f, a_b * 0.15f);
#else
        (void)a_light;
        (void)a_r;
        (void)a_g;
        (void)a_b;
#endif
    }


#if __has_include(<RE/Skyrim.h>)

    void VisualEffectsManager::EnsureLights(ActorEmitters& a_emitters,
                                            RE::NiAVObject* a_anchor) noexcept
    {
        auto* node = a_anchor ? a_anchor->AsNode() : nullptr;
        if (!node)
        {
            return;
        }

        // Create Left Pupil Light
        if (!a_emitters.pupilLightL)
        {
            auto* light = RE::NiPointLight::Create();
            if (light)
            {
                light->name = RE::BSFixedString(kPupilLightLName);
                light->SetLightAttenuation(kPupilGlowRadius);
                light->local.rotate = RE::NiMatrix3();
                light->local.scale = 1.0f;
                node->AttachChild(light, false);
                a_emitters.pupilLightL.reset(light);
                ++_attachedLights;
                ++_lightsCreated;
            }
        }

        // Create Right Pupil Light
        if (!a_emitters.pupilLightR)
        {
            auto* light = RE::NiPointLight::Create();
            if (light)
            {
                light->name = RE::BSFixedString(kPupilLightRName);
                light->SetLightAttenuation(kPupilGlowRadius);
                light->local.rotate = RE::NiMatrix3();
                light->local.scale = 1.0f;
                node->AttachChild(light, false);
                a_emitters.pupilLightR.reset(light);
                ++_attachedLights;
                ++_lightsCreated;
            }
        }

        // Create Terminus Light
        if (_tuning.gazeRaysTerminus && !a_emitters.terminusLight)
        {
            auto* light = RE::NiPointLight::Create();
            if (light)
            {
                light->name = RE::BSFixedString(kTerminusLightName);
                light->SetLightAttenuation(kTerminusGlowRadius);
                light->local.rotate = RE::NiMatrix3();
                light->local.scale = 1.0f;
                node->AttachChild(light, false);
                a_emitters.terminusLight.reset(light);
                ++_attachedLights;
                ++_lightsCreated;
            }
        }
    }

    void VisualEffectsManager::DetachLights(ActorEmitters& a_emitters) noexcept
    {
        auto* node = a_emitters.parent.get() ? a_emitters.parent->AsNode() : nullptr;
        if (!node)
        {
            return;
        }

        if (a_emitters.pupilLightL)
        {
            node->DetachChild(a_emitters.pupilLightL.get());
            a_emitters.pupilLightL = nullptr;
            if (_attachedLights > 0)
                --_attachedLights;
        }
        if (a_emitters.pupilLightR)
        {
            node->DetachChild(a_emitters.pupilLightR.get());
            a_emitters.pupilLightR = nullptr;
            if (_attachedLights > 0)
                --_attachedLights;
        }
        if (a_emitters.terminusLight)
        {
            node->DetachChild(a_emitters.terminusLight.get());
            a_emitters.terminusLight = nullptr;
            if (_attachedLights > 0)
                --_attachedLights;
        }
    }

    void VisualEffectsManager::EnsureBeamGeometry(ActorEmitters& a_emitters,
                                                  RE::NiAVObject* a_anchor) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
#ifdef _WIN32
        __try
        {
            EnsureBeamGeometryInner(a_emitters, a_anchor);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            logger::error("[TrueGaze] SEH fault during beam geometry initialization; falling back "
                          "to lights.");
            a_emitters.geometryState = ActorEmitters::GeometryState::Invalid;
            EnsureLights(a_emitters, a_anchor);
        }
#else
        EnsureBeamGeometryInner(a_emitters, a_anchor);
#endif
#else
        (void)a_emitters;
        (void)a_anchor;
#endif
    }

    void VisualEffectsManager::EnsureBeamGeometryInner(ActorEmitters& a_emitters,
                                                       RE::NiAVObject* a_anchor) noexcept
    {
        if ((a_emitters.geometryL && a_emitters.geometryR) || !a_anchor || !a_anchor->AsNode())
        {
            return;
        }

        const uint64_t retryFrames =
            a_emitters.geometryState == ActorEmitters::GeometryState::Missing ? 300 : 30;
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
        RE::BSModelDB::DBTraits::ArgsType args;
        RE::BSResource::ErrorCode result = RE::BSResource::ErrorCode::kInvalidPath;
        const char* effectivePath = _tuning.beamModelPath;

        try
        {
            result = RE::BSModelDB::Demand(_tuning.beamModelPath, model, args);

            if ((result != RE::BSResource::ErrorCode::kNone || !model) &&
                _tuning.beamModelFallbackPath)
            {
                result = RE::BSModelDB::Demand(_tuning.beamModelFallbackPath, model, args);
                if (result == RE::BSResource::ErrorCode::kNone && model)
                {
                    effectivePath = _tuning.beamModelFallbackPath;
                }
            }
        }
        catch (const std::exception& e)
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
            // Fall back to lights
            EnsureLights(a_emitters, a_anchor);
            return;
        }

        auto clonedL = model->Clone();
        auto clonedR = model->Clone();
        auto* nodeL = clonedL ? clonedL->AsNode() : nullptr;
        auto* nodeR = clonedR ? clonedR->AsNode() : nullptr;

        if (!nodeL || !nodeR)
        {
            logger::warn("[TrueGaze] Failed to clone beam model '{}'.", effectivePath);
            a_emitters.geometryState = ActorEmitters::GeometryState::Invalid;
            EnsureLights(a_emitters, a_anchor);
            return;
        }

        // Crash-safety: validate the cloned geometry has at least one child
        // BSGeometry with a non-null shader property. Python-generated stub
        // NIFs pass BSModelDB::Demand (their header is valid Gamebryo) but
        // crash the renderer when it walks malformed vertex/index buffers or
        // a null shader property — the exact 0xC0000005 that took down the
        // GazeRegionPanel.nif and GazeBeam.nif attempts.
        if (!HasRendererSafeGeometry(nodeL))
        {
            logger::error("[TrueGaze] Beam model '{}' cloned successfully but contains "
                          "no valid BSGeometry with a shader property — rejecting to "
                          "prevent renderer crash. Falling back to light emitters.",
                          effectivePath);
            a_emitters.geometryState = ActorEmitters::GeometryState::Invalid;
            EnsureLights(a_emitters, a_anchor);
            return;
        }

        // Tint geometry BEFORE attaching to the live scene graph!
        // If anything faults during tinting, the cloned nodes have not touched
        // Skyrim's live scene graph and will be destructed safely.
        TintBeamGeometry(nodeL, _tuning);
        TintBeamGeometry(nodeR, _tuning);

        auto* anchorNode = a_anchor->AsNode();
        if (!anchorNode)
        {
            return;
        }

        // Atomic attachment: only attach once fully validated and tinted
        anchorNode->AttachChild(nodeL, false);
        anchorNode->AttachChild(nodeR, false);
        a_emitters.geometryL.reset(nodeL);
        a_emitters.geometryR.reset(nodeR);
        a_emitters.geometryState = ActorEmitters::GeometryState::Loaded;

        // This flag selects the native forward axis used by AlignBeamOrientation.
        // Both GazeBeam.nif and the vanilla marker_arrow.nif are authored along
        // local +Y. Only the Bethesda soul-cairn fallback is +Z-authored. The
        // previous marker_arrow classification routed the proven vanilla arrow
        // through the +Z basis and made it point in the wrong direction.
        a_emitters.usingFallbackMesh = (std::strstr(effectivePath, "fxsoulcairnbeam") != nullptr);
        a_emitters.isMarkerArrow = (std::strstr(effectivePath, "marker_arrow") != nullptr);
        _geometryCreated += 2;

        logger::info("[TrueGaze] Superman Laser Eyes geometry attached from '{}' (twin beams).",
                     effectivePath);
    }

    void VisualEffectsManager::EnsureHcepPanel(ActorEmitters& a_emitters,
                                               RE::NiAVObject* a_anchor) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
#ifdef _WIN32
        __try
        {
            EnsureHcepPanelInner(a_emitters, a_anchor);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            logger::error(
                "[TrueGaze] SEH fault during HCEP panel initialization; suppressing panel.");
            a_emitters.panelState = ActorEmitters::GeometryState::Invalid;
        }
#else
        EnsureHcepPanelInner(a_emitters, a_anchor);
#endif
#else
        (void)a_emitters;
        (void)a_anchor;
#endif
    }

    void VisualEffectsManager::EnsureHcepPanelInner(ActorEmitters& a_emitters,
                                                    RE::NiAVObject* a_anchor) noexcept
    {
        if (a_emitters.hcepPanel || !a_anchor || !a_anchor->AsNode())
        {
            return;
        }

        const uint64_t retryFrames =
            a_emitters.panelState == ActorEmitters::GeometryState::Missing ? 300 : 60;
        if (a_emitters.panelAttempts > 0 &&
            _frameCounter - a_emitters.lastPanelAttemptFrame < retryFrames)
        {
            return;
        }

        ++a_emitters.panelAttempts;
        a_emitters.lastPanelAttemptFrame = _frameCounter;
        a_emitters.panelState = ActorEmitters::GeometryState::Pending;

        RE::NiPointer<RE::NiNode> model;
        RE::BSModelDB::DBTraits::ArgsType args;
        RE::BSResource::ErrorCode result = RE::BSResource::ErrorCode::kInvalidPath;

        try
        {
            result = RE::BSModelDB::Demand(_tuning.hcepPanelModelPath, model, args);
        }
        catch (const std::exception& e)
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
            return;
        }

        auto clonedObject = model->Clone();
        auto* clonedNode = clonedObject ? clonedObject->AsNode() : nullptr;
        if (!clonedNode)
        {
            logger::warn("[TrueGaze] Failed to clone HCEP panel model '{}'.",
                         _tuning.hcepPanelModelPath);
            a_emitters.panelState = ActorEmitters::GeometryState::Invalid;
            return;
        }

        if (!HasRendererSafeGeometry(clonedNode))
        {
            logger::error("[TrueGaze] HCEP panel model '{}' cloned successfully but contains "
                          "no valid BSGeometry with a shader property — rejecting to "
                          "prevent renderer crash.",
                          _tuning.hcepPanelModelPath);
            a_emitters.panelState = ActorEmitters::GeometryState::Invalid;
            return;
        }

        auto* anchorNode = a_anchor->AsNode();
        if (!anchorNode)
        {
            return;
        }

        anchorNode->AttachChild(clonedNode, false);
        a_emitters.hcepPanel.reset(clonedNode);
        a_emitters.panelState = ActorEmitters::GeometryState::Loaded;
        logger::info("[TrueGaze] Attached HCEP floating diagram panel from '{}'.",
                     _tuning.hcepPanelModelPath);
    }

    void VisualEffectsManager::UpdateHcepPanel(ActorEmitters& a_emitters, RE::NiAVObject* a_anchor,
                                               uint8_t a_gazeRegion) noexcept
    {
        if (!a_emitters.hcepPanel || !a_anchor)
        {
            return;
        }

        try
        {
            // Panel floats directly in front of the head at HcepPanelForwardOffsetUnits.
            // The panel mesh is Bethesda's flat glow quad (fxglowflatrndmid, retextured
            // with Kirk's chroma-key HCEP diagram): authored 512 x 512 units in the XZ
            // plane, normal +Y (head-local forward). A viewer looking AT the NPC sees
            // the -Y side, so rotate 180 degrees about Z (diag(-1,-1,1) — symmetric,
            // safe against row/column conventions) to face the viewer.
            //
            // ASPECT: the diagram texture is 2760x1504 (aspect 1.835 w:h). The quad is
            // square, so the aspect is baked into the rotation COLUMNS: X (width)
            // scaled by aspect, Z (height) by 1.0. Columns per the verified NiMatrix3
            // convention (M * unitX = column 0).
            // FIX A6 (2026-10-03): centre the panel on the cyclopean eye, not the
            // head-bone origin, so a gaze of (yaw, pitch) lands at
            // (d*tan(yaw), d*tan(pitch)/cos(yaw)) on the panel as specified in
            // SPEC_Region_Map.md. eyeMidLocal is refreshed in UpdateActor.
            a_emitters.hcepPanel->local.translate =
                RE::NiPoint3{a_emitters.eyeMidLocal.x,
                             a_emitters.eyeMidLocal.y + _tuning.HcepPanelForwardOffsetUnits(),
                             a_emitters.eyeMidLocal.z};
            constexpr float kPanelSourceUnits = 512.0f;         // authored quad size
            constexpr float kDiagramAspect = 2760.0f / 1504.0f; // w:h = 1.835
            const float panelScale =
                (_tuning.hcepPanelScale > 0.1f ? _tuning.hcepPanelScale : 25.0f) /
                kPanelSourceUnits;
            RE::NiMatrix3 faceViewer;
            // column 0 = X axis (width): flipped (face viewer) and widened by aspect
            faceViewer.entry[0][0] = -kDiagramAspect;
            faceViewer.entry[1][0] = 0.0f;
            faceViewer.entry[2][0] = 0.0f;
            // column 1 = Y axis (normal): flipped (face viewer)
            faceViewer.entry[0][1] = 0.0f;
            faceViewer.entry[1][1] = -1.0f;
            faceViewer.entry[2][1] = 0.0f;
            // column 2 = Z axis (height)
            faceViewer.entry[0][2] = 0.0f;
            faceViewer.entry[1][2] = 0.0f;
            faceViewer.entry[2][2] = 1.0f;
            a_emitters.hcepPanel->local.rotate = faceViewer;
            a_emitters.hcepPanel->local.scale = panelScale;

            RE::NiUpdateData updateData;
            updateData.time = 0.0f;
            updateData.flags = RE::NiUpdateData::Flag::kDirty;
            a_emitters.hcepPanel->world = a_anchor->world * a_emitters.hcepPanel->local;
            a_emitters.hcepPanel->UpdateDownwardPass(updateData, 0);

            // Active region highlight: dynamically adjust emissive tint
            if (a_emitters.lastGazeRegion != a_gazeRegion || !a_emitters.lastAgreement)
            {
                auto* node = a_emitters.hcepPanel->AsNode();
                if (node && !node->GetChildren().empty())
                {
                    auto& child = node->GetChildren().front();
                    if (child)
                    {
                        auto* geo = child->AsGeometry();
                        if (geo)
                        {
                            auto* shaderProp = geo->GetGeometryRuntimeData().shaderProperty.get();
                            if (auto* effectShader =
                                    netimmerse_cast<RE::BSEffectShaderProperty*>(shaderProp))
                            {
                                if (auto* material = effectShader->GetMaterial())
                                {
                                    if (a_emitters.lastHitSuccess && !a_emitters.lastAgreement)
                                    {
                                        // A7.4 Mismatch visual alert: warm warning glow
                                        material->baseColor = RE::NiColorA(1.0f, 0.45f, 0.1f, 1.0f);
                                        material->baseColorScale = 1.6f;
                                    }
                                    else
                                    {
                                        const auto regionCol = GetRegionColour(a_gazeRegion, _tuning);
                                        material->baseColor =
                                            RE::NiColorA(regionCol.r, regionCol.g, regionCol.b, 1.0f);
                                        material->baseColorScale = 1.3f;
                                    }
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

    void VisualEffectsManager::DetachPanel(ActorEmitters& a_emitters) noexcept
    {
        auto* node = a_emitters.parent.get() ? a_emitters.parent->AsNode() : nullptr;
        if (node && a_emitters.hcepPanel)
        {
            node->DetachChild(a_emitters.hcepPanel.get());
        }
        a_emitters.hcepPanel = nullptr;
        a_emitters.panelState = ActorEmitters::GeometryState::Unknown;
        a_emitters.panelAttempts = 0;
        a_emitters.lastPanelAttemptFrame = 0;
    }

    void VisualEffectsManager::AttachEmitters(ActorEmitters& a_emitters,
                                              RE::NiAVObject* a_anchor) noexcept
    {
        a_emitters.parent.reset(a_anchor);
    }

    void VisualEffectsManager::DetachAll(ActorEmitters& a_emitters) noexcept
    {
        DetachLights(a_emitters);
        DetachPanel(a_emitters);

        auto* node = a_emitters.parent.get() ? a_emitters.parent->AsNode() : nullptr;
        if (node)
        {
            if (a_emitters.geometryL)
            {
                node->DetachChild(a_emitters.geometryL.get());
            }
            if (a_emitters.geometryR)
            {
                node->DetachChild(a_emitters.geometryR.get());
            }
        }
        a_emitters.geometryL = nullptr;
        a_emitters.geometryR = nullptr;
        a_emitters.geometryState = ActorEmitters::GeometryState::Unknown;
        a_emitters.geometryAttempts = 0;
        a_emitters.lastGeometryAttemptFrame = 0;
        a_emitters.isMarkerArrow = false;
        a_emitters.parent = nullptr;
    }

#else

    void VisualEffectsManager::EnsureLights(ActorEmitters&, RE::NiAVObject*) noexcept {}
    void VisualEffectsManager::DetachLights(ActorEmitters&) noexcept {}
    void VisualEffectsManager::EnsureBeamGeometry(ActorEmitters&, RE::NiAVObject*) noexcept {}
    void VisualEffectsManager::EnsureBeamGeometryInner(ActorEmitters&, RE::NiAVObject*) noexcept {}
    void VisualEffectsManager::EnsureHcepPanel(ActorEmitters&, RE::NiAVObject*) noexcept {}
    void VisualEffectsManager::EnsureHcepPanelInner(ActorEmitters&, RE::NiAVObject*) noexcept {}
    void VisualEffectsManager::UpdateHcepPanel(ActorEmitters&, RE::NiAVObject*, uint8_t) noexcept {}
    void VisualEffectsManager::DetachPanel(ActorEmitters&) noexcept {}

    void VisualEffectsManager::AttachEmitters(ActorEmitters&, RE::NiAVObject*) noexcept {}
    void VisualEffectsManager::DetachAll(ActorEmitters&) noexcept {}

#endif // __has_include(<RE/Skyrim.h>)

    // ---------------------------------------------------------------------------
    // Geometry helpers
    // ---------------------------------------------------------------------------

    void VisualEffectsManager::ResolvePupils(const RE::NiAVObject* a_headBone,
                                             const RE::NiAVObject* a_eyeL,
                                             const RE::NiAVObject* a_eyeR,
                                             const RE::NiAVObject* a_anchor, float a_eyeYawDeg,
                                             float a_eyePitchDeg, RE::NiPoint3& a_originLOut,
                                             RE::NiPoint3& a_originROut,
                                             RE::NiPoint3& a_dirOut) const noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        const RE::NiAVObject* basisObj = a_headBone ? a_headBone : (a_eyeL ? a_eyeL : a_anchor);
        const RE::NiMatrix3 basis = basisObj ? basisObj->world.rotate : RE::NiMatrix3();

        const float yawRad = a_eyeYawDeg * kDegToRad;
        const float pitchRad = a_eyePitchDeg * kDegToRad;

        const RE::NiPoint3 right = basis.GetVectorX();
        const RE::NiPoint3 forward = basis.GetVectorY();
        const RE::NiPoint3 up = basis.GetVectorZ();

        const float scale =
            a_headBone ? (a_headBone->world.scale > 0.0f ? a_headBone->world.scale : 1.0f) : 1.0f;
        const float halfIpd = 2.2f * scale; // ~3.15 cm half-IPD (63mm human average)

        if (a_eyeL && a_eyeR)
        {
            a_originLOut = a_eyeL->world.translate;
            a_originROut = a_eyeR->world.translate;
        }
        else if (a_eyeL)
        {
            a_originLOut = a_eyeL->world.translate;
            a_originROut = a_originLOut + right * (2.0f * halfIpd);
        }
        else if (a_eyeR)
        {
            a_originROut = a_eyeR->world.translate;
            a_originLOut = a_originROut - right * (2.0f * halfIpd);
        }
        else if (a_headBone)
        {
            const float fOff = _tuning.ForwardOffsetUnits() * scale;
            const float uOff = _tuning.UpOffsetUnits() * scale;
            const RE::NiPoint3 center{
                a_headBone->world.translate.x + forward.x * fOff + up.x * uOff,
                a_headBone->world.translate.y + forward.y * fOff + up.y * uOff,
                a_headBone->world.translate.z + forward.z * fOff + up.z * uOff};
            a_originLOut = RE::NiPoint3{center.x - right.x * halfIpd, center.y - right.y * halfIpd,
                                        center.z - right.z * halfIpd};
            a_originROut = RE::NiPoint3{center.x + right.x * halfIpd, center.y + right.y * halfIpd,
                                        center.z + right.z * halfIpd};
        }
        else if (a_anchor)
        {
            const float fOff = _tuning.ForwardOffsetUnits() * scale;
            const float uOff = _tuning.UpOffsetUnits() * scale;
            const RE::NiPoint3 center{a_anchor->world.translate.x + forward.x * fOff + up.x * uOff,
                                      a_anchor->world.translate.y + forward.y * fOff + up.y * uOff,
                                      a_anchor->world.translate.z + forward.z * fOff + up.z * uOff};
            a_originLOut = RE::NiPoint3{center.x - right.x * halfIpd, center.y - right.y * halfIpd,
                                        center.z - right.z * halfIpd};
            a_originROut = RE::NiPoint3{center.x + right.x * halfIpd, center.y + right.y * halfIpd,
                                        center.z + right.z * halfIpd};
        }
        else
        {
            a_originLOut = RE::NiPoint3{};
            a_originROut = RE::NiPoint3{};
        }

        a_dirOut = GazeDirection(basis, yawRad, pitchRad);
#else
        (void)a_headBone;
        (void)a_eyeL;
        (void)a_eyeR;
        (void)a_anchor;
        (void)a_eyeYawDeg;
        (void)a_eyePitchDeg;
        a_originLOut = RE::NiPoint3{};
        a_originROut = RE::NiPoint3{};
        a_dirOut = RE::NiPoint3{};
#endif
    }

    VisualEffectsManager::GazeHitInfo VisualEffectsManager::GetActorHitInfo(
        uint32_t a_formId) const noexcept
    {
        auto it = _emitters.find(a_formId);
        if (it != _emitters.end())
        {
            return {it->second.lastHitSuccess, it->second.lastHitRegion,
                    it->second.lastHitX, it->second.lastHitZ, it->second.lastAgreement};
        }
        return {};
    }

} // namespace TrueGaze::Visuals