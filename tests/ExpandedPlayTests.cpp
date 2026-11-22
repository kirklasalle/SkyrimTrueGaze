#include "TestSupport.hpp"
#include "ExpandedPlay/AwakeningRitual.hpp"
#include "ExpandedPlay/BookStudyTracker.hpp"
#include "ExpandedPlay/CombatAndCreatureSight.hpp"
#include "ExpandedPlay/CoSaveSerializer.hpp"
#include "ExpandedPlay/DemeanorController.hpp"
#include "ExpandedPlay/ExpandedPlayConfig.hpp"
#include "ExpandedPlay/ExpandedPlayModifiers.hpp"
#include "ExpandedPlay/ExpandedPlayTypes.hpp"
#include "ExpandedPlay/GazeLedger.hpp"
#include "ExpandedPlay/GazeReticle.hpp"
#include "ExpandedPlay/InterpretationEngine.hpp"
#include "ExpandedPlay/PerceptionGate.hpp"
#include "ExpandedPlay/PersuasionTransactions.hpp"
#include "ExpandedPlay/PresenceSignature.hpp"
#include "ExpandedPlay/ResponseLadder.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

namespace EP = TrueGaze::ExpandedPlay;

namespace
{
    void TestDefaultsAndSafety()
    {
        const EP::ExpandedPlayConfig cfg{};
        assert(!cfg.enabled);
        assert(!cfg.moduleMatureReactions);
        assert(cfg.moduleSocialReactions);
        assert(cfg.moduleRelationshipMemory);
        assert(cfg.moduleBookStudy);
        assert(cfg.modulePresenceProgression);
        assert(cfg.demeanorMode == EP::DemeanorMode::Hybrid);
        assert(cfg.activeDemeanor == EP::Demeanor::Courteous);
        assert(cfg.enableReticle);
        assert(std::abs(cfg.reticleOpacity - 0.22f) < 1e-6f);
        assert(std::abs(cfg.reticleMagnetismStrength - 0.35f) < 1e-6f);
        assert(cfg.colorBlindMode == 0);
        assert(!cfg.immersiveHideReticle);
        assert(cfg.enableEyeContactInitiate);
        assert(cfg.enableImpressionWhispers);
        assert(std::abs(cfg.thirdPersonEfficacy - 0.80f) < 1e-6f);
        assert(cfg.emitAiNpcBridgeEvents);

        const EP::PresenceState presence{};
        assert(std::abs(presence.current - 20.0f) < 1e-6f);
        assert(std::abs(presence.maximum - 20.0f) < 1e-6f);
        assert(!presence.canticleAwakened);
        assert(!presence.timidDebuff);

        const EP::GazeLedgerEntry ledger{};
        assert(ledger.notableEvents.size() == 8);
        assert(ledger.eventCount == 0);
        assert(!ledger.recognized);
    }

    void TestDemeanorParsingAndNames()
    {
        bool valid = false;
        assert(EP::ParseDemeanorMode("Manual", &valid) == EP::DemeanorMode::Manual && valid);
        assert(EP::ParseDemeanorMode("  manual \t", &valid) == EP::DemeanorMode::Manual && valid);
        assert(EP::ParseDemeanorMode("AUTO", &valid) == EP::DemeanorMode::Auto && valid);
        assert(EP::ParseDemeanorMode("HyBrId", &valid) == EP::DemeanorMode::Hybrid && valid);

        assert(EP::ParseDemeanorMode("", &valid) == EP::DemeanorMode::Hybrid && !valid);
        assert(EP::ParseDemeanorMode("SemiAuto", &valid) == EP::DemeanorMode::Hybrid && !valid);

        assert(std::strcmp(EP::DemeanorName(EP::Demeanor::Courteous), "Courteous") == 0);
        assert(std::strcmp(EP::DemeanorName(EP::Demeanor::Predatory), "Predatory") == 0);
        assert(std::strcmp(EP::DemeanorModeName(EP::DemeanorMode::Hybrid), "Hybrid") == 0);
        assert(std::strcmp(EP::DwellBandName(EP::DwellBand::Fixation), "Fixation") == 0);
        assert(std::strcmp(EP::InterpretationName(EP::Interpretation::IntrusionLeer),
                           "IntrusionLeer") == 0);
        assert(std::strcmp(EP::RelationshipTierName(EP::RelationshipTier::Spouse), "Spouse") == 0);
        assert(std::strcmp(EP::PresenceTierName(EP::PresenceTier::Legendary), "Legendary") == 0);
        assert(std::strcmp(EP::ScholarshipRankName(EP::ScholarshipRank::Canticle), "Canticle") == 0);
    }

    void TestDwellClassification()
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float inf = std::numeric_limits<float>::infinity();

        assert(EP::ClassifyDwellBand(nan) == EP::DwellBand::None);
        assert(EP::ClassifyDwellBand(inf) == EP::DwellBand::None);
        assert(EP::ClassifyDwellBand(-inf) == EP::DwellBand::None);
        assert(EP::ClassifyDwellBand(-1.0f) == EP::DwellBand::None);
        assert(EP::ClassifyDwellBand(0.0f) == EP::DwellBand::None);
        assert(EP::ClassifyDwellBand(0.079f) == EP::DwellBand::None);

        assert(EP::ClassifyDwellBand(0.08f) == EP::DwellBand::Glance);
        assert(EP::ClassifyDwellBand(0.59f) == EP::DwellBand::Glance);
        assert(EP::ClassifyDwellBand(0.60f) == EP::DwellBand::Look);
        assert(EP::ClassifyDwellBand(1.99f) == EP::DwellBand::Look);
        assert(EP::ClassifyDwellBand(2.00f) == EP::DwellBand::Gaze);
        assert(EP::ClassifyDwellBand(4.49f) == EP::DwellBand::Gaze);
        assert(EP::ClassifyDwellBand(4.50f) == EP::DwellBand::Stare);
        assert(EP::ClassifyDwellBand(8.99f) == EP::DwellBand::Stare);
        assert(EP::ClassifyDwellBand(9.00f) == EP::DwellBand::Fixation);
        assert(EP::ClassifyDwellBand(30.0f) == EP::DwellBand::Fixation);

        // Rapid escalation (non-face intrusive checks)
        assert(EP::ClassifyDwellBand(0.079f, true) == EP::DwellBand::None);
        assert(EP::ClassifyDwellBand(0.08f, true) == EP::DwellBand::Glance);
        assert(EP::ClassifyDwellBand(0.44f, true) == EP::DwellBand::Glance);
        assert(EP::ClassifyDwellBand(0.45f, true) == EP::DwellBand::Look);
        assert(EP::ClassifyDwellBand(1.49f, true) == EP::DwellBand::Look);
        assert(EP::ClassifyDwellBand(1.50f, true) == EP::DwellBand::Gaze);
        assert(EP::ClassifyDwellBand(2.99f, true) == EP::DwellBand::Gaze);
        assert(EP::ClassifyDwellBand(3.00f, true) == EP::DwellBand::Stare);
        assert(EP::ClassifyDwellBand(5.99f, true) == EP::DwellBand::Stare);
        assert(EP::ClassifyDwellBand(6.00f, true) == EP::DwellBand::Fixation);
    }

    void TestProgressionTiers()
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float inf = std::numeric_limits<float>::infinity();

        assert(EP::ClassifyPresenceTier(nan) == EP::PresenceTier::Unnoticed);
        assert(EP::ClassifyPresenceTier(inf) == EP::PresenceTier::Unnoticed);
        assert(EP::ClassifyPresenceTier(-5.0f) == EP::PresenceTier::Unnoticed);
        assert(EP::ClassifyPresenceTier(19.99f) == EP::PresenceTier::Unnoticed);
        assert(EP::ClassifyPresenceTier(20.0f) == EP::PresenceTier::Noticed);
        assert(EP::ClassifyPresenceTier(39.99f) == EP::PresenceTier::Noticed);
        assert(EP::ClassifyPresenceTier(40.0f) == EP::PresenceTier::Steady);
        assert(EP::ClassifyPresenceTier(59.99f) == EP::PresenceTier::Steady);
        assert(EP::ClassifyPresenceTier(60.0f) == EP::PresenceTier::Commanding);
        assert(EP::ClassifyPresenceTier(79.99f) == EP::PresenceTier::Commanding);
        assert(EP::ClassifyPresenceTier(80.0f) == EP::PresenceTier::Legendary);
        assert(EP::ClassifyPresenceTier(100.0f) == EP::PresenceTier::Legendary);

        assert(EP::ClassifyScholarshipRank(0) == EP::ScholarshipRank::Novice);
        assert(EP::ClassifyScholarshipRank(4) == EP::ScholarshipRank::Novice);
        assert(EP::ClassifyScholarshipRank(5) == EP::ScholarshipRank::Reader);
        assert(EP::ClassifyScholarshipRank(14) == EP::ScholarshipRank::Reader);
        assert(EP::ClassifyScholarshipRank(15) == EP::ScholarshipRank::Acolyte);
        assert(EP::ClassifyScholarshipRank(34) == EP::ScholarshipRank::Acolyte);
        assert(EP::ClassifyScholarshipRank(35) == EP::ScholarshipRank::Scholar);
        assert(EP::ClassifyScholarshipRank(74) == EP::ScholarshipRank::Scholar);
        assert(EP::ClassifyScholarshipRank(75) == EP::ScholarshipRank::Prelate);
        assert(EP::ClassifyScholarshipRank(149) == EP::ScholarshipRank::Prelate);
        assert(EP::ClassifyScholarshipRank(150) == EP::ScholarshipRank::Canticle);
    }

    void TestConfigSanitisation()
    {
        EP::ExpandedPlayConfig cfg{};
        cfg.demeanorMode = static_cast<EP::DemeanorMode>(99);
        cfg.activeDemeanor = static_cast<EP::Demeanor>(42);
        cfg.reticleOpacity = -2.0f;
        cfg.reticleMagnetismStrength = 5.0f;
        cfg.colorBlindMode = 99;
        cfg.thirdPersonEfficacy = 0.10f;
        cfg.Sanitise();

        assert(cfg.demeanorMode == EP::DemeanorMode::Hybrid);
        assert(cfg.activeDemeanor == EP::Demeanor::Courteous);
        assert(std::abs(cfg.reticleOpacity - 0.0f) < 1e-6f);
        assert(std::abs(cfg.reticleMagnetismStrength - 1.0f) < 1e-6f);
        assert(cfg.colorBlindMode == 3);
        assert(std::abs(cfg.thirdPersonEfficacy - 0.50f) < 1e-6f);

        cfg.reticleOpacity = std::numeric_limits<float>::quiet_NaN();
        cfg.reticleMagnetismStrength = std::numeric_limits<float>::infinity();
        cfg.thirdPersonEfficacy = -std::numeric_limits<float>::infinity();
        cfg.colorBlindMode = -4;
        cfg.Sanitise();

        assert(std::isfinite(cfg.reticleOpacity) && std::abs(cfg.reticleOpacity - 0.22f) < 1e-6f);
        assert(std::isfinite(cfg.reticleMagnetismStrength) &&
               std::abs(cfg.reticleMagnetismStrength - 0.35f) < 1e-6f);
        assert(std::isfinite(cfg.thirdPersonEfficacy) &&
               std::abs(cfg.thirdPersonEfficacy - 0.80f) < 1e-6f);
        assert(cfg.colorBlindMode == 0);
    }

    void TestCoSaveSignRoundTripAndSanitisation()
    {
        EP::PersistentState ps{};
        assert(!ps.HasMeaningfulState());

        EP::SignRecordData src{};
        src.presence.canticleAwakened     = true;
        src.presence.current              = 31.5f;
        src.presence.maximum              = 47.0f;
        src.presence.shakenUntilGameDays  = 12.25f;
        src.presence.timidDebuff          = true;
        src.presence.dailyGainAccumulated = 2.5f;
        src.presence.lastGainGameDay      = 10.0f;
        src.signature                     = {42.0f, -15.5f, 18.0f, 61.0f};
        src.activeDemeanor                = EP::Demeanor::Stern;
        src.demeanorMode                  = EP::DemeanorMode::Manual;
        src.booksFullyStudiedCount        = 17u;
        src.skillBoostActorValue          = 21u;
        src.skillBoostExpiresGameDays     = 14.5f;

        const auto bytes = EP::EncodeSignRecord(src);
        EP::SignRecordData dst{};
        assert(EP::DecodeSignRecord(bytes, EP::kSerializationVersion, dst));
        assert(dst.presence.canticleAwakened);
        assert(std::abs(dst.presence.current - 31.5f) < 1e-5f);
        assert(std::abs(dst.presence.maximum - 47.0f) < 1e-5f);
        assert(dst.presence.timidDebuff);
        assert(std::abs(dst.signature.warmth - 42.0f) < 1e-5f);
        assert(std::abs(dst.signature.dominance + 15.5f) < 1e-5f);
        assert(std::abs(dst.signature.candour - 18.0f) < 1e-5f);
        assert(std::abs(dst.signature.restraint - 61.0f) < 1e-5f);
        assert(dst.activeDemeanor == EP::Demeanor::Stern);
        assert(dst.demeanorMode == EP::DemeanorMode::Manual);
        assert(dst.booksFullyStudiedCount == 17u);
        assert(dst.skillBoostActorValue == 21u);

        // Wrong version & truncated payloads must reject cleanly.
        assert(!EP::DecodeSignRecord(bytes, 99u, dst));
        assert(!EP::DecodeSignRecord({bytes.data(), bytes.size() - 1u}, EP::kSerializationVersion,
                                     dst));

        // Corrupt out-of-range / NaN floats & invalid enums must clamp safely.
        EP::SignRecordData dirty{};
        dirty.presence.maximum      = 250.0f;
        dirty.presence.current      = 999.0f;
        dirty.signature.warmth      = std::numeric_limits<float>::quiet_NaN();
        dirty.signature.restraint   = -900.0f;
        dirty.activeDemeanor        = static_cast<EP::Demeanor>(88);
        dirty.demeanorMode          = static_cast<EP::DemeanorMode>(77);
        const auto dirtyBytes = EP::EncodeSignRecord(dirty);
        EP::SignRecordData cleaned{};
        assert(EP::DecodeSignRecord(dirtyBytes, EP::kSerializationVersion, cleaned));
        assert(std::abs(cleaned.presence.maximum - 100.0f) < 1e-5f);
        assert(std::abs(cleaned.presence.current - 100.0f) < 1e-5f);
        assert(std::abs(cleaned.signature.warmth - 0.0f) < 1e-5f);
        assert(std::abs(cleaned.signature.restraint + 100.0f) < 1e-5f);
        assert(cleaned.activeDemeanor == EP::Demeanor::Courteous);
        assert(cleaned.demeanorMode == EP::DemeanorMode::Hybrid);

        ps.sign = dst;
        assert(ps.HasMeaningfulState());
        ps.Clear();
        assert(!ps.HasMeaningfulState());
    }

    void TestCoSaveLedgerRoundTripAndFormIdResolution()
    {
        EP::GazeLedgerEntry camilla{};
        camilla.npcFormId        = 0x0001347Bu;
        camilla.lastSeenGameDays = 5.5f;
        camilla.lruSequence      = 42u;
        camilla.impressions      = {25.0f, 40.0f, -5.0f, 0.0f, 12.0f, 0.0f};
        camilla.permanentFloor   = {5.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        camilla.recognized       = true;
        camilla.eventCount       = 2u;
        camilla.eventHead        = 2u;
        camilla.notableEvents[0] = {EP::NotableEventType::FirstMutualGaze, 4.0f, 0x0001677Au, 0u};
        camilla.notableEvents[1] = {EP::NotableEventType::TriangleWitnessed, 5.5f, 0x0001677Au,
                                    0x00013492u};

        EP::GazeLedgerEntry missingModNpc{};
        missingModNpc.npcFormId        = 0xFE005123u;
        missingModNpc.lastSeenGameDays = 6.0f;
        missingModNpc.lruSequence      = 43u;

        const std::vector<EP::GazeLedgerEntry> src{camilla, missingModNpc};
        const auto bytes = EP::EncodeLedgerRecord(src);

        // Simulate uninstalled ESP/ESL: 0xFE005123 fails resolution, Camilla & Sven remap.
        const EP::FormIdResolver resolver = [](uint32_t oldId, uint32_t& newId) -> bool
        {
            if (oldId == 0xFE005123u)
            {
                return false;
            }
            if (oldId == 0x00013492u)
            {
                newId = 0x00013493u;
                return true;
            }
            newId = oldId;
            return true;
        };

        std::vector<EP::GazeLedgerEntry> out;
        assert(EP::DecodeLedgerRecord(bytes, EP::kSerializationVersion, out, resolver));
        assert(out.size() == 1u);
        assert(out[0].npcFormId == 0x0001347Bu);
        assert(out[0].recognized);
        assert(out[0].eventCount == 2u);
        assert(out[0].notableEvents[0].type == EP::NotableEventType::FirstMutualGaze);
        assert(out[0].notableEvents[1].thirdPartyFormId == 0x00013493u);
        assert(std::abs(out[0].impressions.warmth - 40.0f) < 1e-5f);

        // Truncated ledger payload must fail cleanly.
        assert(!EP::DecodeLedgerRecord({bytes.data(), bytes.size() - 2u}, EP::kSerializationVersion,
                                       out, resolver));
    }

    void TestCoSaveBookRoundTripAndTruncationSafety()
    {
        EP::BookStudyEntry canticle{};
        canticle.bookFormId          = 0x05001001u;
        canticle.pagesReadMask       = 0b1011ULL;
        canticle.totalSpreads        = 4u;
        canticle.fullStudyAwarded    = false;
        canticle.lastStudiedGameDays = 7.75f;

        EP::BookStudyEntry missingBook{};
        missingBook.bookFormId    = 0xFE012345u;
        missingBook.pagesReadMask = 0xFFULL;
        missingBook.totalSpreads  = 8u;

        const std::vector<EP::BookStudyEntry> src{canticle, missingBook};
        const auto bytes = EP::EncodeBookRecord(src);

        const EP::FormIdResolver resolver = [](uint32_t oldId, uint32_t& newId) -> bool
        {
            if (oldId == 0xFE012345u)
            {
                return false;
            }
            newId = (oldId == 0x05001001u) ? 0x06001001u : oldId;
            return true;
        };

        std::vector<EP::BookStudyEntry> out;
        assert(EP::DecodeBookRecord(bytes, EP::kSerializationVersion, out, resolver));
        assert(out.size() == 1u);
        assert(out[0].bookFormId == 0x06001001u);
        assert(out[0].pagesReadMask == 0b1011ULL);
        assert(out[0].totalSpreads == 4u);
        assert(!out[0].fullStudyAwarded);
        assert(std::abs(out[0].lastStudiedGameDays - 7.75f) < 1e-5f);

        // Corrupt oversized count must fail immediately without OOM.
        std::vector<uint8_t> corrupt{0xFFu, 0xFFu, 0xFFu, 0xFFu};
        assert(!EP::DecodeBookRecord(corrupt, EP::kSerializationVersion, out));
        assert(!EP::DecodeBookRecord({bytes.data(), bytes.size() - 1u}, EP::kSerializationVersion,
                                     out));
    }

    void TestModifierLedgerAndScholarshipBonuses()
    {
        const auto novice = EP::ComputeScholarshipBonus(EP::ScholarshipRank::Novice);
        assert(novice.magickaRateMultBonus == 0.0f && novice.presenceRegenBonusPct == 0.0f &&
               novice.speechIllusionBonus == 0.0f);

        const auto canticle = EP::ComputeScholarshipBonus(EP::ScholarshipRank::Canticle);
        assert(std::abs(canticle.magickaRateMultBonus - 15.0f) < 1e-5f);
        assert(std::abs(canticle.presenceRegenBonusPct - 20.0f) < 1e-5f);
        assert(std::abs(canticle.speechIllusionBonus - 10.0f) < 1e-5f);

        EP::ModifierLedger ledger;
        assert(ledger.ActiveCount() == 0u);

        // Initial application & idempotent repeat
        assert(std::abs(ledger.SetTargetDelta(0x14u, 156, 6.0f) - 6.0f) < 1e-5f);
        assert(ledger.ActiveCount() == 1u);
        assert(std::abs(ledger.SetTargetDelta(0x14u, 156, 6.0f) - 0.0f) < 1e-5f);

        // Upgrade from 6.0 -> 9.0 applies only +3.0 delta
        assert(std::abs(ledger.SetTargetDelta(0x14u, 156, 9.0f) - 3.0f) < 1e-5f);
        assert(std::abs(ledger.SetTargetDelta(0x14u, 107, 4.0f) - 4.0f) < 1e-5f);
        assert(ledger.ActiveCount() == 2u);

        // Teardown strips exact inverse (-9.0 and -4.0) and clears ledger
        const auto teardown = ledger.CollectTeardownDeltas();
        assert(teardown.size() == 2u);
        assert(ledger.ActiveCount() == 0u);
        assert(teardown[0].actorValueId == 156 && std::abs(teardown[0].delta + 9.0f) < 1e-5f);
        assert(teardown[1].actorValueId == 107 && std::abs(teardown[1].delta + 4.0f) < 1e-5f);
    }

    void TestDemeanorControllerAutoPriorityAndHybridPin()
    {
        // Priority 1: Court/Superior without Thane -> Deferential
        EP::DemeanorContext ctx{};
        ctx.inJarlCourtOrTemple = true;
        ctx.weaponDrawn = true; // Court etiquette wins over weapon check unless Thane
        assert(EP::InferAutoDemeanor(ctx) == EP::Demeanor::Deferential);

        ctx.playerIsThane = true; // Thane unlocks Stern/Courteous in court
        assert(EP::InferAutoDemeanor(ctx) == EP::Demeanor::Stern);

        // Priority 3: Sneaking / stolen goods -> Evasive
        ctx = {};
        ctx.sneaking = true;
        ctx.targetRelationship = EP::RelationshipTier::Spouse;
        assert(EP::InferAutoDemeanor(ctx) == EP::Demeanor::Evasive);

        // Priority 4: Spouse/Partner/Friend -> Warm
        ctx.sneaking = false;
        assert(EP::InferAutoDemeanor(ctx) == EP::Demeanor::Warm);

        // Priority 5: Unlocked Predatory in DB armor (blocked in towns & never on children)
        ctx = {};
        ctx.wearingDarkBrotherhoodArmor = true;
        ctx.predatoryUnlocked = true;
        ctx.inPeacefulTown = true;
        assert(EP::InferAutoDemeanor(ctx) == EP::Demeanor::Courteous);
        ctx.inPeacefulTown = false;
        assert(EP::InferAutoDemeanor(ctx) == EP::Demeanor::Predatory);
        ctx.targetIsChild = true;
        assert(EP::InferAutoDemeanor(ctx) == EP::Demeanor::Courteous);

        // Hybrid pinning: stays active until 60s expiry or major context switch
        EP::DemeanorController ctrl;
        EP::DemeanorContext hybridCtx{};
        hybridCtx.currentLocationFormId = 0x0001677Au;
        hybridCtx.dialogueTargetFormId = 0x0001347Bu;
        ctrl.PinManualDemeanor(EP::Demeanor::Warm, hybridCtx);
        assert(ctrl.IsPinned());
        assert(ctrl.Update(EP::DemeanorMode::Hybrid, EP::Demeanor::Courteous, hybridCtx, 30.0f) ==
               EP::Demeanor::Warm);
        assert(ctrl.IsPinned() && std::abs(ctrl.PinRemainingSec() - 30.0f) < 1e-4f);

        // Ending dialogue clears pin immediately -> reverts to Auto (Courteous)
        hybridCtx.dialogueTargetFormId = 0u;
        bool changed = false;
        assert(ctrl.Update(EP::DemeanorMode::Hybrid, EP::Demeanor::Courteous, hybridCtx, 0.1f,
                           &changed) == EP::Demeanor::Courteous);
        assert(changed && !ctrl.IsPinned());

        // Child safety overrides manual Predatory selection too
        hybridCtx.targetIsChild = true;
        assert(ctrl.Update(EP::DemeanorMode::Manual, EP::Demeanor::Predatory, hybridCtx, 0.1f) ==
               EP::Demeanor::Courteous);

        // Efficacy scaling
        const auto p3 = EP::BuildDemeanorProfile(EP::Demeanor::Warm, true, 0.80f);
        const auto p1 = EP::BuildDemeanorProfile(EP::Demeanor::Warm, false, 0.80f);
        assert(std::abs(p3.efficacyMultiplier - 0.80f) < 1e-5f);
        assert(std::abs(p1.efficacyMultiplier - 1.00f) < 1e-5f);
    }

    void TestGazeReticlePaletteArcAndMagnetism()
    {
        // Canonical HCEP-02 region colors + color-blind palettes + shapes
        assert(EP::RegionColorRgb(0, 0) == 0x33FF59u); // LeftEye
        assert(EP::RegionColorRgb(1, 0) == 0xFF9926u); // RightEye
        assert(EP::RegionColorRgb(2, 0) == 0xFF59BFu); // Mouth
        assert(EP::RegionColorRgb(3, 0) == 0xD966FFu); // Forehead
        assert(EP::RegionColorRgb(5, 0) == 0xFF2633u); // Torso
        assert(EP::RegionColorRgb(8, 0) == 0x2659E6u); // Ground
        assert(EP::RegionColorRgb(0, 1) != EP::RegionColorRgb(5, 1));
        assert(EP::RegionColorRgb(0, 2) != EP::RegionColorRgb(5, 2));
        assert(EP::RegionColorRgb(0, 3) != EP::RegionColorRgb(5, 3));

        assert(EP::RegionGlyphShape(0) == EP::ReticleGlyphShape::Diamond);
        assert(EP::RegionGlyphShape(2) == EP::ReticleGlyphShape::Circle);
        assert(EP::RegionGlyphShape(3) == EP::ReticleGlyphShape::Triangle);
        assert(EP::RegionGlyphShape(5) == EP::ReticleGlyphShape::Square);

        // Dwell arc quarter alignment at threshold crossings
        assert(std::abs(EP::ComputeDwellArcState(0.60f).progress01 - 0.25f) < 1e-5f);
        assert(std::abs(EP::ComputeDwellArcState(2.00f).progress01 - 0.50f) < 1e-5f);
        assert(std::abs(EP::ComputeDwellArcState(4.50f).progress01 - 0.75f) < 1e-5f);
        assert(std::abs(EP::ComputeDwellArcState(9.00f).progress01 - 1.00f) < 1e-5f);

        // Reciprocity pulse fires on rising edge and decays cleanly
        EP::ReciprocityPulseState pulse;
        assert(std::abs(pulse.Step(false, 0.1f)) < 1e-5f);
        assert(std::abs(pulse.Step(true, 0.0f) - 1.0f) < 1e-5f);
        assert(pulse.Step(true, 0.225f) > 0.45f && pulse.Step(true, 0.0f) < 0.55f);
        assert(std::abs(pulse.Step(true, 0.50f)) < 1e-5f);

        // Soft magnetism active inside 2 deg only when unarmed / non-combat
        const auto mOk = EP::ComputeSoftMagnetismOffset(1.0f, 3.0f, 4.0f, 0.5f, false);
        assert(mOk.active && mOk.offsetX > 0.0f && mOk.offsetY > 0.0f);
        assert(!EP::ComputeSoftMagnetismOffset(2.5f, 1.0f, 0.0f, 0.5f, false).active);
        assert(!EP::ComputeSoftMagnetismOffset(1.0f, 1.0f, 0.0f, 0.5f, true).active);
    }

    void TestPerceptionGateVisibilityAndOcclusion()
    {
        EP::PerceptionInput base{};
        base.distanceMeters = 3.0f;
        const auto ok = EP::PerceptionGate::Evaluate(base);
        assert(ok.perceived && ok.regionSpecific);
        assert(ok.reason == EP::PerceptionReason::Perceived);
        assert(std::abs(ok.intensity - 1.0f) < 1e-5f);
        assert(std::abs(ok.facialBondScale - 1.0f) < 1e-5f);

        // Unaware / combat / range / LOS / hemisphere / invalid gates
        {
            auto dead = base;
            dead.observerAlive = false;
            const auto r = EP::PerceptionGate::Evaluate(dead);
            assert(!r.perceived && r.reason == EP::PerceptionReason::Unaware);

            auto asleep = base;
            asleep.observerSleeping = true;
            assert(EP::PerceptionGate::Evaluate(asleep).reason == EP::PerceptionReason::Unaware);

            auto ragdoll = base;
            ragdoll.observerRagdolled = true;
            assert(EP::PerceptionGate::Evaluate(ragdoll).reason == EP::PerceptionReason::Unaware);

            auto combat = base;
            combat.observerInCombat = true;
            assert(EP::PerceptionGate::Evaluate(combat).reason == EP::PerceptionReason::InCombat);

            auto tooFar = base;
            tooFar.distanceMeters = 15.5f;
            assert(EP::PerceptionGate::Evaluate(tooFar).reason == EP::PerceptionReason::OutOfRange);

            auto blocked = base;
            blocked.hasLineOfSight = false;
            assert(EP::PerceptionGate::Evaluate(blocked).reason ==
                   EP::PerceptionReason::NoLineOfSight);

            auto behind = base;
            behind.dirToPlayerY = -1.0f;
            assert(EP::PerceptionGate::Evaluate(behind).reason ==
                   EP::PerceptionReason::BehindObserver);

            auto badVec = base;
            badVec.observerForwardY = 0.0f;
            assert(EP::PerceptionGate::Evaluate(badVec).reason ==
                   EP::PerceptionReason::InvalidInput);

            auto badDist = base;
            badDist.distanceMeters = std::numeric_limits<float>::quiet_NaN();
            assert(EP::PerceptionGate::Evaluate(badDist).reason ==
                   EP::PerceptionReason::InvalidInput);
        }

        // Region specificity boundary at 8 m vs presence out to 15 m
        {
            auto atEight = base;
            atEight.distanceMeters = 8.0f;
            const auto r8 = EP::PerceptionGate::Evaluate(atEight);
            assert(r8.perceived && r8.regionSpecific);

            auto atEleven = base;
            atEleven.distanceMeters = 11.0f;
            const auto r11 = EP::PerceptionGate::Evaluate(atEleven);
            assert(r11.perceived && !r11.regionSpecific && r11.intensity > 0.45f);
        }

        // Scripted scene halves intensity
        {
            auto scene = base;
            scene.observerInScene = true;
            const auto rs = EP::PerceptionGate::Evaluate(scene);
            assert(rs.perceived && std::abs(rs.intensity - 0.50f) < 1e-5f);
        }

        // Lighting & Night-Eye floor
        {
            auto dark = base;
            dark.lightLevel01 = 0.0f;
            assert(std::abs(EP::PerceptionGate::Evaluate(dark).intensity - 0.25f) < 1e-5f);

            dark.observerHasNightEye = true;
            assert(std::abs(EP::PerceptionGate::Evaluate(dark).intensity - 0.85f) < 1e-5f);
        }

        // Helmet & hood facial bond attenuation
        {
            auto helmet = base;
            helmet.playerFullFaceHelmet = true;
            assert(std::abs(EP::PerceptionGate::Evaluate(helmet).facialBondScale - 0.35f) < 1e-5f);

            auto hood = base;
            hood.playerHooded = true;
            assert(std::abs(EP::PerceptionGate::Evaluate(hood).facialBondScale - 0.70f) < 1e-5f);
        }
    }

    void TestInterpretationEngineMatrixAndChildSafety()
    {
        // 1. Exhaustive Child Safety Sweep (§11.3): Attraction & IntrusionLeer unreachable
        constexpr float kDwells[] = {0.1f, 0.7f, 2.5f, 3.5f, 5.0f, 10.0f};
        for (uint8_t region = 0; region <= EP::Region::LRPeriph; ++region)
        {
            for (float dwell : kDwells)
            {
                for (uint8_t t = 0; t <= static_cast<uint8_t>(EP::RelationshipTier::Subordinate); ++t)
                {
                    EP::InterpretationContext childCtx{};
                    childCtx.isChild = true;
                    childCtx.matureReactionsEnabled = true;
                    childCtx.isMutual = true;
                    childCtx.tier = static_cast<EP::RelationshipTier>(t);

                    const auto res = EP::InterpretationEngine::EvaluateLook(region, dwell, childCtx);
                    assert(res.interpretation != EP::Interpretation::Attraction);
                    assert(res.interpretation != EP::Interpretation::IntrusionLeer);
                    if (EP::IsBodyRegion(region))
                    {
                        assert(res.interpretation == EP::Interpretation::None);
                    }
                }
            }
        }

        // 2. Adult Torso disambiguation
        {
            EP::InterpretationContext ctx{};
            ctx.matureReactionsEnabled = true;
            ctx.compassionContext = true;
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::Torso, 1.0f, ctx)
                       .interpretation == EP::Interpretation::Compassion);

            ctx.compassionContext = false;
            ctx.emblemContext = true;
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::Torso, 1.0f, ctx)
                       .interpretation == EP::Interpretation::Recognition);

            ctx.emblemContext = false;
            ctx.tier = EP::RelationshipTier::Spouse;
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::Torso, 2.5f, ctx)
                       .interpretation == EP::Interpretation::Attraction);

            ctx.tier = EP::RelationshipTier::Stranger;
            ctx.matureReactionsEnabled = false;
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::Torso, 4.0f, ctx)
                       .interpretation == EP::Interpretation::None);

            ctx.matureReactionsEnabled = true;
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::Torso, 3.1f, ctx)
                       .interpretation == EP::Interpretation::IntrusionLeer);

            // Beyond 8 m (regionSpecific == false): Torso/Mouth/Hands produce None
            ctx.regionSpecific = false;
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::Torso, 5.0f, ctx)
                       .interpretation == EP::Interpretation::None);
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::Mouth, 3.0f, ctx)
                       .interpretation == EP::Interpretation::None);
        }

        // 3. Relationship & Etiquette matrix
        {
            EP::InterpretationContext ctx{};
            ctx.isMutual = true;
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::LeftEye, 3.0f, ctx)
                       .interpretation == EP::Interpretation::Respect);
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::LeftEye, 6.0f, ctx)
                       .interpretation == EP::Interpretation::Challenge);

            ctx.tier = EP::RelationshipTier::Friend;
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::LeftEye, 3.0f, ctx)
                       .interpretation == EP::Interpretation::Warmth);

            ctx.tier = EP::RelationshipTier::Spouse;
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::LeftEye, 3.0f, ctx)
                       .interpretation == EP::Interpretation::Attraction);

            ctx.tier = EP::RelationshipTier::Rival;
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::LeftEye, 3.0f, ctx)
                       .interpretation == EP::Interpretation::Challenge);

            ctx.tier = EP::RelationshipTier::Superior;
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::Ground, 1.0f, ctx)
                       .interpretation == EP::Interpretation::Deference);
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::Chin, 1.0f, ctx)
                       .interpretation == EP::Interpretation::Deference);
            ctx.playerIsThane = false;
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::LeftEye, 3.0f, ctx)
                       .interpretation == EP::Interpretation::Challenge);
            ctx.playerIsThane = true;
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::LeftEye, 3.0f, ctx)
                       .interpretation == EP::Interpretation::Respect);
        }

        // 4. Culture & Helmet Occlusion
        {
            EP::InterpretationContext genericCtx{};
            genericCtx.perceptionIntensity = 0.50f;
            const auto genLook =
                EP::InterpretationEngine::EvaluateLook(EP::Region::LeftEye, 1.0f, genericCtx);

            auto nordCtx = genericCtx;
            nordCtx.culture = EP::ObserverCulture::Nord;
            const auto nordLook =
                EP::InterpretationEngine::EvaluateLook(EP::Region::LeftEye, 1.0f, nordCtx);
            assert(nordLook.interpretation == EP::Interpretation::Respect &&
                   nordLook.strength > genLook.strength);

            auto altmerCtx = genericCtx;
            altmerCtx.culture = EP::ObserverCulture::Altmer;
            altmerCtx.playerIsThane = false;
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::LeftEye, 3.0f, altmerCtx)
                       .interpretation == EP::Interpretation::Challenge);

            auto orcCtx = genericCtx;
            orcCtx.culture = EP::ObserverCulture::Orc;
            const auto genStare =
                EP::InterpretationEngine::EvaluateLook(EP::Region::LeftEye, 5.0f, genericCtx);
            const auto orcStare =
                EP::InterpretationEngine::EvaluateLook(EP::Region::LeftEye, 5.0f, orcCtx);
            assert(orcStare.interpretation == EP::Interpretation::Challenge &&
                   orcStare.strength > genStare.strength);

            auto khajiitCtx = genericCtx;
            khajiitCtx.culture = EP::ObserverCulture::Khajiit;
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::LeftEye, 5.0f, khajiitCtx)
                       .interpretation == EP::Interpretation::Respect);

            auto argonianCtx = genericCtx;
            argonianCtx.culture = EP::ObserverCulture::Argonian;
            const auto argLook =
                EP::InterpretationEngine::EvaluateLook(EP::Region::LeftEye, 1.0f, argonianCtx);
            assert(argLook.strength < genLook.strength);

            // Helmet scales Warmth (0.35x) while leaving Challenge unattenuated
            EP::InterpretationContext friendBare{};
            friendBare.tier = EP::RelationshipTier::Friend;
            friendBare.isMutual = true;
            auto friendHelmet = friendBare;
            friendHelmet.facialBondScale = 0.35f;
            const auto warmBare =
                EP::InterpretationEngine::EvaluateLook(EP::Region::LeftEye, 3.0f, friendBare);
            const auto warmHelm =
                EP::InterpretationEngine::EvaluateLook(EP::Region::LeftEye, 3.0f, friendHelmet);
            assert(warmBare.interpretation == EP::Interpretation::Warmth &&
                   warmHelm.interpretation == EP::Interpretation::Warmth &&
                   std::abs(warmHelm.strength - warmBare.strength * 0.35f) < 1e-4f);

            EP::InterpretationContext strangerHelm{};
            strangerHelm.facialBondScale = 0.35f;
            const auto chalHelm =
                EP::InterpretationEngine::EvaluateLook(EP::Region::LeftEye, 5.0f, strangerHelm);
            assert(chalHelm.interpretation == EP::Interpretation::Challenge &&
                   std::abs(chalHelm.strength - 0.80f) < 1e-4f);
        }

        // 5. Aversion & Hands
        {
            EP::InterpretationContext ctx{};
            ctx.playerMakingClaim = true;
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::LRPeriph, 1.0f, ctx)
                       .interpretation == EP::Interpretation::Deceit);

            ctx.playerMakingClaim = false;
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::LRPeriph, 1.0f, ctx)
                       .interpretation == EP::Interpretation::None);
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::LRPeriph, 2.5f, ctx)
                       .interpretation == EP::Interpretation::Suspicion);

            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::LeftHand, 1.0f, ctx)
                       .interpretation == EP::Interpretation::None);
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::LeftHand, 2.5f, ctx)
                       .interpretation == EP::Interpretation::Suspicion);
            assert(EP::InterpretationEngine::EvaluateLook(EP::Region::RightHand, 2.5f, ctx)
                       .interpretation == EP::Interpretation::Suspicion);
        }
    }

    void TestGazeLedgerLruDecayAndWitnessHooks()
    {
        EP::GazeLedger ledger;

        // Neutral stranger (< 5.0 max impression, no events, not mutual) stays transient only.
        ledger.RecordInterpretation(0x1001u, EP::Interpretation::Respect, 0.5f, 1.0f, 0u, false);
        assert(ledger.TransientCount() == 1u && ledger.PersistentCount() == 0u);
        assert(ledger.Find(0x1001u) != nullptr);

        // Mutual warmth promotes & sets FirstMutualGaze
        ledger.RecordInterpretation(0x1002u, EP::Interpretation::Warmth, 1.0f, 1.0f, 0x777u, true);
        assert(ledger.PersistentCount() == 1u);
        const auto* e2 = ledger.Find(0x1002u);
        assert(e2 != nullptr && e2->recognized && e2->eventCount == 1u &&
               e2->notableEvents[0].type == EP::NotableEventType::FirstMutualGaze);

        // Permanent floor preserves LeerPublic unease across 28 days of decay
        ledger.RecordInterpretation(0x1003u, EP::Interpretation::IntrusionLeer, 1.0f, 1.0f, 0x777u,
                                    false);
        auto copy = *ledger.Find(0x1003u);
        assert(copy.impressions.unease >= 12.0f);
        EP::GazeLedger::ApplyDecay(copy, 15.0f); // 1 half-life: decays toward floor 10.0
        assert(std::abs(copy.impressions.unease - 11.0f) < 0.05f);
        EP::GazeLedger::ApplyDecay(copy, 140.0f); // long decay stops at permanent floor 10.0
        assert(copy.impressions.unease >= 9.99f);

        // Spouse jealousy & Riverwood Sven/Camilla/Faendal triangle hook
        assert(ledger.RecordWitnessedInteraction(0x2001u, EP::WitnessRole::Spouse, 0x1002u,
                                                 EP::Interpretation::Attraction, 1.0f, 2.0f, 0x777u));
        const auto* spouse = ledger.Find(0x2001u);
        assert(spouse != nullptr && spouse->impressions.unease >= 15.0f &&
               spouse->notableEvents[0].type == EP::NotableEventType::SpouseJealousy);

        assert(ledger.RecordWitnessedInteraction(0x2002u, EP::WitnessRole::LoveTriangleRival,
                                                 0x1002u, EP::Interpretation::Warmth, 0.8f, 2.0f,
                                                 0x777u));
        const auto* rival = ledger.Find(0x2002u);
        assert(rival != nullptr && rival->impressions.suspicion > 5.0f &&
               rival->notableEvents[0].type == EP::NotableEventType::TriangleWitnessed);

        // 512-entry LRU eviction preserves newest entries and caps at 512
        EP::GazeLedger lru;
        for (uint32_t id = 1u; id <= 520u; ++id)
        {
            lru.RecordNotableEvent(0x9000u + id, EP::NotableEventType::StareDownWon,
                                   static_cast<float>(id), 0u, 0u);
        }
        assert(lru.PersistentCount() == EP::GazeLedger::kMaxPersistentEntries);
        assert(lru.Find(0x9001u) == nullptr);  // oldest evicted
        assert(lru.Find(0x9208u) != nullptr);  // newest retained
    }

    void TestPresenceSignatureDiminishingReturnsAndTitles()
    {
        EP::PresenceSignature ps;
        assert(ps.CurrentTier() == EP::PresenceTier::Noticed); // starts at 20.0

        // Daily diminishing returns: first 3.0 full, next 6.0 at 50% -> capped at 6.0/day
        const float g1 = ps.AwardMaxPresence(3.0f, 1.0f);
        const float g2 = ps.AwardMaxPresence(4.0f, 1.0f); // 4.0 * 0.5 = 2.0
        const float g3 = ps.AwardMaxPresence(5.0f, 1.0f); // room left = 1.0 (daily total 6.0)
        const float g4 = ps.AwardMaxPresence(5.0f, 1.0f); // capped at 0.0
        assert(std::abs(g1 - 3.0f) < 1e-4f);
        assert(std::abs(g2 - 2.0f) < 1e-4f);
        assert(std::abs(g3 - 1.0f) < 1e-4f);
        assert(std::abs(g4 - 0.0f) < 1e-4f);
        assert(std::abs(ps.Presence().maximum - 26.0f) < 1e-4f);

        // New game day resets daily budget
        const float gNextDay = ps.AwardMaxPresence(1.5f, 2.0f);
        assert(std::abs(gNextDay - 1.5f) < 1e-4f);

        // Stare-down loss applies 1-day shaken -0.5 penalty and -1.0 current
        const float curBefore = ps.Presence().current;
        ps.RecordStareDownOutcome(false, 2.0f);
        assert(std::abs(ps.Presence().current - (curBefore - 1.0f)) < 1e-4f);
        assert(std::abs(ps.EffectiveMaxPresence(2.5f) - (ps.Presence().maximum - 0.5f)) < 1e-4f);
        assert(std::abs(ps.EffectiveMaxPresence(3.1f) - ps.Presence().maximum) < 1e-4f);

        // One-shot Canticle Awakening grants +5.0 bypassing daily cap
        assert(ps.TriggerCanticleAwakening(2.0f));
        assert(!ps.TriggerCanticleAwakening(2.0f));

        // Signature titles
        assert(EP::EvaluateSignatureTitle({65.0f, 0.0f, 65.0f, 0.0f}) ==
               EP::SignatureTitle::MarasOwn);
        assert(EP::EvaluateSignatureTitle({-65.0f, 65.0f, 0.0f, 0.0f}) ==
               EP::SignatureTitle::IceGaze);
        assert(EP::EvaluateSignatureTitle({0.0f, 0.0f, -65.0f, 0.0f}) ==
               EP::SignatureTitle::QuickEyes);
        assert(EP::EvaluateSignatureTitle({0.0f, 0.0f, 0.0f, -65.0f}) ==
               EP::SignatureTitle::WanderingEyes);

        // Regen scales up with social setting & scholarship
        ps.RecordLeerCaught(2.0f);
        const float lowCur = ps.Presence().current;
        ps.TickRegeneration(1.0f, 3.5f, true, EP::ScholarshipRank::Canticle);
        assert(ps.Presence().current > lowCur);
    }

    void TestBookStudyTrackerDwellParsingAndRewards()
    {
        // Word-count dwell clamping
        assert(std::abs(EP::ComputeMinSpreadDwell(0u) - 2.5f) < 1e-5f);
        assert(std::abs(EP::ComputeMinSpreadDwell(26u) - 4.0f) < 1e-5f);
        assert(std::abs(EP::ComputeMinSpreadDwell(500u) - 12.0f) < 1e-5f);

        const auto spreads = EP::ParseBookSpreads(
            "<font face='$HandwrittenFont'>Page one words here.</font>[pagebreak]"
            "Page two has several words to read carefully.[PAGEBREAK]Final page.");
        assert(spreads.size() == 3u);
        assert(spreads[0].wordCount == 4u);
        assert(spreads[1].wordCount == 8u);
        assert(spreads[2].wordCount == 2u);

        // Session: speed-flip spread 0 (< 1.5s), read spread 1 & 2 slowly (>= 2.5s)
        EP::BookStudySession session;
        session.Begin(0x12345u,
                      "First page.[pagebreak]Second page.[pagebreak]Third page.",
                      true, 21u /*Illusion*/, true /*rare canticle*/);
        assert(session.IsActive() && session.SpreadCount() == 3u);

        session.RecordSpreadDwell(0u, 0.8f, false); // ignored speed-flip
        session.RecordSpreadDwell(1u, 3.0f, true);  // valid + True Reader scan
        session.RecordSpreadDwell(2u, 60.0f, false); // capped at 45s AFK, still valid (>= 2.5s)

        EP::BookStudyEntry entry{};
        uint32_t studiedBooks = 4u; // 4 -> 5 promotes to Reader rank!
        const auto s1 = session.Finalize(entry, studiedBooks, 5.0f);
        assert(s1.speedFlippedSpreads == 1u);
        assert(s1.newlyCreditedSpreads == 2u);
        assert(s1.totalStudiedSpreadsInBook == 2u);
        assert(!s1.fullStudyJustCompleted); // 2/3 = 66.7% < 80%
        assert(studiedBooks == 4u);

        // Re-open book and actually read spread 0 -> reaches 3/3 (100% >= 80%), unlocks rank!
        session.Begin(0x12345u,
                      "First page.[pagebreak]Second page.[pagebreak]Third page.",
                      true, 21u, true);
        session.RecordSpreadDwell(0u, 3.0f, false);
        const auto s2 = session.Finalize(entry, studiedBooks, 5.0f);
        assert(s2.newlyCreditedSpreads == 1u);
        assert(s2.fullStudyJustCompleted);
        assert(s2.skillBookDeepCraftGranted && s2.skillBoostActorValue == 21u);
        assert(studiedBooks == 5u);
        assert(s2.newScholarshipRank == EP::ScholarshipRank::Reader);
        assert(entry.fullStudyAwarded);

        // Immediate re-read (< 7 days) yields 0 duplicate credit; >= 7 days grants 25% refresh
        session.Begin(0x12345u,
                      "First page.[pagebreak]Second page.[pagebreak]Third page.",
                      true, 21u, true);
        session.RecordSpreadDwell(0u, 4.0f, false);
        const auto sDup = session.Finalize(entry, studiedBooks, 6.0f);
        assert(sDup.newlyCreditedSpreads == 0u && sDup.refreshedSpreads == 0u);

        session.Begin(0x12345u,
                      "First page.[pagebreak]Second page.[pagebreak]Third page.",
                      true, 21u, true);
        session.RecordSpreadDwell(0u, 4.0f, false);
        const auto sRefresh = session.Finalize(entry, studiedBooks, 13.0f);
        assert(sRefresh.refreshedSpreads == 1u && sRefresh.presenceAwarded > 0.0f);
    }

    void TestResponseLadderAndFirstSliceFeatures()
    {
        // 1. DominantImpressionScore positive and negative dominance
        {
            EP::Impressions pos{};
            pos.respect = 12.0f;
            pos.warmth = 28.0f;
            pos.unease = 10.0f;
            assert(std::abs(EP::DominantImpressionScore(pos) - 28.0f) < 1e-5f);

            EP::Impressions neg{};
            neg.respect = 15.0f;
            neg.unease = 32.0f;
            assert(std::abs(EP::DominantImpressionScore(neg) - (-32.0f)) < 1e-5f);
        }

        // 2. EvaluateResponseLadder Rungs 1-4 across key interpretations & toggles
        {
            EP::Impressions imp{};
            imp.warmth = 18.0f;

            const EP::InterpretationResult warm{
                EP::Interpretation::Warmth, 0.70f, EP::DwellBand::Gaze, true};
            const auto dw = EP::EvaluateResponseLadder(
                warm, EP::Demeanor::Warm, EP::PresenceTier::Steady, imp, true, true, true, true);
            assert(dw.rung1.applyModeOverride && dw.rung1.hcepMode == 1u);
            assert(dw.rung1.lockMutualReciprocity && !dw.rung1.forceAversion);
            assert(dw.rung2.applyExpression && dw.rung2.mood == EP::FaceExpressionMood::Happy);
            assert(dw.rung2.weight >= 0.15f && dw.rung2.weight <= 0.45f);
            assert(dw.rung3.interpretation == static_cast<uint8_t>(EP::Interpretation::Warmth));
            assert(dw.rung3.demeanor == static_cast<uint8_t>(EP::Demeanor::Warm));
            assert(dw.rung3.presenceTier == static_cast<uint8_t>(EP::PresenceTier::Steady));
            assert(std::abs(dw.rung3.dominantImpression - 18.0f) < 1e-5f);
            assert(dw.rung4.emitWhisper && dw.rung4.emitModEvent && dw.rung4.emitAiNpcBridge);

            // Cooldown false suppresses Rung 4 while preserving Rungs 1-3
            const auto dwCool = EP::EvaluateResponseLadder(
                warm, EP::Demeanor::Warm, EP::PresenceTier::Steady, imp, true, true, true, false);
            assert(dwCool.rung1.applyModeOverride && dwCool.rung2.applyExpression);
            assert(!dwCool.rung4.emitWhisper && !dwCool.rung4.emitModEvent &&
                   !dwCool.rung4.emitAiNpcBridge);

            // Challenge
            const EP::InterpretationResult chal{
                EP::Interpretation::Challenge, 0.80f, EP::DwellBand::Stare, true};
            const auto dc = EP::EvaluateResponseLadder(
                chal, EP::Demeanor::Stern, EP::PresenceTier::Noticed, imp, true, false, true, true);
            assert(dc.rung1.applyModeOverride && dc.rung1.hcepMode == 0u &&
                   dc.rung1.lockMutualReciprocity);
            assert(dc.rung2.applyExpression && dc.rung2.mood == EP::FaceExpressionMood::Anger);
            assert(!dc.rung4.emitWhisper && dc.rung4.emitModEvent && dc.rung4.emitAiNpcBridge);

            // IntrusionLeer
            const EP::InterpretationResult leer{
                EP::Interpretation::IntrusionLeer, 0.75f, EP::DwellBand::Stare, true};
            const auto dl = EP::EvaluateResponseLadder(
                leer, EP::Demeanor::Courteous, EP::PresenceTier::Unnoticed, imp, true, true, false,
                true);
            assert(dl.rung1.forceAversion && dl.rung1.hcepMode == 4u &&
                   dl.rung1.aversionRegionId == EP::Region::LRPeriph);
            assert(dl.rung2.mood == EP::FaceExpressionMood::Disgusted);
            assert(dl.rung4.emitWhisper && dl.rung4.emitModEvent && !dl.rung4.emitAiNpcBridge);

            // Suspicion, Compassion, Deference
            const EP::InterpretationResult susp{
                EP::Interpretation::Suspicion, 0.50f, EP::DwellBand::Gaze, true};
            assert(EP::EvaluateResponseLadder(susp, EP::Demeanor::Evasive, EP::PresenceTier::Noticed,
                                              imp, true, true, true, true)
                       .rung2.mood == EP::FaceExpressionMood::Puzzled);

            const EP::InterpretationResult comp{
                EP::Interpretation::Compassion, 0.60f, EP::DwellBand::Look, true};
            const auto dComp = EP::EvaluateResponseLadder(
                comp, EP::Demeanor::Warm, EP::PresenceTier::Noticed, imp, true, true, true, true);
            assert(dComp.rung1.hcepMode == 3u && dComp.rung2.mood == EP::FaceExpressionMood::Sad);
            assert(!dComp.rung4.emitModEvent); // Look < Gaze

            const EP::InterpretationResult def{
                EP::Interpretation::Deference, 0.50f, EP::DwellBand::Look, true};
            const auto dDef = EP::EvaluateResponseLadder(
                def, EP::Demeanor::Deferential, EP::PresenceTier::Noticed, imp, true, true, true,
                true);
            assert(dDef.rung1.forceAversion && dDef.rung2.mood == EP::FaceExpressionMood::Neutral);

            // Social module disabled blocks Rungs 1, 2, 4
            const auto dOff = EP::EvaluateResponseLadder(
                warm, EP::Demeanor::Warm, EP::PresenceTier::Steady, imp, false, true, true, true);
            assert(!dOff.rung1.applyModeOverride && !dOff.rung2.applyExpression &&
                   !dOff.rung4.emitWhisper && !dOff.rung4.emitModEvent);
        }

        // 3. NpcCooldownTracker (20.0s per-NPC isolation + reset)
        {
            EP::NpcCooldownTracker tracker;
            assert(tracker.IsReady(0x101u, 1.0f));
            assert(tracker.TryConsume(0x101u, 1.0f));
            assert(!tracker.IsReady(0x101u, 10.0f));
            assert(!tracker.TryConsume(0x101u, 20.9f));
            assert(tracker.IsReady(0x102u, 2.0f)); // Independent NPC
            assert(tracker.TryConsume(0x101u, 21.0f));
            tracker.Reset();
            assert(tracker.IsReady(0x101u, 22.0f));
        }

        // 4. Whisper & AI-NPC Bridge formatting
        {
            const auto w = EP::FormatWhisperText("Orgnar", EP::Interpretation::Respect);
            assert(w.find("Orgnar") != std::string::npos);
            const auto b = EP::FormatAiNpcBridgeContext(
                "Camilla Valerius", EP::Region::LeftEye, EP::Interpretation::Warmth,
                EP::DwellBand::Gaze, 2.6f, EP::Demeanor::Warm);
            assert(b.find("[TrueGaze]") != std::string::npos);
            assert(b.find("Camilla Valerius") != std::string::npos);
            assert(b.find("Warmth") != std::string::npos);
            assert(b.find("LeftEye") != std::string::npos);
        }

        // 5. ShouldInitiateEyeContactGreeting gate
        {
            EP::EyeContactGreetingInput base{};
            base.enabled = true;
            base.featureToggle = true;
            base.presenceTier = EP::PresenceTier::Noticed;
            base.inTutorialInn = false;
            base.isMutual = true;
            base.interpretation = EP::Interpretation::Respect;
            base.dwellSec = 2.2f;
            base.distanceMeters = 3.0f;
            base.cooldownReady = true;
            assert(EP::ShouldInitiateEyeContactGreeting(base));

            auto tutorial = base;
            tutorial.presenceTier = EP::PresenceTier::Unnoticed;
            assert(!EP::ShouldInitiateEyeContactGreeting(tutorial));
            tutorial.inTutorialInn = true;
            assert(EP::ShouldInitiateEyeContactGreeting(tutorial));

            auto tooFarDist = base;
            tooFarDist.distanceMeters = 5.0f;
            assert(!EP::ShouldInitiateEyeContactGreeting(tooFarDist));

            auto brief = base;
            brief.dwellSec = 1.8f;
            assert(!EP::ShouldInitiateEyeContactGreeting(brief));

            auto child = base;
            child.targetIsChild = true;
            assert(!EP::ShouldInitiateEyeContactGreeting(child));

            auto combat = base;
            combat.inCombat = true;
            assert(!EP::ShouldInitiateEyeContactGreeting(combat));

            auto scene = base;
            scene.inScene = true;
            assert(!EP::ShouldInitiateEyeContactGreeting(scene));

            auto armed = base;
            armed.playerWeaponDrawn = true;
            assert(!EP::ShouldInitiateEyeContactGreeting(armed));
        }

        // 6. StaringContestController: rounds 1->2->3 win, look-away grace & loss, blink loss
        {
            EP::StaringContestController winContest;
            winContest.Start(0x4001u);
            assert(winContest.IsActive() && winContest.CurrentRound() == 1u);

            // Brief look-away within grace (< 0.45s) recovers when eyes return
            winContest.Tick(0.20f, false, false, 20.0f);
            assert(winContest.IsActive());
            winContest.Tick(0.10f, true, false, 20.0f);
            assert(winContest.IsActive());

            for (int i = 0; i < 60; ++i)
            {
                winContest.Tick(0.10f, true, false, 20.0f);
            }
            assert(winContest.CurrentRound() == 2u);
            for (int i = 0; i < 50; ++i)
            {
                winContest.Tick(0.10f, true, false, 20.0f);
            }
            assert(winContest.CurrentRound() == 3u && winContest.BlinkPressure01() > 0.3f);
            for (int i = 0; i < 45; ++i)
            {
                winContest.Tick(0.10f, true, false, 20.0f);
            }
            assert(winContest.Status() == EP::ContestStatus::Won && !winContest.IsActive());

            EP::StaringContestController awayContest;
            awayContest.Start(0x4002u);
            awayContest.Tick(0.25f, false, false, 20.0f);
            awayContest.Tick(0.25f, false, false, 20.0f);
            assert(awayContest.Status() == EP::ContestStatus::LostLookedAway);

            EP::StaringContestController blinkContest;
            blinkContest.Start(0x4003u);
            blinkContest.Tick(0.50f, true, false, 20.0f);
            assert(blinkContest.BlinkPressure01() > 0.0f);
            blinkContest.Tick(0.10f, true, true, 20.0f);
            assert(blinkContest.Status() == EP::ContestStatus::LostBlinked);
        }
    }

    void TestOblivionRealtimePersuasionAndTransactions()
    {
        // 1. Riverwood cast profiles & universal deterministic fallback
        {
            const auto camilla = EP::ResolveNpcAffinityProfile(
                0x01347Bu, EP::ObserverCulture::Imperial, EP::RelationshipTier::Stranger, false,
                false);
            assert(camilla.ForVector(EP::PersuasionVector::Admire) == EP::AffinityTier::Love);
            assert(camilla.ForVector(EP::PersuasionVector::Coerce) == EP::AffinityTier::Hate);

            const auto delphine = EP::ResolveNpcAffinityProfile(
                0x013485u, EP::ObserverCulture::Breton, EP::RelationshipTier::Stranger, false,
                false);
            assert(delphine.ForVector(EP::PersuasionVector::Coerce) == EP::AffinityTier::Love);
            assert(delphine.ForVector(EP::PersuasionVector::Joke) == EP::AffinityTier::Hate);

            const auto lucan = EP::ResolveNpcAffinityProfile(
                0x01347Cu, EP::ObserverCulture::Imperial, EP::RelationshipTier::Stranger, false,
                false);
            assert(lucan.ForVector(EP::PersuasionVector::Boast) == EP::AffinityTier::Love);
            assert(lucan.ForVector(EP::PersuasionVector::SlyBribe) == EP::AffinityTier::Like);

            const auto guardA = EP::ResolveNpcAffinityProfile(
                0x0ABCDEFu, EP::ObserverCulture::Nord, EP::RelationshipTier::Superior, true,
                false);
            const auto guardB = EP::ResolveNpcAffinityProfile(
                0x0ABCDEFu, EP::ObserverCulture::Nord, EP::RelationshipTier::Superior, true,
                false);
            assert(guardA.ForVector(EP::PersuasionVector::Coerce) == EP::AffinityTier::Hate);
            assert(guardA.tiers == guardB.tiers);
        }

        // 2. Hard Child Safety Gate on Coerce & SlyBribe
        {
            const auto childProf = EP::ResolveNpcAffinityProfile(
                0x01347Cu, EP::ObserverCulture::Nord, EP::RelationshipTier::Stranger, false, true);
            assert(childProf.ForVector(EP::PersuasionVector::Coerce) == EP::AffinityTier::Hate);
            assert(childProf.ForVector(EP::PersuasionVector::SlyBribe) == EP::AffinityTier::Hate);

            EP::TransactionInput childCoerce{};
            childCoerce.vector = EP::PersuasionVector::Coerce;
            childCoerce.isChild = true;
            childCoerce.dwellSec = 3.0f;
            const auto out = EP::EvaluateTransactionCommit(childCoerce);
            assert(!out.committed && out.blockedByChildGate && out.speechBarterDelta == 0.0f);
        }

        // 3. EyeHandEyeTracker cadences (SlyBribe & StandDownGrace)
        {
            EP::EyeHandEyeTracker tracker;
            tracker.Observe(0x5001u, EP::Region::RightHand, false, 10.0f);
            tracker.Observe(0x5001u, EP::Region::LeftEye, false, 11.2f);
            assert(tracker.IsSlyBribeCadenceActive(0x5001u, 11.5f));
            assert(EP::ClassifyPersuasionVector(
                       EP::Region::LeftEye, EP::Demeanor::Courteous, false, false, false,
                       EP::PresenceTier::Noticed, false, false, true, false) ==
                   EP::PersuasionVector::SlyBribe);

            tracker.Reset();
            tracker.Observe(0x5002u, EP::Region::LeftEye, true, 20.0f);
            tracker.Observe(0x5002u, EP::Region::LeftEye, false, 21.4f);
            assert(tracker.IsStandDownGraceActive(0x5002u, 22.0f));

            EP::TransactionInput graceIn{};
            graceIn.vector = EP::PersuasionVector::Admire;
            graceIn.affinity = EP::AffinityTier::Like;
            graceIn.dwellSec = 2.2f;
            graceIn.standDownGrace = true;
            const auto graceOut = EP::EvaluateTransactionCommit(graceIn);
            assert(graceOut.committed && graceOut.type == EP::TransactionType::StandDownGrace);
            assert(graceOut.speechBarterDelta >= 5.0f && graceOut.ledgerDelta.fear < 0.0f);
        }

        // 4. Phase 1 Probe Tell & "Guard's Glance" witness interrupt
        {
            const auto camilla = EP::ResolveNpcAffinityProfile(
                0x01347Bu, EP::ObserverCulture::Imperial, EP::RelationshipTier::Stranger, false,
                false);
            const auto loveProbe =
                EP::EvaluateProbeTell(EP::PersuasionVector::Admire, camilla, 0.80f, 0u, false);
            assert(loveProbe.active && loveProbe.affinity == EP::AffinityTier::Love);
            assert(loveProbe.hcepMode == 3u && loveProbe.lockMutualReciprocity &&
                   loveProbe.morphMood == EP::FaceExpressionMood::Happy);

            const auto hateProbe =
                EP::EvaluateProbeTell(EP::PersuasionVector::Coerce, camilla, 0.80f, 0u, false);
            assert(hateProbe.active && hateProbe.affinity == EP::AffinityTier::Hate);
            assert(hateProbe.forceAversion && hateProbe.morphMood == EP::FaceExpressionMood::Anger);

            const auto guardGlance =
                EP::EvaluateProbeTell(EP::PersuasionVector::SlyBribe, camilla, 1.00f, 1u, false);
            assert(guardGlance.active && guardGlance.guardGlanceInterrupt &&
                   guardGlance.forceAversion &&
                   guardGlance.morphMood == EP::FaceExpressionMood::Fear);
            const auto whisper = EP::FormatProbeWhisperText("Orgnar", guardGlance);
            assert(whisper.find("guard") != std::string::npos);

            assert(!EP::EvaluateProbeTell(EP::PersuasionVector::Admire, camilla, 2.20f, 0u, false)
                        .active);
        }

        // 5. Oblivion GMST cost & yield curves + law witness block + Master half cost
        {
            EP::TransactionInput base{};
            base.vector = EP::PersuasionVector::SlyBribe;
            base.affinity = EP::AffinityTier::Like;
            base.dwellSec = 2.4f;
            base.playerSpeechcraft = 25.0f;
            base.npcSpeechcraft = 45.0f;
            base.playerLevel = 5;
            base.npcLevel = 15;
            base.presenceMax = 25.0f;

            const auto clearTx = EP::EvaluateTransactionCommit(base);
            assert(clearTx.committed && clearTx.type == EP::TransactionType::SlyBribePrime);
            assert(clearTx.computedBribeCostGold > 60 && clearTx.speechBarterDelta > 0.0f);

            auto master = base;
            master.scholarshipRank = EP::ScholarshipRank::Canticle;
            const auto masterTx = EP::EvaluateTransactionCommit(master);
            assert(masterTx.computedBribeCostGold < clearTx.computedBribeCostGold);

            auto witnessed = base;
            witnessed.lawWitnessCount = 1u;
            const auto blockedTx = EP::EvaluateTransactionCommit(witnessed);
            assert(!blockedTx.committed && blockedTx.blockedByWitness &&
                   blockedTx.ledgerDelta.suspicion > 0.0f);

            // Diminishing yield curve (fBribeCurve=0.75): Tick 4 (9.5s) > Tick 2 (2.4s)
            EP::TransactionInput shortJoke{};
            shortJoke.vector = EP::PersuasionVector::Joke;
            shortJoke.affinity = EP::AffinityTier::Love;
            shortJoke.dwellSec = 2.4f;
            auto longJoke = shortJoke;
            longJoke.dwellSec = 9.5f;
            const auto sj = EP::EvaluateTransactionCommit(shortJoke);
            const auto lj = EP::EvaluateTransactionCommit(longJoke);
            assert(lj.ledgerDelta.warmth > sj.ledgerDelta.warmth);
            assert(lj.ledgerDelta.warmth < sj.ledgerDelta.warmth * 2.25f); // Sub-linear 0.75 curve
        }

        // 6. Saturation decay (45s novelty) + ModifierLedger stacking & cleanup
        {
            EP::TransactionSaturationTracker sat;
            assert(std::abs(sat.NoveltyMultiplier(0x7001u, EP::PersuasionVector::Joke, 10.0f) -
                            1.0f) < 1e-5f);
            sat.RecordCommit(0x7001u, EP::PersuasionVector::Joke, 10.0f);
            assert(std::abs(sat.NoveltyMultiplier(0x7001u, EP::PersuasionVector::Joke, 20.0f) -
                            0.50f) < 1e-5f);
            sat.RecordCommit(0x7001u, EP::PersuasionVector::Joke, 20.0f);
            assert(std::abs(sat.NoveltyMultiplier(0x7001u, EP::PersuasionVector::Joke, 25.0f) -
                            0.20f) < 1e-5f);
            assert(std::abs(sat.NoveltyMultiplier(0x7001u, EP::PersuasionVector::Joke, 70.0f) -
                            1.0f) < 1e-5f);

            EP::ModifierLedger ledger;
            const float scholar =
                EP::ComputeScholarshipBonus(EP::ScholarshipRank::Scholar).speechIllusionBonus;
            const float d1 = ledger.SetTargetDelta(0x14u, 107, scholar + 6.0f);
            assert(std::abs(d1 - (scholar + 6.0f)) < 1e-5f);
            const auto teardown = ledger.CollectTeardownDeltas();
            assert(teardown.size() == 1u &&
                   std::abs(teardown[0].delta + scholar + 6.0f) < 1e-5f);
        }

        // 7. Multi-channel ApplyImpressionDelta (StandDownGrace & SlyBribePrime)
        {
            EP::GazeLedger gl;
            EP::Impressions standDown{};
            standDown.respect = 2.5f;
            standDown.warmth  = 1.8f;
            standDown.fear    = -2.0f;
            gl.ApplyImpressionDelta(0x8801u, standDown, 5.0f, 0x777u, true);
            const auto* entry = gl.Find(0x8801u);
            assert(entry != nullptr && entry->recognized);
            assert(std::abs(entry->impressions.respect - 2.5f) < 1e-4f);
            assert(std::abs(entry->impressions.warmth - 1.8f) < 1e-4f);
            assert(std::abs(entry->impressions.fear + 2.0f) < 1e-4f);
        }

        // 8. BookSpreadClock delayed-animation page-turn reconciliation (2-spread book)
        {
            EP::BookStudySession session;
            session.Begin(0x9901u,
                          "First spread text with enough words to read slowly [pagebreak] "
                          "Second spread text read calmly after pressing D",
                          false, 0u, false);
            assert(session.SpreadCount() == 2u);

            EP::BookSpreadClock clock;
            clock.Reset(static_cast<uint32_t>(session.SpreadCount()), 0.0f);
            // User reads spread 0 for 4.0s, presses D (Scaleform still reports spread 0)
            clock.ObserveScaleformSpread(0u, 4.0f, false, session);
            clock.NotifyPageTurnInput(+1, 4.0f, true, session);
            // User reads spread 1 for 4.5s without touching mouse, then closes book at t=8.5s
            clock.FlushOnClose(8.5f, false, session);

            EP::BookStudyEntry bookEntry{};
            uint32_t totalStudied = 0u;
            const auto summary = session.Finalize(bookEntry, totalStudied, 10.0f);
            assert(summary.newlyCreditedSpreads == 2u);
            assert(summary.fullStudyJustCompleted);
            assert(totalStudied == 1u);
            assert(bookEntry.pagesReadMask == 0b11ULL);

            // Speed-flip guard still catches rapid keyboard flips (< 1.5s per spread)
            EP::BookStudySession fastSession;
            fastSession.Begin(0x9902u, "Page one [pagebreak] Page two", false, 0u, false);
            EP::BookSpreadClock fastClock;
            fastClock.Reset(2u, 0.0f);
            fastClock.NotifyPageTurnInput(+1, 0.4f, false, fastSession);
            fastClock.FlushOnClose(0.9f, false, fastSession);
            EP::BookStudyEntry fastEntry{};
            const auto fastSummary = fastSession.Finalize(fastEntry, totalStudied, 10.0f);
            assert(fastSummary.newlyCreditedSpreads == 0u);
            assert(fastSummary.speedFlippedSpreads == 2u);
            assert(!fastSummary.fullStudyJustCompleted);
        }
    }

    void TestAwakeningRitualStagedProgressionAndCoSave()
    {
        // 1. UnlockMode parsing
        {
            bool valid = false;
            assert(EP::ParseUnlockMode("Ritual", &valid) == EP::UnlockMode::Ritual && valid);
            assert(EP::ParseUnlockMode("sandbox", &valid) == EP::UnlockMode::Sandbox && valid);
            assert(EP::ParseUnlockMode("invalid", &valid) == EP::UnlockMode::Ritual && !valid);
        }

        // 2. All 24 order-independent permutations of the 4 tumblers -> Stage 0->1->2->3->3 -> Ceremony->4
        {
            std::array<uint8_t, 4> bits{
                EP::TumblerBits::kWord, EP::TumblerBits::kVessel,
                EP::TumblerBits::kSharedSight, EP::TumblerBits::kPilgrimage};
            std::sort(bits.begin(), bits.end());
            int permutations = 0;
            do
            {
                EP::AwakeningState st{};
                assert(EP::ComputeAwakeningStage(st, EP::UnlockMode::Ritual) ==
                       EP::AwakeningStage::Dormant);
                assert(!EP::ResolveStageCapabilities(st, EP::UnlockMode::Ritual).expandedActive);

                // 1st tumbler -> Stage 1 (immediately enables Expanded Play!)
                const auto e1 = EP::MarkTumbler(st, bits[0], EP::UnlockMode::Ritual, "Key 1");
                assert(e1.progressed && e1.stageAdvanced &&
                       e1.newStage == EP::AwakeningStage::FirstGlimmer);
                const auto c1 = EP::ResolveStageCapabilities(st, EP::UnlockMode::Ritual);
                assert(c1.expandedActive && c1.allowReticle && c1.allowSocialMemory &&
                       !c1.allowPersuasionProbes && !c1.allowTransactions &&
                       !c1.allowPredatoryDemeanor);
                assert(std::abs(c1.maxPresenceCap - 39.0f) < 1e-4f);
                assert(EP::ClampDemeanorToStage(EP::Demeanor::Stern, c1) == EP::Demeanor::Courteous);

                // 2nd tumbler -> Stage 2
                const auto e2 = EP::MarkTumbler(st, bits[1], EP::UnlockMode::Ritual, "Key 2");
                assert(e2.progressed && e2.stageAdvanced &&
                       e2.newStage == EP::AwakeningStage::SteadySight);
                const auto c2 = EP::ResolveStageCapabilities(st, EP::UnlockMode::Ritual);
                assert(c2.allowPersuasionProbes && c2.allowEyeContactGreeting &&
                       c2.allowSternEvasiveDemeanor && !c2.allowTransactions &&
                       !c2.allowKindredSight);
                assert(EP::ClampDemeanorToStage(EP::Demeanor::Stern, c2) == EP::Demeanor::Stern);
                assert(EP::ClampDemeanorToStage(EP::Demeanor::Predatory, c2) == EP::Demeanor::Stern);

                // 3rd tumbler -> Stage 3
                const auto e3 = EP::MarkTumbler(st, bits[2], EP::UnlockMode::Ritual, "Key 3");
                assert(e3.progressed && e3.stageAdvanced &&
                       e3.newStage == EP::AwakeningStage::LivingGlyph);
                const auto c3 = EP::ResolveStageCapabilities(st, EP::UnlockMode::Ritual);
                assert(c3.allowTransactions && c3.allowStaringContest && c3.allowKindredSight &&
                       !c3.allowPredatoryDemeanor);

                // Cannot complete Ceremony with only 3 tumblers
                const auto premature =
                    EP::TryCompleteSanctuaryCeremony(st, true, true, true, EP::UnlockMode::Ritual);
                assert(!premature.progressed && !st.ceremonyCompleted);

                // 4th tumbler -> still Stage 3 until Sanctuary Ceremony completes
                const auto e4 = EP::MarkTumbler(st, bits[3], EP::UnlockMode::Ritual, "Key 4");
                assert(e4.progressed && !e4.stageAdvanced &&
                       e4.newStage == EP::AwakeningStage::LivingGlyph);

                // Ceremony requires Sanctuary + Focus + Studied Book
                assert(!EP::TryCompleteSanctuaryCeremony(st, false, true, true, EP::UnlockMode::Ritual)
                            .progressed);
                assert(!EP::TryCompleteSanctuaryCeremony(st, true, false, true, EP::UnlockMode::Ritual)
                            .progressed);
                assert(!EP::TryCompleteSanctuaryCeremony(st, true, true, false, EP::UnlockMode::Ritual)
                            .progressed);

                const auto cer =
                    EP::TryCompleteSanctuaryCeremony(st, true, true, true, EP::UnlockMode::Ritual);
                assert(cer.progressed && cer.stageAdvanced &&
                       cer.newStage == EP::AwakeningStage::CanticleAwakened);
                const auto c4 = EP::ResolveStageCapabilities(st, EP::UnlockMode::Ritual);
                assert(c4.allowPredatoryDemeanor && c4.allowFullSignatureResonance &&
                       std::abs(c4.maxPresenceCap - 100.0f) < 1e-4f &&
                       c4.maxScholarshipRank == EP::ScholarshipRank::Canticle);
                ++permutations;
            } while (std::next_permutation(bits.begin(), bits.end()));
            assert(permutations == 24);
        }

        // 3. Base-Skyrim Sanctuaries (7 zero-DLC sites) & 2-shrine Pilgrimage tumbler
        {
            assert(EP::MatchSanctuaryBit("Sleeping Giant Inn") == EP::SanctuaryBits::kSleepingGiantInn);
            assert(EP::MatchSanctuaryBit("Temple of Kynareth") == EP::SanctuaryBits::kTempleOfKynareth);
            assert(EP::MatchSanctuaryBit("Eldergleam Sanctuary") == EP::SanctuaryBits::kEldergleam);
            assert(EP::MatchSanctuaryBit("Temple of Mara") == EP::SanctuaryBits::kTempleOfMara);
            assert(EP::MatchSanctuaryBit("Temple of the Divines") ==
                   EP::SanctuaryBits::kTempleOfTheDivines);
            assert(EP::MatchSanctuaryBit("High Hrothgar") == EP::SanctuaryBits::kHighHrothgar);
            assert(EP::MatchSanctuaryBit("The Arcanaeum") == EP::SanctuaryBits::kArcanaeum);
            assert(EP::MatchSanctuaryBit("Whiterun") == EP::SanctuaryBits::kNone);

            EP::AwakeningState st{};
            const auto v1 = EP::RecordSanctuaryStillness(
                st, EP::SanctuaryBits::kSleepingGiantInn, EP::UnlockMode::Ritual);
            assert(v1.progressed && (st.tumblerMask & EP::TumblerBits::kPilgrimage) == 0u);
            // Revisiting same shrine does not light Pilgrimage
            const auto v1Dup = EP::RecordSanctuaryStillness(
                st, EP::SanctuaryBits::kSleepingGiantInn, EP::UnlockMode::Ritual);
            assert(!v1Dup.progressed);
            const auto v2 = EP::RecordSanctuaryStillness(
                st, EP::SanctuaryBits::kEldergleam, EP::UnlockMode::Ritual);
            assert(v2.progressed && (st.tumblerMask & EP::TumblerBits::kPilgrimage) != 0u);
        }

        // 4. Relic classification (ESL bespoke + vanilla Skyrim.esm fallback) & Sandbox override
        {
            assert(EP::ClassifyRelicByName("The First Canticle of the Eye", true, false, false) ==
                   EP::RelicKind::CanticleBook);
            assert(EP::ClassifyRelicByName("Effects of the Elder Scrolls", true, false, false) ==
                   EP::RelicKind::CanticleBook);
            assert(EP::ClassifyRelicByName("Amulet of the Ancestor Moth", false, true, false) ==
                   EP::RelicKind::PlayerToggleRelic);
            assert(EP::ClassifyRelicByName("Moth-Silk Browband", false, true, false) ==
                   EP::RelicKind::PlayerToggleRelic);
            assert(EP::ClassifyRelicByName("Ring of Quiet Sight", false, true, false) ==
                   EP::RelicKind::GiftableSightRelic);
            assert(EP::ClassifyRelicByName("Moth-Silk Ribbon", false, false, true) ==
                   EP::RelicKind::GiftableSightRelic);
            assert(EP::ClassifyRelicByName("Amulet of Julianos", false, true, false) ==
                   EP::RelicKind::VanillaFocus);
            assert(EP::ClassifyRelicByName("Iron Helmet", false, true, false) ==
                   EP::RelicKind::None);

            EP::AwakeningState empty{};
            const auto sb = EP::ResolveStageCapabilities(empty, EP::UnlockMode::Sandbox);
            assert(sb.expandedActive && sb.stage == EP::AwakeningStage::CanticleAwakened &&
                   sb.allowPredatoryDemeanor && sb.allowTransactions);
        }

        // 5. AWKN co-save round-trip, diegetic toggle & truncation safety
        {
            EP::AwakeningState orig{};
            orig.tumblerMask = EP::TumblerBits::kWord | EP::TumblerBits::kSharedSight;
            orig.visitedSanctuariesMask = EP::SanctuaryBits::kTempleOfKynareth;
            orig.ceremonyCompleted = false;
            orig.diegeticToggleOn = false;
            orig.followerRelicWitnessCount = 7u;

            const auto bytes = EP::EncodeAwakeningRecord(orig);
            EP::AwakeningState decoded{};
            assert(EP::DecodeAwakeningRecord(bytes, EP::kSerializationVersion, decoded));
            assert(decoded.tumblerMask == orig.tumblerMask);
            assert(decoded.visitedSanctuariesMask == orig.visitedSanctuariesMask);
            assert(decoded.ceremonyCompleted == orig.ceremonyCompleted);
            assert(decoded.diegeticToggleOn == orig.diegeticToggleOn);
            assert(decoded.followerRelicWitnessCount == 7u);
            assert(!EP::ResolveStageCapabilities(decoded, EP::UnlockMode::Ritual).expandedActive);

            // Truncated buffer rejected
            assert(!EP::DecodeAwakeningRecord(std::span<const uint8_t>(bytes.data(), 3u),
                                              EP::kSerializationVersion, decoded));
        }

        // 6. Parchment MessageBox ritual checklist formatting (incomplete vs ready)
        {
            EP::AwakeningState partial{};
            partial.tumblerMask = EP::TumblerBits::kWord;
            partial.visitedSanctuariesMask = EP::SanctuaryBits::kTempleOfKynareth;
            const std::string check1 = EP::FormatRitualStatusChecklist(
                partial, EP::UnlockMode::Ritual, true, "Temple of Kynareth", true, true);
            assert(check1.find("Stage 1: First Glimmer (1/4 Keys Lit)") != std::string::npos);
            assert(check1.find("[X] The Word") != std::string::npos);
            assert(check1.find("[ ] The Vessel") != std::string::npos);
            assert(check1.find("[ ] Shared Sight") != std::string::npos);
            assert(check1.find("[ ] The Pilgrimage (1/2)") != std::string::npos);
            assert(check1.find("[X] Inside Sanctuary (Temple of Kynareth)") != std::string::npos);
            assert(check1.find("cannot awaken in full") != std::string::npos);

            EP::AwakeningState readyForSanctuary{};
            readyForSanctuary.tumblerMask = EP::TumblerBits::kAllFour;
            readyForSanctuary.visitedSanctuariesMask = static_cast<uint8_t>(
                EP::SanctuaryBits::kSleepingGiantInn | EP::SanctuaryBits::kTempleOfKynareth);
            const std::string check2 = EP::FormatRitualStatusChecklist(
                readyForSanctuary, EP::UnlockMode::Ritual, false, "", true, true);
            assert(check2.find("All four keys are lit!") != std::string::npos);
            assert(check2.find("[ ] Stand inside a Skyrim Sanctuary") != std::string::npos);
        }
    }

    void TestSmartBrandishingCombatAndCreatureSight()
    {
        // 1. Smart Brandishing: peaceful utility (Canticle spell / torch / healing) != lethal brandish
        {
            EP::ActorHeldPosture canticle{};
            canticle.handsRaised = true;
            canticle.rightHand = EP::HeldItemThreat::UtilityOrPeaceful;
            canticle.leftHand = EP::HeldItemThreat::UtilityOrPeaceful;
            assert(!canticle.IsLethalBrandish());
            assert(canticle.IsPeacefulOrUtility());

            EP::ActorHeldPosture sheathedSword{};
            sheathedSword.handsRaised = false;
            sheathedSword.rightHand = EP::HeldItemThreat::MeleeWeapon;
            assert(!sheathedSword.IsLethalBrandish());

            EP::ActorHeldPosture drawnSword{};
            drawnSword.handsRaised = true;
            drawnSword.rightHand = EP::HeldItemThreat::MeleeWeapon;
            drawnSword.leftHand = EP::HeldItemThreat::ShieldOnly;
            assert(drawnSword.IsLethalBrandish());
            assert(drawnSword.HasShield());

            EP::ActorHeldPosture fireball{};
            fireball.handsRaised = true;
            fireball.rightHand = EP::HeldItemThreat::HostileSpell;
            assert(fireball.IsLethalBrandish() && fireball.HasHostileSpell());
        }

        // 2. Combat Sight: Stage 1 gated off; Stage 2+ reads Weapon Wind-Up, Spell Charge, Shield, Footwork
        {
            EP::CombatSightInput base{};
            base.stage = EP::AwakeningStage::FirstGlimmer;
            base.targetInCombat = true;
            base.targetHostile = true;
            base.regionId = EP::Region::RightHand;
            base.dwellSec = 0.5f;
            base.distanceMeters = 3.0f;
            base.targetAttacking = true;
            assert(!EP::EvaluateCombatSight(base, "Bandit").triggered);

            // Stage 2 Weapon Wind-Up -> Block & Armor brace
            base.stage = EP::AwakeningStage::SteadySight;
            const auto windup = EP::EvaluateCombatSight(base, "Bandit");
            assert(windup.triggered && windup.kind == EP::CombatSightKind::WeaponWindup);
            assert(windup.buffs.blockBonus >= 8.0f && windup.buffs.armorBonus >= 15.0f);

            // Stage 3 Spell Charge -> Magic Resist ward
            auto mage = base;
            mage.stage = EP::AwakeningStage::LivingGlyph;
            mage.targetAttacking = false;
            mage.targetCasting = true;
            const auto spellRead = EP::EvaluateCombatSight(mage, "Necromancer");
            assert(spellRead.triggered && spellRead.kind == EP::CombatSightKind::SpellCharge);
            assert(spellRead.buffs.resistMagicBonus >= 12.0f);

            // Shield Rhythm on LeftHand -> Offense opening
            auto blocker = base;
            blocker.stage = EP::AwakeningStage::CanticleAwakened;
            blocker.targetAttacking = false;
            blocker.targetBlocking = true;
            blocker.regionId = EP::Region::LeftHand;
            const auto shieldRead = EP::EvaluateCombatSight(blocker, "Draugr");
            assert(shieldRead.triggered && shieldRead.kind == EP::CombatSightKind::ShieldOpening);
            assert(shieldRead.buffs.offenseBonus >= 10.0f);

            // Footwork lunge on Ground -> Armor + Stamina regen
            auto lunger = base;
            lunger.targetAttacking = false;
            lunger.regionId = EP::Region::Ground;
            lunger.targetSpeedUnitsPerSec = 240.0f;
            const auto footRead = EP::EvaluateCombatSight(lunger, "Bandit Marauder");
            assert(footRead.triggered && footRead.kind == EP::CombatSightKind::FootworkLunge);
            assert(footRead.buffs.armorBonus >= 20.0f && footRead.buffs.staminaRegenBonus >= 10.0f);

            // Pre-combat NPC brandishing steel
            EP::CombatSightInput pre{};
            pre.stage = EP::AwakeningStage::SteadySight;
            pre.targetInCombat = false;
            pre.dwellSec = 0.6f;
            pre.distanceMeters = 4.0f;
            pre.targetPosture.handsRaised = true;
            pre.targetPosture.rightHand = EP::HeldItemThreat::MeleeWeapon;
            const auto brandish = EP::EvaluateCombatSight(pre, "Highwayman");
            assert(brandish.triggered && brandish.kind == EP::CombatSightKind::PreCombatBrandish);
        }

        // 3. Creature Sight: Prey Gentle Sight, Predator Kyne's Stillness, Dragon Dov's Weight
        {
            assert(EP::ClassifyCreatureArchetype("ElkRace", false) == EP::CreatureArchetype::Prey);
            assert(EP::ClassifyCreatureArchetype("WolfRace", false) == EP::CreatureArchetype::Predator);
            assert(EP::ClassifyCreatureArchetype("DragonRace", false) == EP::CreatureArchetype::Dragon);
            assert(EP::ClassifyCreatureArchetype("DragonPriestRace", false) != EP::CreatureArchetype::Dragon);
            assert(EP::ClassifyCreatureArchetype("NordRace", true) == EP::CreatureArchetype::Humanoid);

            // Prey Gentle Sight -> Sneak bonus when unarmed & Deferential
            EP::CreatureSightInput prey{};
            prey.stage = EP::AwakeningStage::FirstGlimmer;
            prey.archetype = EP::CreatureArchetype::Prey;
            prey.playerLethalBrandish = false;
            prey.activeDemeanor = EP::Demeanor::Deferential;
            prey.dwellSec = 1.5f;
            prey.distanceMeters = 9.0f;
            const auto preyRes = EP::EvaluateCreatureSight(prey, "Elk");
            assert(preyRes.triggered && preyRes.buffs.sneakBonus >= 10.0f);

            // Predator Kyne's Stillness -> +0.15 Presence milestone + armor/stamina
            EP::CreatureSightInput wolf{};
            wolf.stage = EP::AwakeningStage::SteadySight;
            wolf.archetype = EP::CreatureArchetype::Predator;
            wolf.playerLethalBrandish = false;
            wolf.dwellSec = 1.6f;
            wolf.distanceMeters = 5.0f;
            const auto wolfRes = EP::EvaluateCreatureSight(wolf, "Wolf");
            assert(wolfRes.triggered && wolfRes.markMilestone && wolfRes.presenceAward > 0.10f);

            // Dragon Dov's Weight -> +0.50 Presence milestone + Magic Resist ward
            EP::CreatureSightInput dov{};
            dov.stage = EP::AwakeningStage::SteadySight;
            dov.archetype = EP::CreatureArchetype::Dragon;
            dov.dwellSec = 2.2f;
            dov.distanceMeters = 14.0f;
            const auto dovRes = EP::EvaluateCreatureSight(dov, "Mirmulnir");
            assert(dovRes.triggered && dovRes.markMilestone && dovRes.presenceAward >= 0.50f &&
                   dovRes.buffs.resistMagicBonus >= 15.0f);
        }

        // 4. Sanctuary Relic Discovery mapping & backward-compatible AWKN v1 co-save round-trip
        {
            assert(EP::GetSanctuaryRelicSpec(EP::SanctuaryBits::kSleepingGiantInn).slot ==
                   EP::SanctuaryRelicSlot::FirstCanticle);
            assert(EP::GetSanctuaryRelicSpec(EP::SanctuaryBits::kTempleOfKynareth).slot ==
                   EP::SanctuaryRelicSlot::AmuletMoth);
            assert(EP::GetSanctuaryRelicSpec(EP::SanctuaryBits::kEldergleam).slot ==
                   EP::SanctuaryRelicSlot::MothSilkBrowband);
            assert(EP::GetSanctuaryRelicSpec(EP::SanctuaryBits::kTempleOfMara).slot ==
                   EP::SanctuaryRelicSlot::RingQuietSight);
            assert(EP::GetSanctuaryRelicSpec(EP::SanctuaryBits::kTempleOfTheDivines).slot ==
                   EP::SanctuaryRelicSlot::SilverMedallion);
            assert(EP::GetSanctuaryRelicSpec(EP::SanctuaryBits::kHighHrothgar).slot ==
                   EP::SanctuaryRelicSlot::MothSilkRibbon);
            assert(EP::GetSanctuaryRelicSpec(EP::SanctuaryBits::kArcanaeum).slot ==
                   EP::SanctuaryRelicSlot::FirstCanticle);

            EP::AwakeningState aw{};
            assert(EP::MarkSanctuaryRelicDiscovered(aw, EP::SanctuaryBits::kTempleOfKynareth));
            assert(!EP::MarkSanctuaryRelicDiscovered(aw, EP::SanctuaryBits::kTempleOfKynareth));
            assert(aw.HasAnyProgress());

            const auto bytes9 = EP::EncodeAwakeningRecord(aw);
            assert(bytes9.size() == 9u);
            EP::AwakeningState decoded9{};
            assert(EP::DecodeAwakeningRecord(bytes9, EP::kSerializationVersion, decoded9));
            assert(decoded9.discoveredSanctuaryRelicsMask == EP::SanctuaryBits::kTempleOfKynareth);

            // Legacy 8-byte AWKN v1 record remains 100% readable
            std::vector<uint8_t> legacy8(bytes9.begin(), bytes9.begin() + 8);
            EP::AwakeningState decoded8{};
            assert(EP::DecodeAwakeningRecord(legacy8, EP::kSerializationVersion, decoded8));
            assert(decoded8.discoveredSanctuaryRelicsMask == EP::SanctuaryBits::kNone);
        }
    }
} // namespace

int main()
{
    TestDefaultsAndSafety();
    TestDemeanorParsingAndNames();
    TestDwellClassification();
    TestProgressionTiers();
    TestConfigSanitisation();
    TestCoSaveSignRoundTripAndSanitisation();
    TestCoSaveLedgerRoundTripAndFormIdResolution();
    TestCoSaveBookRoundTripAndTruncationSafety();
    TestModifierLedgerAndScholarshipBonuses();
    TestDemeanorControllerAutoPriorityAndHybridPin();
    TestGazeReticlePaletteArcAndMagnetism();
    TestPerceptionGateVisibilityAndOcclusion();
    TestInterpretationEngineMatrixAndChildSafety();
    TestGazeLedgerLruDecayAndWitnessHooks();
    TestPresenceSignatureDiminishingReturnsAndTitles();
    TestBookStudyTrackerDwellParsingAndRewards();
    TestResponseLadderAndFirstSliceFeatures();
    TestOblivionRealtimePersuasionAndTransactions();
    TestAwakeningRitualStagedProgressionAndCoSave();
    TestSmartBrandishingCombatAndCreatureSight();

    std::cout << "ExpandedPlayTests: all checks passed.\n";
    return 0;
}
