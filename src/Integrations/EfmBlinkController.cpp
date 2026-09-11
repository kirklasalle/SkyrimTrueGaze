#include "EfmBlinkController.hpp"
#include <algorithm>

namespace TrueGaze::Integrations {

void EfmBlinkController::ApplyMorphs(uint32_t actorFormId, float eyelidWeight) noexcept
{
    if (actorFormId == 0) return;

    float clampedWeight = std::clamp(eyelidWeight, 0.0f, 1.0f);

#if __has_include(<RE/Skyrim.h>)
    auto* form = RE::TESForm::LookupByID(actorFormId);
    if (!form) return;

    auto* actor = form->As<RE::Actor>();
    if (!actor || !actor->Get3D()) return;

    // In full game environment: Bind to Expressive Facegen Morphs (EFM) / BSFaceGenAnimationData
    auto* faceGenData = actor->GetFaceGenAnimationData();
    if (faceGenData) {
        // Morph indexes for EFM: Left/Right blink
        // faceGenData->exprOverrides[RE::FaceGen::Expression::BlinkLeft] = clampedWeight;
        // faceGenData->exprOverrides[RE::FaceGen::Expression::BlinkRight] = clampedWeight;
    }
#else
    // Standalone fallback: simulate morph application
    (void)clampedWeight;
#endif
}

} // namespace TrueGaze::Integrations
