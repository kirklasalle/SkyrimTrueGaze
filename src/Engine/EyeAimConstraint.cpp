#include "EyeAimConstraint.hpp"
#include "BoneController.hpp"

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#endif

#include <array>
#include <cmath>

namespace TrueGaze::Engine
{

    namespace
    {

#if __has_include(<RE/Skyrim.h>)

        /// A bone whose animated local rotation has been modified this frame.
        ///
        /// The reference is a raw pointer held only for the duration of one frame.
        /// The game cannot unload an actor while we are inside the update hook, so a
        /// smart pointer would add ref-count traffic for no safety gain.
        struct TouchedBone
        {
            uint32_t actorFormId{0};
            RE::NiAVObject *bone{nullptr};
            RE::NiMatrix3 originalRotate{};
            bool active{false};
        };

        /// Fixed capacity: an actor has at most six gaze joints of interest.
        /// Sized to support up to 85 simultaneously tracked actors per frame.
        constexpr uint32_t MAX_TOUCHED_BONES = 512;

        std::array<TouchedBone, MAX_TOUCHED_BONES> g_touched{};
        uint32_t g_touchedCount{0};
        bool g_frameOpen{false};

        /// Build the relative rotation for a yaw/pitch deflection, in degrees,
        /// expressed in the bone's parent frame.
        ///
        /// Skyrim coordinate system:
        ///   X = Right (lateral axis) -> Pitch (nodding up/down)
        ///   Y = Forward (longitudinal axis) -> Roll (head tilt) = 0
        ///   Z = Up (vertical axis) -> Yaw (turning left/right)
        RE::NiMatrix3 MakeGazeRotation(float yawDeg, float pitchDeg) noexcept
        {
            constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

            const float y = yawDeg * kDegToRad;
            const float p = pitchDeg * kDegToRad;

            RE::NiMatrix3 m;
            // In Skyrim / NetImmerse:
            // X-axis: pitch (negated so positive pitch rotates up toward +Z)
            // Y-axis: roll = 0 (no head tilt)
            // Z-axis: yaw (positive yaw rotates right toward +X)
            m.EulerAnglesToAxesZXY(-p, 0.0f, y);
            return m;
        }

        /// Recompute a bone's world transform from its local transform and its
        /// parent's world transform using NetImmerse transform composition.
        void RefreshWorldTransform(RE::NiAVObject *bone) noexcept
        {
            if (!bone)
            {
                return;
            }

            RE::NiAVObject *parent = bone->parent;
            if (parent)
            {
                bone->world = parent->world * bone->local;
            }
            else
            {
                // Root node: world and local coincide.
                bone->world = bone->local;
            }

            // Downward pass propagates transform changes to child meshes
            // (hair, beard, horns, helmets, and facial attachments).
            RE::NiUpdateData updateData;
            updateData.time = 0.0f;
            updateData.flags = RE::NiUpdateData::Flag::kDirty;
            bone->UpdateDownwardPass(updateData, 0);
        }

        /// Locate an existing record for this bone, or create one.
        TouchedBone *FindOrCreate(uint32_t actorFormId, RE::NiAVObject *bone) noexcept
        {
            for (uint32_t i = 0; i < g_touchedCount; ++i)
            {
                if (g_touched[i].bone == bone)
                {
                    return &g_touched[i];
                }
            }

            if (g_touchedCount >= MAX_TOUCHED_BONES)
            {
                return nullptr;
            }

            TouchedBone &slot = g_touched[g_touchedCount++];
            slot.actorFormId = actorFormId;
            slot.bone = bone;
            slot.originalRotate = bone->local.rotate; // cache the animated pose
            slot.active = true;
            return &slot;
        }

#endif // __has_include(<RE/Skyrim.h>)

    } // namespace

    void EyeAimConstraint::BeginFrame() noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        // The previous frame's bones must already have been restored. If they were
        // not, Withdraw was missed — fail safe by restoring now rather than letting
        // the deflection compound frame over frame.
        if (g_frameOpen)
        {
            Withdraw();
        }

        g_touchedCount = 0;
        g_frameOpen = true;
#endif
    }

    bool EyeAimConstraint::Apply(uint32_t actorFormId, RE::NiAVObject *bone,
                                 float yawDeg, float pitchDeg) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (!bone)
        {
            return false;
        }

        if (!g_frameOpen)
        {
            BeginFrame();
        }

        // Nothing to do for a negligible deflection; leave the animated pose intact.
        if (std::abs(yawDeg) < 0.01f && std::abs(pitchDeg) < 0.01f)
        {
            return true;
        }

        TouchedBone *slot = FindOrCreate(actorFormId, bone);
        if (!slot)
        {
            return false; // capacity exhausted
        }

        // Always compose from the cached animated pose, never from the modified one,
        // so repeated calls within a frame are idempotent.
        slot->bone->local.rotate = MakeGazeRotation(yawDeg, pitchDeg) * slot->originalRotate;

        // The world transform is stale until this is recomputed; without it the
        // bone's skinned vertices use last frame's matrix.
        RefreshWorldTransform(slot->bone);
        return true;
#else
        (void)actorFormId;
        (void)bone;
        (void)yawDeg;
        (void)pitchDeg;
        return false;
#endif
    }

    void EyeAimConstraint::WithdrawActor(uint32_t actorFormId) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        auto *form = RE::TESForm::LookupByID(actorFormId);
        auto *actor = form ? form->As<RE::Actor>() : nullptr;
        auto *root = actor ? actor->Get3D() : nullptr;

        uint32_t write = 0;
        for (uint32_t read = 0; read < g_touchedCount; ++read)
        {
            TouchedBone &slot = g_touched[read];
            if (slot.active && slot.actorFormId == actorFormId)
            {
                if (slot.bone && root)
                {
                    slot.bone->local.rotate = slot.originalRotate;
                    RefreshWorldTransform(slot.bone);
                }
                slot.active = false;
                slot.actorFormId = 0;
                slot.bone = nullptr;
                continue;
            }

            if (write != read)
            {
                g_touched[write] = slot;
            }
            ++write;
        }
        g_touchedCount = write;
        g_frameOpen = g_touchedCount != 0;
#else
        (void)actorFormId;
#endif
    }

    void EyeAimConstraint::Withdraw() noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        for (uint32_t i = 0; i < g_touchedCount; ++i)
        {
            TouchedBone &slot = g_touched[i];
            if (slot.active && slot.bone)
            {
                auto *form = RE::TESForm::LookupByID(slot.actorFormId);
                auto *actor = form ? form->As<RE::Actor>() : nullptr;
                if (actor && actor->Get3D())
                {
                    slot.bone->local.rotate = slot.originalRotate;
                    RefreshWorldTransform(slot.bone);
                }
            }
            slot.active = false;
            slot.actorFormId = 0;
            slot.bone = nullptr;
        }

        g_touchedCount = 0;
        g_frameOpen = false;
#endif
    }

    void EyeAimConstraint::Reset() noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        for (uint32_t i = 0; i < g_touchedCount; ++i)
        {
            g_touched[i].active = false;
            g_touched[i].actorFormId = 0;
            g_touched[i].bone = nullptr;
        }

        g_touchedCount = 0;
        g_frameOpen = false;
#endif
    }

    uint32_t EyeAimConstraint::ActiveBoneCount() noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        uint32_t count = 0;
        for (uint32_t i = 0; i < g_touchedCount; ++i)
        {
            if (g_touched[i].active)
            {
                ++count;
            }
        }
        return count;
#else
        return 0;
#endif
    }

} // namespace TrueGaze::Engine
