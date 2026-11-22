#pragma once

#include "ExpandedPlay/ExpandedPlayTypes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace TrueGaze::ExpandedPlay
{
    /// @brief Snapshot of game context used to infer or unpin 3rd-person Demeanor.
    struct DemeanorContext
    {
        bool inJarlCourtOrTemple{false};
        bool playerIsThane{false};
        bool weaponDrawn{false};
        bool hostileNearby15m{false};
        bool sneaking{false};
        bool carryingStolenNearOwnerOrGuard{false};
        bool wearingDarkBrotherhoodArmor{false};
        bool predatoryUnlocked{false};
        bool allowPredatoryInTowns{false};
        bool inPeacefulTown{false};
        bool targetIsChild{false};
        RelationshipTier targetRelationship{RelationshipTier::Stranger};
        uint32_t currentLocationFormId{0};
        uint32_t dialogueTargetFormId{0};
        bool inCombat{false};
    };

    /// @brief Tuned behavioral parameters produced by an active third-person Demeanor.
    struct DemeanorProfile
    {
        Demeanor demeanor{Demeanor::Courteous};
        float dwellScale{1.0f};
        float aversionTendency{0.25f};
        float foreheadBias{0.10f};
        float mouthBias{0.15f};
        float groundChinBias{0.05f};
        float efficacyMultiplier{0.80f};
    };

    /// @brief Resolves the automatic third-person Demeanor by strict priority rules.
    [[nodiscard]] constexpr Demeanor InferAutoDemeanor(const DemeanorContext& ctx) noexcept
    {
        // 1. Court / Temple / Superior etiquette unless holding Thane privilege
        if ((ctx.inJarlCourtOrTemple || ctx.targetRelationship == RelationshipTier::Superior) &&
            !ctx.playerIsThane)
        {
            return Demeanor::Deferential;
        }

        // 2. Armed or immediate threat posture
        if (ctx.weaponDrawn || ctx.hostileNearby15m || ctx.inCombat)
        {
            return Demeanor::Stern;
        }

        // 3. Concealment / stolen goods pressure
        if (ctx.sneaking || ctx.carryingStolenNearOwnerOrGuard)
        {
            return Demeanor::Evasive;
        }

        // 4. Close bonds (spouse, partner, friend, confidant)
        if (ctx.targetRelationship == RelationshipTier::Spouse ||
            ctx.targetRelationship == RelationshipTier::Partner ||
            ctx.targetRelationship == RelationshipTier::Friend ||
            ctx.targetRelationship == RelationshipTier::Confidant)
        {
            return Demeanor::Warm;
        }

        // 5. Unlocked Dark Brotherhood Predatory bearing (never against children)
        if (ctx.wearingDarkBrotherhoodArmor && ctx.predatoryUnlocked && !ctx.targetIsChild &&
            (!ctx.inPeacefulTown || ctx.allowPredatoryInTowns))
        {
            return Demeanor::Predatory;
        }

        // 6. Baseline social triangle courtesy
        return Demeanor::Courteous;
    }

    /// @brief Produces gaze kinematics weights and efficacy scaling for a Demeanor.
    [[nodiscard]] inline DemeanorProfile BuildDemeanorProfile(Demeanor demeanor,
                                                              bool isThirdPerson,
                                                              float thirdPersonEfficacy = 0.80f) noexcept
    {
        float efficacy = 1.0f;
        if (isThirdPerson)
        {
            efficacy = std::isfinite(thirdPersonEfficacy)
                           ? std::clamp(thirdPersonEfficacy, 0.50f, 1.00f)
                           : 0.80f;
        }

        switch (demeanor)
        {
        case Demeanor::Warm:
            return {Demeanor::Warm, 1.15f, 0.15f, 0.05f, 0.35f, 0.02f, efficacy};
        case Demeanor::Stern:
            return {Demeanor::Stern, 1.40f, 0.05f, 0.25f, 0.02f, 0.00f, efficacy};
        case Demeanor::Evasive:
            return {Demeanor::Evasive, 0.50f, 0.75f, 0.00f, 0.00f, 0.25f, efficacy};
        case Demeanor::Deferential:
            return {Demeanor::Deferential, 0.65f, 0.45f, 0.00f, 0.00f, 0.60f, efficacy};
        case Demeanor::Predatory:
            return {Demeanor::Predatory, 1.80f, 0.00f, 0.15f, 0.00f, 0.00f, efficacy};
        case Demeanor::Courteous:
        default:
            return {Demeanor::Courteous, 1.00f, 0.25f, 0.10f, 0.15f, 0.05f, efficacy};
        }
    }

    /// @brief State machine governing Manual, Auto, and Hybrid 3rd-person Demeanor selection.
    class DemeanorController
    {
    public:
        static constexpr float kHybridPinDurationSec = 60.0f;

        void PinManualDemeanor(Demeanor chosen, const DemeanorContext& currentCtx,
                               float pinDurationSec = kHybridPinDurationSec) noexcept
        {
            if (static_cast<uint8_t>(chosen) >= static_cast<uint8_t>(Demeanor::Count))
            {
                chosen = Demeanor::Courteous;
            }
            if (currentCtx.targetIsChild && chosen == Demeanor::Predatory)
            {
                chosen = Demeanor::Courteous;
            }

            _pinned = true;
            _pinnedDemeanor = chosen;
            _pinRemainingSec =
                (std::isfinite(pinDurationSec) && pinDurationSec > 0.0f)
                    ? pinDurationSec
                    : kHybridPinDurationSec;
            _pinLocationFormId = currentCtx.currentLocationFormId;
            _pinDialogueTargetFormId = currentCtx.dialogueTargetFormId;
            _pinInCombat = currentCtx.inCombat;
        }

        void ClearPin() noexcept
        {
            _pinned = false;
            _pinRemainingSec = 0.0f;
        }

        [[nodiscard]] bool IsPinned() const noexcept { return _pinned; }
        [[nodiscard]] float PinRemainingSec() const noexcept
        {
            return _pinned ? _pinRemainingSec : 0.0f;
        }
        [[nodiscard]] Demeanor ActiveDemeanor() const noexcept { return _active; }

        /// @brief Updates active Demeanor for the current frame/tick.
        Demeanor Update(DemeanorMode mode, Demeanor configuredManual, const DemeanorContext& ctx,
                        float deltaSec, bool* outChanged = nullptr) noexcept
        {
            const float dt =
                (std::isfinite(deltaSec) && deltaSec > 0.0f) ? deltaSec : 0.0f;

            if (static_cast<uint8_t>(configuredManual) >= static_cast<uint8_t>(Demeanor::Count))
            {
                configuredManual = Demeanor::Courteous;
            }

            if (_pinned)
            {
                const bool locationChanged = (ctx.currentLocationFormId != _pinLocationFormId);
                const bool combatChanged = (ctx.inCombat != _pinInCombat);
                const bool dialogueEnded =
                    (_pinDialogueTargetFormId != 0u && ctx.dialogueTargetFormId == 0u);

                _pinRemainingSec -= dt;
                if (locationChanged || combatChanged || dialogueEnded || _pinRemainingSec <= 0.0f)
                {
                    ClearPin();
                }
            }

            Demeanor resolved = Demeanor::Courteous;
            switch (mode)
            {
            case DemeanorMode::Manual:
                resolved = configuredManual;
                break;
            case DemeanorMode::Auto:
                ClearPin();
                resolved = InferAutoDemeanor(ctx);
                break;
            case DemeanorMode::Hybrid:
            default:
                resolved = _pinned ? _pinnedDemeanor : InferAutoDemeanor(ctx);
                break;
            }

            if (ctx.targetIsChild && resolved == Demeanor::Predatory)
            {
                resolved = Demeanor::Courteous;
            }

            const bool changed = !_initialized || (resolved != _active);
            _active = resolved;
            _initialized = true;
            if (outChanged)
            {
                *outChanged = changed;
            }
            return _active;
        }

    private:
        bool _pinned{false};
        Demeanor _pinnedDemeanor{Demeanor::Courteous};
        float _pinRemainingSec{0.0f};
        uint32_t _pinLocationFormId{0};
        uint32_t _pinDialogueTargetFormId{0};
        bool _pinInCombat{false};
        Demeanor _active{Demeanor::Courteous};
        bool _initialized{false};
    };
} // namespace TrueGaze::ExpandedPlay
