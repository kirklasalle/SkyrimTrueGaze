#include "EfmBlinkController.hpp"
#include <algorithm>

namespace TrueGaze::Integrations
{

    void EfmBlinkController::ApplyGazeMorphs(uint32_t actorFormId, float eyelidWeight, float eyeYawDeg, float eyePitchDeg) noexcept
    {
        if (actorFormId == 0)
            return;

        float clampedWeight = std::clamp(eyelidWeight, 0.0f, 1.0f);

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
        if (faceGenData)
        {
            using Modifier = RE::BSFaceGenKeyframeMultiple::Modifier;

            faceGenData->modifierKeyFrame.SetValue(
                static_cast<std::uint32_t>(Modifier::BlinkLeft), clampedWeight);
            faceGenData->modifierKeyFrame.SetValue(
                static_cast<std::uint32_t>(Modifier::BlinkRight), clampedWeight);

            const float lookLeft = (eyeYawDeg < -0.5f) ? std::clamp(-eyeYawDeg / 30.0f, 0.0f, 1.0f) : 0.0f;
            const float lookRight = (eyeYawDeg > 0.5f) ? std::clamp(eyeYawDeg / 30.0f, 0.0f, 1.0f) : 0.0f;
            const float lookDown = (eyePitchDeg < -0.5f) ? std::clamp(-eyePitchDeg / 25.0f, 0.0f, 1.0f) : 0.0f;
            const float lookUp = (eyePitchDeg > 0.5f) ? std::clamp(eyePitchDeg / 25.0f, 0.0f, 1.0f) : 0.0f;

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
