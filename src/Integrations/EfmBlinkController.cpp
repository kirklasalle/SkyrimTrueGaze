#include "EfmBlinkController.hpp"
#include <algorithm>

namespace TrueGaze::Integrations
{

    void EfmBlinkController::ApplyGazeMorphs(uint32_t actorFormId, float eyelidWeight,
                                             float eyeYawDeg, float eyePitchDeg,
                                             float fullScaleDeg, float gain) noexcept
    {
        if (actorFormId == 0)
            return;

        float clampedWeight = std::clamp(eyelidWeight, 0.0f, 1.0f);

        // Guard against a degenerate full-scale that would divide by ~zero.
        const float invFullScale = 1.0f / std::max(1.0f, fullScaleDeg);
        const float g = std::max(0.0f, gain);

#if __has_include(<RE/Skyrim.h>)
        auto *form = RE::TESForm::LookupByID(actorFormId);
        if (!form)
            return;

        auto *actor = form->As<RE::Actor>();
        if (!actor || !actor->Get3D())
            return;

        // In full game environment: drive BSFaceGenAnimationData modifier keyframes.
        // Eyelid closure: BlinkLeft = 0, BlinkRight = 1
        // Eye Direction: LookDown = 8, LookLeft = 9, LookRight = 10, LookUp = 11
        // This directly enhances Expressive Facial Animation (EFA) & Expressive Facegen Morphs (EFM),
        // as well as vanilla Skyrim eye meshes, without corrupting expression keyframes.
        auto *faceGenData = actor->GetFaceGenAnimationData();
        if (faceGenData && faceGenData->modifierKeyFrame.values &&
            faceGenData->modifierKeyFrame.count > static_cast<std::uint32_t>(RE::BSFaceGenKeyframeMultiple::Modifier::LookUp))
        {
            using Modifier = RE::BSFaceGenKeyframeMultiple::Modifier;

            faceGenData->modifierKeyFrame.SetValue(
                static_cast<std::uint32_t>(Modifier::BlinkLeft), clampedWeight);
            faceGenData->modifierKeyFrame.SetValue(
                static_cast<std::uint32_t>(Modifier::BlinkRight), clampedWeight);

            // Map solved ocular deflection to Look* morph weights. The gain and
            // full-scale come from configuration so vanilla NPCs (no eye bones) show
            // a clearly visible, eye-leading gaze at conversation distance, where the
            // raw solved angle is only a few degrees. Result is clamped to [0,1] so
            // the eyes can never over-rotate past the morph's physical extent.
            const float lookLeft = (eyeYawDeg < -0.25f) ? std::clamp(-eyeYawDeg * invFullScale * g, 0.0f, 1.0f) : 0.0f;
            const float lookRight = (eyeYawDeg > 0.25f) ? std::clamp(eyeYawDeg * invFullScale * g, 0.0f, 1.0f) : 0.0f;
            const float lookDown = (eyePitchDeg < -0.25f) ? std::clamp(-eyePitchDeg * invFullScale * g, 0.0f, 1.0f) : 0.0f;
            const float lookUp = (eyePitchDeg > 0.25f) ? std::clamp(eyePitchDeg * invFullScale * g, 0.0f, 1.0f) : 0.0f;

            faceGenData->modifierKeyFrame.SetValue(
                static_cast<std::uint32_t>(Modifier::LookLeft), lookLeft);
            faceGenData->modifierKeyFrame.SetValue(
                static_cast<std::uint32_t>(Modifier::LookRight), lookRight);
            faceGenData->modifierKeyFrame.SetValue(
                static_cast<std::uint32_t>(Modifier::LookDown), lookDown);
            faceGenData->modifierKeyFrame.SetValue(
                static_cast<std::uint32_t>(Modifier::LookUp), lookUp);

            faceGenData->modifierKeyFrame.isUpdated = true;
        }
#else
        (void)clampedWeight;
        (void)eyeYawDeg;
        (void)eyePitchDeg;
#endif
    }

} // namespace TrueGaze::Integrations
