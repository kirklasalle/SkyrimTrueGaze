#include "EfmBlinkController.hpp"
#include <algorithm>
#include <cmath>

namespace TrueGaze::Integrations
{
    namespace
    {
#if __has_include(<RE/Skyrim.h>)
        class FaceGenSpinLockGuard
        {
        public:
            explicit FaceGenSpinLockGuard(RE::BSSpinLock& lock) noexcept : _lock(lock) { _lock.Lock(); }
            ~FaceGenSpinLockGuard() noexcept { _lock.Unlock(); }
            FaceGenSpinLockGuard(const FaceGenSpinLockGuard&) = delete;
            FaceGenSpinLockGuard& operator=(const FaceGenSpinLockGuard&) = delete;

        private:
            RE::BSSpinLock& _lock;
        };
#endif
    } // namespace

    EfmBlinkController::FaceGenEyeSnapshot
    EfmBlinkController::ReadRenderedFaceGenEyes(uint32_t actorFormId, float fullScaleDeg,
                                                float gain) noexcept
    {
        FaceGenEyeSnapshot snap{};
        if (actorFormId == 0)
        {
            return snap;
        }
#if __has_include(<RE/Skyrim.h>)
        auto* form = RE::TESForm::LookupByID(actorFormId);
        auto* actor = form ? form->As<RE::Actor>() : nullptr;
        if (!actor || !actor->Get3D())
        {
            return snap;
        }
        auto* fg = actor->GetFaceGenAnimationData();
        if (!fg)
        {
            return snap;
        }

        constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
        // Invert TrueGaze's active morph mapping (fullScaleDeg / gain) when reading back
        // `modifier3` so a 12 deg glance with gain=1.7 doesn't overshoot arrows to 32 deg,
        // capped at Skyrim's physical eyeball limit (~32 deg H, ~24 deg V).
        const float safeGain = std::max(0.25f, gain);
        const float effMaxYawDeg = std::clamp(std::max(1.0f, fullScaleDeg) / safeGain, 10.0f, 32.0f);
        const float effMaxPitchDeg = std::clamp(std::max(1.0f, fullScaleDeg) / safeGain, 10.0f, 24.0f);

        FaceGenSpinLockGuard guard(fg->lock);
        snap.valid = true;
        snap.eyesHeadingDeg = std::isfinite(fg->eyesHeading) ? (fg->eyesHeading * kRadToDeg) : 0.0f;
        snap.eyesPitchDeg = std::isfinite(fg->eyesPitch) ? (fg->eyesPitch * kRadToDeg) : 0.0f;

        using Modifier = RE::BSFaceGenKeyframeMultiple::Modifier;
        const auto idxD = static_cast<std::uint32_t>(Modifier::LookDown);
        const auto idxL = static_cast<std::uint32_t>(Modifier::LookLeft);
        const auto idxR = static_cast<std::uint32_t>(Modifier::LookRight);
        const auto idxU = static_cast<std::uint32_t>(Modifier::LookUp);

        if (fg->modifier3.values && fg->modifier3.count > idxU)
        {
            const auto safe01 = [](float v) noexcept {
                return std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f;
            };
            snap.mod3LookDown = safe01(fg->modifier3.values[idxD]);
            snap.mod3LookLeft = safe01(fg->modifier3.values[idxL]);
            snap.mod3LookRight = safe01(fg->modifier3.values[idxR]);
            snap.mod3LookUp = safe01(fg->modifier3.values[idxU]);

            const float netYaw = snap.mod3LookRight - snap.mod3LookLeft;
            const float netPitch = snap.mod3LookUp - snap.mod3LookDown;
            snap.renderedYawDeg = netYaw * effMaxYawDeg;
            snap.renderedPitchDeg = netPitch * effMaxPitchDeg;
            // If FaceGen's own eyesHeading is larger in the same direction (e.g. vanilla side-eye),
            // preserve the wider physical angle so arrows never under-shoot a genuine side-eye.
            if (std::abs(snap.eyesHeadingDeg) > std::abs(snap.renderedYawDeg) &&
                (snap.eyesHeadingDeg * netYaw > 0.0f || std::abs(netYaw) <= 0.015f))
            {
                snap.renderedYawDeg = std::clamp(snap.eyesHeadingDeg, -32.0f, 32.0f);
            }
            snap.hasRenderedMorphs = (std::abs(netYaw) > 0.015f || std::abs(netPitch) > 0.015f ||
                                      std::abs(snap.eyesHeadingDeg) > 1.0f);
        }
#else
        (void)fullScaleDeg;
        (void)gain;
#endif
        return snap;
    }

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
        if (faceGenData)
        {
            FaceGenSpinLockGuard guard(faceGenData->lock);
            if (faceGenData->modifierKeyFrame.values &&
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
        }
#else
        (void)clampedWeight;
        (void)eyeYawDeg;
        (void)eyePitchDeg;
#endif
    }

} // namespace TrueGaze::Integrations
