#pragma once

#include "ExpandedPlay/AwakeningRitual.hpp"
#include "ExpandedPlay/CombatAndCreatureSight.hpp"
#include "ExpandedPlay/DemeanorController.hpp"
#include "ExpandedPlay/ExpandedPlayTypes.hpp"
#include "ExpandedPlay/FactionSight.hpp"
#include "ExpandedPlay/GazeLedger.hpp"
#include "ExpandedPlay/NpcVoiceResponder.hpp"
#include "ExpandedPlay/PersuasionTransactions.hpp"
#include "ExpandedPlay/PresenceSignature.hpp"
#include "ExpandedPlay/ResponseLadder.hpp"

#include <cstdint>

namespace TrueGaze::ExpandedPlay
{
    /// @brief Top-level gameplay orchestrator for TrueGaze Expanded Play (Phase G0-G3).
    /// Ticked from GazeEngine::EndFrame on the game thread at ~10 Hz focus / 2 Hz witness,
    /// and zero-overhead early-returning when [ExpandedPlay] bEnabled == false.
    class ExpandedPlayManager
    {
    public:
        static ExpandedPlayManager& Get() noexcept;

        ExpandedPlayManager(const ExpandedPlayManager&) = delete;
        ExpandedPlayManager& operator=(const ExpandedPlayManager&) = delete;

        void Tick(float deltaSeconds) noexcept;
        void ResetSession() noexcept;
        void OnGameLoaded() noexcept;
        void OnBeforeSave() noexcept;

        void PinManualDemeanor(Demeanor chosen) noexcept;
        bool StartOrAbortStaringContest(uint32_t targetActorFormId) noexcept;
        void ForceConsoleAwakeningUnlock() noexcept;

        // --- Diagnostics & console readout ------------------------------------
        [[nodiscard]] AwakeningState CurrentAwakening() const noexcept;
        [[nodiscard]] AwakeningStage CurrentAwakeningStage() const noexcept;
        [[nodiscard]] StageCapabilities CurrentCapabilities() const noexcept;
        [[nodiscard]] uint32_t ActiveKindredAllyCount() const noexcept;
        [[nodiscard]] Demeanor ActiveDemeanor() const noexcept;
        [[nodiscard]] bool IsDemeanorPinned() const noexcept;
        [[nodiscard]] float DemeanorPinRemainingSec() const noexcept;
        [[nodiscard]] ContestStatus ContestState() const noexcept;
        [[nodiscard]] bool IsContestActive() const noexcept;
        [[nodiscard]] uint8_t ContestRound() const noexcept;
        [[nodiscard]] float ContestElapsedSec() const noexcept;
        [[nodiscard]] float ContestBlinkPressure01() const noexcept;
        [[nodiscard]] size_t LedgerEntryCount() const noexcept;
        [[nodiscard]] PresenceState CurrentPresence() const noexcept;
        [[nodiscard]] SignatureAxes CurrentSignature() const noexcept;
        [[nodiscard]] SignatureTitle CurrentSignatureTitle() const noexcept;
        [[nodiscard]] Interpretation LastInterpretation() const noexcept;
        [[nodiscard]] PersuasionVector LastPersuasionVector() const noexcept;
        [[nodiscard]] AffinityTier LastAffinityTier() const noexcept;
        [[nodiscard]] int32_t LastBribeCostGold() const noexcept;
        [[nodiscard]] float ActiveRapportSpeechBonus() const noexcept;
        [[nodiscard]] uint32_t ActiveLawWitnessCount() const noexcept;

    private:
        ExpandedPlayManager() = default;

        void EvaluateFocus10Hz(float gameDays) noexcept;
        void EvaluateWitnesses2Hz(float gameDays) noexcept;
        void SyncPersistentStateToSerializer() noexcept;

        DemeanorController _demeanorCtrl{};
        GazeLedger _ledger{};
        PresenceSignature _presenceSig{};
        AwakeningState _awakening{};
        StaringContestController _contest{};
        EyeHandEyeTracker _handEyeTracker{};
        TransactionSaturationTracker _txSaturation{};
        NpcCooldownTracker _barkCooldown{};
        NpcCooldownTracker _greetingCooldown{};
        NpcCooldownTracker _witnessCooldown{};
        NpcCooldownTracker _kindredCooldown{};
        NpcCooldownTracker _combatSightCooldown{};
        NpcCooldownTracker _creatureSightCooldown{};
        NpcCooldownTracker _factionSightCooldown{};
        std::vector<uint32_t> _awardedCreatureMilestones{};

        float _sessionElapsedSec{0.0f};
        float _focusAccumSec{0.0f};
        float _witnessAccumSec{0.0f};
        float _ledgerCommitAccumSec{0.0f};
        float _sanctuaryStillnessSec{0.0f};
        uint8_t _lastSanctuaryBit{0};
        uint32_t _activeKindredAllyCount{0};

        uint32_t _focusActorFormId{0};
        uint8_t _focusRegionId{0};
        float _focusDwellSec{0.0f};
        float _focusLossGraceSec{0.0f};
        DwellBand _lastLoggedDwellBand{DwellBand::None};
        Demeanor _lastLoggedDemeanor{Demeanor::Courteous};
        bool _loggedDemeanorInit{false};
        bool _focusIsMutual{false};
        bool _focusRapidEscalation{false};
        Interpretation _lastInterpretation{Interpretation::None};
        PersuasionVector _lastPersuasionVector{PersuasionVector::None};
        AffinityTier _lastAffinityTier{AffinityTier::Like};
        int32_t _lastBribeCostGold{0};
        uint32_t _activeNeutralWitnessCount{0};
        uint32_t _activeLawWitnessCount{0};
        uint32_t _activeLawWitnessActorId{0};
        int8_t _activeLawWitnessDirSign{0};
        float _activeGuardGlanceRangeMeters{12.0f};
        bool _persuasionProbeActive{false};
        bool _persuasionGuardGlanceActive{false};
        bool _slyBribeArmedNow{false};
        float _lastDiscoveryCheckSec{-100.0f};
        float _lastNoteInventoryCheckSec{-100.0f};
        bool _visitedSleepingGiantThisSession{false};
        float _innLeftGameDay{-1.0f};
        bool _pendingInnNoteHandoff{false};
        uint32_t _pendingInnNoteSpeakerId{0};
        bool _probeTellSent{false};
        bool _txCommittedForHold{false};
        float _lastStrength{0.0f};
        float _frostVignetteAlpha{0.0f};
        float _magnetismX{0.0f};
        float _magnetismY{0.0f};
        float _thirdPersonPulseDwell{0.0f};
        uint32_t _rapportTargetFormId{0};
        float _rapportExpiresSessionSec{0.0f};
        float _tacticalBuffExpiresSessionSec{0.0f};
        bool _tacticalBuffActive{false};
        bool _stateDirty{false};
        DemeanorContext _lastDemeanorCtx{};
        bool _hadFocusedActor{false};
    };
} // namespace TrueGaze::ExpandedPlay
