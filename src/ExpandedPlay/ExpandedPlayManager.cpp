#include "ExpandedPlay/ExpandedPlayManager.hpp"

#include "Engine/ConfigManager.hpp"
#include "Engine/GazeAnchors.hpp"
#include "Engine/GazeEngine.hpp"
#include "Engine/PlayerGazeResolver.hpp"
#include "Engine/TraceLogger.hpp"
#include "ExpandedPlay/CoSaveSerializer.hpp"
#include "ExpandedPlay/ExpandedPlayModifiers.hpp"
#include "ExpandedPlay/GazeReticle.hpp"
#include "ExpandedPlay/InterpretationEngine.hpp"
#include "ExpandedPlay/PerceptionGate.hpp"
#include "PCH.h"

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <format>
#include <mutex>

namespace TrueGaze::ExpandedPlay
{
    namespace
    {
        constexpr float kUnitsPerMeter = 70.0f;
        constexpr float kFocusPeriodSec = 0.10f;   // 10 Hz
        constexpr float kWitnessPeriodSec = 0.50f; // 2 Hz

        // Vanilla Skyrim base FormIDs (low 24 bits) for Riverwood's love-triangle hook.
        constexpr uint32_t kBaseCamillaValerius = 0x01347Bu;
        constexpr uint32_t kBaseSven = 0x01347Fu;
        constexpr uint32_t kBaseFaendal = 0x013480u;

        std::mutex g_diagMutex;

        [[nodiscard]] bool ContainsIgnoreCase(std::string_view haystack,
                                              std::string_view needle) noexcept
        {
            if (needle.empty() || haystack.size() < needle.size())
            {
                return false;
            }
            for (size_t i = 0; i + needle.size() <= haystack.size(); ++i)
            {
                bool match = true;
                for (size_t j = 0; j < needle.size(); ++j)
                {
                    if (Detail::AsciiLower(haystack[i + j]) != Detail::AsciiLower(needle[j]))
                    {
                        match = false;
                        break;
                    }
                }
                if (match)
                {
                    return true;
                }
            }
            return false;
        }

#if __has_include(<RE/Skyrim.h>)
        [[nodiscard]] float QueryGameDaysPassed() noexcept
        {
            if (auto* cal = RE::Calendar::GetSingleton())
            {
                const float days = cal->GetDaysPassed();
                if (std::isfinite(days) && days >= 0.0f)
                {
                    return days;
                }
            }
            return 0.0f;
        }

        [[nodiscard]] const char* SafeActorName(const RE::Actor* actor) noexcept
        {
            if (!actor)
            {
                return "Target";
            }
            const char* name = actor->GetName();
            return (name && name[0] != '\0') ? name : "Target";
        }

        [[nodiscard]] bool IsSleepingGiantInn(const RE::PlayerCharacter* player) noexcept
        {
            if (!player)
            {
                return false;
            }
            if (auto* loc = player->GetCurrentLocation())
            {
                if (const char* name = loc->GetName(); name && name[0] != '\0')
                {
                    return ContainsIgnoreCase(name, "Sleeping Giant");
                }
            }
            return false;
        }

        [[nodiscard]] bool IsInFactionId(const RE::Actor* actor, uint32_t formId) noexcept
        {
            if (!actor || formId == 0u)
            {
                return false;
            }
            auto* fact = RE::TESForm::LookupByID<RE::TESFaction>(static_cast<RE::FormID>(formId));
            return fact && actor->IsInFaction(fact);
        }

        [[nodiscard]] uint32_t InspectActorFactionBits(RE::Actor* actor,
                                                       bool isPlayer) noexcept
        {
            if (!actor)
            {
                return FactionBits::kNone;
            }
            uint32_t bits = FactionBits::kNone;
            if (IsInFactionId(actor, VanillaFactionIds::kDarkBrotherhood))
                bits |= FactionBits::kDarkBrotherhood;
            if (IsInFactionId(actor, VanillaFactionIds::kThievesGuild))
                bits |= FactionBits::kThievesGuild;
            if (IsInFactionId(actor, VanillaFactionIds::kCompanions))
                bits |= FactionBits::kCompanions;
            if (IsInFactionId(actor, VanillaFactionIds::kCompanionsCircle) ||
                IsInFactionId(actor, VanillaFactionIds::kPlayerWerewolf))
            {
                bits |= FactionBits::kCompanions | FactionBits::kCompanionsCircle;
            }
            if (IsInFactionId(actor, VanillaFactionIds::kCollegeOfWinterhold))
                bits |= FactionBits::kCollegeWinterhold;
            if (IsInFactionId(actor, VanillaFactionIds::kCWImperial) ||
                IsInFactionId(actor, VanillaFactionIds::kCWImperialNpc))
                bits |= FactionBits::kImperialLegion;
            if (IsInFactionId(actor, VanillaFactionIds::kCWSons))
                bits |= FactionBits::kStormcloaks;
            if (IsInFactionId(actor, VanillaFactionIds::kBardsCollege))
                bits |= FactionBits::kBardsCollege;
            if (isPlayer)
            {
                if (auto* ms05 = RE::TESForm::LookupByID<RE::TESQuest>(
                        static_cast<RE::FormID>(VanillaFactionIds::kBardsCollegeQuest));
                    ms05 && ms05->IsCompleted())
                {
                    bits |= FactionBits::kBardsCollege;
                }
            }
            if (IsInFactionId(actor, VanillaFactionIds::kVampirePC) ||
                IsInFactionId(actor, VanillaFactionIds::kVampireFaction))
            {
                bits |= FactionBits::kVampire;
            }
            else if (auto* vampKw = RE::TESForm::LookupByID<RE::BGSKeyword>(
                         static_cast<RE::FormID>(VanillaFactionIds::kVampireKeyword));
                     vampKw && actor->HasKeyword(vampKw))
            {
                bits |= FactionBits::kVampire;
            }
            if (IsInFactionId(actor, VanillaFactionIds::kVigilantOfStendarr))
                bits |= FactionBits::kVigilant;

            // Runtime-optional Dawnguard detection (zero hard DLC dependency)
            actor->VisitFactions(
                [&](RE::TESFaction* f, std::int8_t rank) -> bool
                {
                    if (f && rank >= 0)
                    {
                        const char* nm = f->GetName();
                        if (nm && ContainsIgnoreCase(nm, "Dawnguard"))
                        {
                            bits |= FactionBits::kDawnguard;
                            return true;
                        }
                    }
                    return false;
                });
            return bits;
        }

        void InspectWornFactionArmor(RE::Actor* actor, bool& dbGear, bool& tgGear,
                                     bool& collegeRobes, bool& impArmor, bool& scArmor,
                                     bool& hunterGear) noexcept
        {
            dbGear = tgGear = collegeRobes = impArmor = scArmor = hunterGear = false;
            if (!actor)
            {
                return;
            }
            using Slot = RE::BGSBipedObjectForm::BipedObjectSlot;
            constexpr Slot kSlots[] = {Slot::kBody, Slot::kHead, Slot::kHair};
            for (const auto slot : kSlots)
            {
                auto* worn = actor->GetWornArmor(slot, true);
                if (!worn)
                {
                    continue;
                }
                const char* nm = worn->GetName();
                if (!nm || nm[0] == '\0')
                {
                    continue;
                }
                if (ContainsIgnoreCase(nm, "Shrouded") ||
                    ContainsIgnoreCase(nm, "Dark Brotherhood"))
                    dbGear = true;
                if (ContainsIgnoreCase(nm, "Thieves Guild") ||
                    ContainsIgnoreCase(nm, "Nightingale") || ContainsIgnoreCase(nm, "Guild Master"))
                    tgGear = true;
                if (ContainsIgnoreCase(nm, "College") || ContainsIgnoreCase(nm, "Archmage") ||
                    ContainsIgnoreCase(nm, "Mage Robes") || ContainsIgnoreCase(nm, "Necromancer"))
                    collegeRobes = true;
                if (ContainsIgnoreCase(nm, "Imperial") || ContainsIgnoreCase(nm, "Penitus"))
                    impArmor = true;
                if (ContainsIgnoreCase(nm, "Stormcloak"))
                    scArmor = true;
                if (ContainsIgnoreCase(nm, "Dawnguard") || ContainsIgnoreCase(nm, "Vigilant"))
                    hunterGear = true;
            }
        }

        [[nodiscard]] bool HasStartedElderScrollOrMothQuest() noexcept
        {
            constexpr uint32_t kVanillaScrollQuests[] = {
                0x0002D516u, // MQ205 Elder Knowledge
                0x0002D512u  // DA04 Discerning the Transmundane
            };
            for (const uint32_t qid : kVanillaScrollQuests)
            {
                if (auto* q = RE::TESForm::LookupByID<RE::TESQuest>(static_cast<RE::FormID>(qid)))
                {
                    if (q->IsRunning() || q->IsCompleted() || q->GetCurrentStageID() > 0)
                    {
                        return true;
                    }
                }
            }
            // Optional Dawnguard Prophet quest (runtime EditorID lookup; zero hard DLC master)
            constexpr const char* kOptionalProphetIds[] = {"DLC1VQ03Hunter", "DLC1VQ03Vampire"};
            for (const char* edid : kOptionalProphetIds)
            {
                if (auto* q = RE::TESForm::LookupByEditorID<RE::TESQuest>(edid))
                {
                    if (q->IsRunning() || q->IsCompleted() || q->GetCurrentStageID() > 0)
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        [[nodiscard]] bool IsDaylightNow() noexcept
        {
            if (auto* cal = RE::Calendar::GetSingleton())
            {
                const float hr = cal->GetHour();
                return hr >= 6.0f && hr < 20.0f;
            }
            return true;
        }

        [[nodiscard]] bool IsCourtOrTempleLocation(const RE::TESForm* locForm) noexcept
        {
            const auto* loc = locForm ? locForm->As<RE::BGSLocation>() : nullptr;
            if (!loc)
            {
                return false;
            }
            const char* name = loc->GetName();
            if (!name || name[0] == '\0')
            {
                return false;
            }
            return ContainsIgnoreCase(name, "Palace") || ContainsIgnoreCase(name, "Dragonsreach") ||
                   ContainsIgnoreCase(name, "Keep") || ContainsIgnoreCase(name, "Longhouse") ||
                   ContainsIgnoreCase(name, "Temple") || ContainsIgnoreCase(name, "High Hrothgar");
        }

        [[nodiscard]] bool IsPeacefulTownLocation(const RE::BGSLocation* loc) noexcept
        {
            if (!loc)
            {
                return false;
            }
            const char* name = loc->GetName();
            if (!name || name[0] == '\0')
            {
                return false;
            }
            return ContainsIgnoreCase(name, "Riverwood") || ContainsIgnoreCase(name, "Whiterun") ||
                   ContainsIgnoreCase(name, "Solitude") || ContainsIgnoreCase(name, "Windhelm") ||
                   ContainsIgnoreCase(name, "Riften") || ContainsIgnoreCase(name, "Markarth") ||
                   ContainsIgnoreCase(name, "Falkreath") || ContainsIgnoreCase(name, "Morthal") ||
                   ContainsIgnoreCase(name, "Dawnstar") || ContainsIgnoreCase(name, "Inn");
        }

        [[nodiscard]] RE::TESFaction* PlayerMarriedFaction() noexcept
        {
            static RE::TESFaction* cached = nullptr;
            static bool resolved = false;
            if (!resolved)
            {
                cached = RE::TESForm::LookupByEditorID<RE::TESFaction>("PlayerMarriedFaction");
                if (!cached)
                {
                    cached = RE::TESForm::LookupByID<RE::TESFaction>(0x000C6472u);
                }
                resolved = true;
            }
            return cached;
        }

        [[nodiscard]] bool IsPlayerSpouse(RE::Actor* actor) noexcept
        {
            if (!actor)
            {
                return false;
            }
            if (auto* married = PlayerMarriedFaction())
            {
                return actor->IsInFaction(married);
            }
            return false;
        }

        [[nodiscard]] ObserverCulture MapObserverCulture(const RE::Actor* actor) noexcept
        {
            const auto* base = actor ? actor->GetActorBase() : nullptr;
            auto* npcBase = base ? const_cast<RE::TESNPC*>(base->As<RE::TESNPC>()) : nullptr;
            const auto* race = npcBase ? npcBase->GetRace() : nullptr;
            const char* edid = race ? race->GetFormEditorID() : nullptr;
            if (!edid)
            {
                return ObserverCulture::Generic;
            }
            const auto has = [edid](const char* prefix) noexcept
            {
                return std::strncmp(edid, prefix, std::strlen(prefix)) == 0;
            };
            if (has("NordRace") || has("NordVampire"))
                return ObserverCulture::Nord;
            if (has("ImperialRace") || has("ImperialVampire"))
                return ObserverCulture::Imperial;
            if (has("BretonRace") || has("BretonVampire"))
                return ObserverCulture::Breton;
            if (has("RedguardRace") || has("RedguardVampire"))
                return ObserverCulture::Redguard;
            if (has("HighElfRace") || has("HighElfVampire"))
                return ObserverCulture::Altmer;
            if (has("WoodElfRace") || has("WoodElfVampire"))
                return ObserverCulture::Bosmer;
            if (has("DarkElfRace") || has("DarkElfVampire"))
                return ObserverCulture::Dunmer;
            if (has("OrcRace") || has("OrcVampire"))
                return ObserverCulture::Orc;
            if (has("KhajiitRace") || has("KhajiitVampire"))
                return ObserverCulture::Khajiit;
            if (has("ArgonianRace") || has("ArgonianVampire"))
                return ObserverCulture::Argonian;
            return ObserverCulture::Generic;
        }

        [[nodiscard]] RelationshipTier
        ClassifyRelationshipTier(RE::Actor* observer, const GazeLedgerEntry* ledgerEntry) noexcept
        {
            if (!observer)
            {
                return RelationshipTier::Stranger;
            }
            if (IsPlayerSpouse(observer))
            {
                return RelationshipTier::Spouse;
            }
            if (observer->IsPlayerTeammate())
            {
                return RelationshipTier::Follower;
            }
            const int16_t rank = Engine::GazeEngine::RelationshipRankForActor(observer);
            if (rank >= 4)
                return RelationshipTier::Partner;
            if (rank >= 3)
                return RelationshipTier::Confidant;
            if (rank >= 1)
                return RelationshipTier::Friend;
            if (rank <= -3)
                return RelationshipTier::Enemy;
            if (rank <= -1)
                return RelationshipTier::Rival;
            if (observer->IsGuard())
            {
                return RelationshipTier::Superior;
            }
            if (ledgerEntry && ledgerEntry->recognized)
            {
                return RelationshipTier::Acquaintance;
            }
            return RelationshipTier::Stranger;
        }

        void InspectPlayerHeadwear(RE::PlayerCharacter* player, bool& outFullHelm, bool& outHooded,
                                   bool& outDarkBrotherhood) noexcept
        {
            outFullHelm = false;
            outHooded = false;
            outDarkBrotherhood = false;
            if (!player)
            {
                return;
            }
            if (auto* helm =
                    player->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kHead, true))
            {
                const char* name = helm->GetName();
                const std::string_view sv = name ? std::string_view(name) : std::string_view{};
                const bool coversHair =
                    helm->HasPartOf(RE::BGSBipedObjectForm::BipedObjectSlot::kHair);
                if (ContainsIgnoreCase(sv, "Hood") || ContainsIgnoreCase(sv, "Cowl"))
                {
                    outHooded = true;
                }
                else if (coversHair || ContainsIgnoreCase(sv, "Mask"))
                {
                    outFullHelm = true;
                }
                if (ContainsIgnoreCase(sv, "Shrouded"))
                {
                    outDarkBrotherhood = true;
                }
            }
            if (auto* body =
                    player->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kBody, true))
            {
                const char* name = body->GetName();
                if (name && ContainsIgnoreCase(name, "Shrouded"))
                {
                    outDarkBrotherhood = true;
                }
            }
        }

        [[nodiscard]] HeldItemThreat ClassifyHandObjectThreat(const RE::TESForm* obj) noexcept
        {
            if (!obj)
            {
                return HeldItemThreat::None;
            }
            const auto ft = obj->GetFormType();
            if (ft == RE::FormType::Light || ft == RE::FormType::Book)
            {
                return HeldItemThreat::UtilityOrPeaceful;
            }
            if (ft == RE::FormType::Armor)
            {
                if (const auto* armo = obj->As<RE::TESObjectARMO>(); armo && armo->IsShield())
                {
                    return HeldItemThreat::ShieldOnly;
                }
                return HeldItemThreat::UtilityOrPeaceful;
            }
            if (ft == RE::FormType::Weapon)
            {
                const auto* weap = obj->As<RE::TESObjectWEAP>();
                if (!weap)
                {
                    return HeldItemThreat::MeleeWeapon;
                }
                if (weap->IsHandToHandMelee())
                {
                    return HeldItemThreat::UnarmedFists;
                }
                if (weap->IsBow() || weap->IsCrossbow() || weap->IsStaff())
                {
                    return HeldItemThreat::RangedWeapon;
                }
                return HeldItemThreat::MeleeWeapon;
            }
            if (ft == RE::FormType::Spell)
            {
                const auto* spell = obj->As<RE::SpellItem>();
                if (!spell)
                {
                    return HeldItemThreat::UtilityOrPeaceful;
                }
                const char* name = spell->GetName();
                if (name && (ContainsIgnoreCase(name, "First Canticle") ||
                             ContainsIgnoreCase(name, "TrueGaze") ||
                             ContainsIgnoreCase(name, "Candlelight") ||
                             ContainsIgnoreCase(name, "Healing") ||
                             ContainsIgnoreCase(name, "Clairvoyance") ||
                             ContainsIgnoreCase(name, "Detect")))
                {
                    return HeldItemThreat::UtilityOrPeaceful;
                }
                if (spell->IsHostile())
                {
                    return HeldItemThreat::HostileSpell;
                }
                if (const auto school = spell->GetAssociatedSkill();
                    school == RE::ActorValue::kDestruction)
                {
                    return HeldItemThreat::HostileSpell;
                }
                return HeldItemThreat::UtilityOrPeaceful;
            }
            return HeldItemThreat::UtilityOrPeaceful;
        }

        [[nodiscard]] ActorHeldPosture InspectActorHeldPosture(const RE::Actor* actor) noexcept
        {
            ActorHeldPosture posture{};
            if (!actor)
            {
                return posture;
            }
            const auto* st = actor->AsActorState();
            posture.handsRaised = st ? st->IsWeaponDrawn() : false;
            posture.rightHand = ClassifyHandObjectThreat(actor->GetEquippedObject(false));
            posture.leftHand = ClassifyHandObjectThreat(actor->GetEquippedObject(true));
            if (posture.handsRaised && posture.rightHand == HeldItemThreat::None &&
                posture.leftHand == HeldItemThreat::None)
            {
                posture.rightHand = HeldItemThreat::UnarmedFists;
            }
            return posture;
        }

        [[nodiscard]] CreatureArchetype
        InspectActorCreatureArchetype(const RE::Actor* actor) noexcept
        {
            if (!actor)
            {
                return CreatureArchetype::Humanoid;
            }
            const auto* base = actor->GetActorBase();
            auto* npcBase = base ? const_cast<RE::TESNPC*>(base->As<RE::TESNPC>()) : nullptr;
            const auto* race = npcBase ? npcBase->GetRace() : nullptr;
            const char* edid = race ? race->GetFormEditorID() : nullptr;
            const char* rname = race ? race->GetName() : nullptr;
            const bool humanoid =
                (edid &&
                 (ContainsIgnoreCase(edid, "NordRace") ||
                  ContainsIgnoreCase(edid, "ImperialRace") ||
                  ContainsIgnoreCase(edid, "BretonRace") ||
                  ContainsIgnoreCase(edid, "RedguardRace") ||
                  ContainsIgnoreCase(edid, "HighElfRace") ||
                  ContainsIgnoreCase(edid, "WoodElfRace") ||
                  ContainsIgnoreCase(edid, "DarkElfRace") || ContainsIgnoreCase(edid, "OrcRace") ||
                  ContainsIgnoreCase(edid, "KhajiitRace") ||
                  ContainsIgnoreCase(edid, "ArgonianRace") ||
                  ContainsIgnoreCase(edid, "ElderRace") || ContainsIgnoreCase(edid, "Vampire"))) ||
                actor->IsChild();
            if (humanoid)
            {
                return CreatureArchetype::Humanoid;
            }
            if (edid && edid[0] != '\0')
            {
                const auto byEdid = ClassifyCreatureArchetype(edid, false);
                if (byEdid != CreatureArchetype::OtherCreature)
                {
                    return byEdid;
                }
            }
            return ClassifyCreatureArchetype(rname ? std::string_view(rname) : std::string_view{},
                                             false);
        }

        [[nodiscard]] uint8_t
        ClassifyPlayerCrosshairRegion(const RE::PlayerCharacter* player, RE::Actor* target,
                                      const Engine::PlayerGazeResolver::PlayerGaze& gaze,
                                      uint8_t prevRegionId) noexcept
        {
            if (!player || !target || !target->Get3D() || gaze.targetFormId == 0u)
            {
                return Region::LeftEye;
            }

            TrueGaze::Anchors::CameraBasis cam{};
            if (!TrueGaze::Anchors::ResolveCameraBasis(cam))
            {
                return gaze.onFace ? Region::LeftEye : Region::Torso;
            }

            const RE::NiPoint3& camPos = cam.pos;
            const RE::NiPoint3& fwd = cam.forward;
            const RE::NiPoint3& right = cam.right;

            const RE::NiPoint3 toFace{gaze.faceX - camPos.x, gaze.faceY - camPos.y,
                                      gaze.faceZ - camPos.z};
            const float distUnits = std::max(toFace.Length(), 10.0f);
            const float rayXAtTarget = camPos.x + fwd.x * distUnits;
            const float rayYAtTarget = camPos.y + fwd.y * distUnits;
            const float rayZAtTarget = camPos.z + fwd.z * distUnits;
            const float dzUnits = rayZAtTarget - gaze.faceZ;
            const float latUnits =
                (rayXAtTarget - gaze.faceX) * right.x + (rayYAtTarget - gaze.faceY) * right.y;

            if (dzUnits > 4.5f)
                return Region::Forehead;
            if (dzUnits >= -3.5f)
            {
                // Lateral deadband prevents LeftEye <-> RightEye strobing on nose bridge
                if (latUnits > 1.5f)
                    return Region::LeftEye;
                if (latUnits < -1.5f)
                    return Region::RightEye;
                return (prevRegionId == Region::RightEye) ? Region::RightEye : Region::LeftEye;
            }
            if (dzUnits >= -8.0f)
                return Region::Mouth;
            if (dzUnits >= -12.5f)
                return Region::Chin;
            if (dzUnits >= -45.0f)
            {
                // Upper chest / shoulders stay Torso unless aiming distinctly off to the side
                // or down into the waist/hands/belt band (prevents 3-5 m glances from tripping
                // Hands).
                if (dzUnits < -24.0f)
                {
                    if (latUnits < -10.0f)
                        return Region::RightHand;
                    if (latUnits > 10.0f)
                        return Region::LeftHand;
                    if (dzUnits < -28.0f)
                        return Region::RightHand; // Purse / belt waist band
                }
                else
                {
                    if (latUnits < -16.0f)
                        return Region::RightHand;
                    if (latUnits > 16.0f)
                        return Region::LeftHand;
                }
                return Region::Torso;
            }
            return Region::Ground;
        }
#endif
    } // namespace

    ExpandedPlayManager& ExpandedPlayManager::Get() noexcept
    {
        static ExpandedPlayManager instance;
        return instance;
    }

    void ExpandedPlayManager::ResetSession() noexcept
    {
        std::lock_guard lock(g_diagMutex);
        _demeanorCtrl.ClearPin();
        _ledger.Clear();
        _presenceSig = {};
        _awakening = {};
        _contest.Reset();
        _handEyeTracker.Reset();
        _txSaturation.Reset();
        _barkCooldown.Reset();
        _greetingCooldown.Reset();
        _witnessCooldown.Reset();
        _kindredCooldown.Reset();
        _combatSightCooldown.Reset();
        _creatureSightCooldown.Reset();
        _awardedCreatureMilestones.clear();
        _sessionElapsedSec = 0.0f;
        _focusAccumSec = 0.0f;
        _witnessAccumSec = 0.0f;
        _ledgerCommitAccumSec = 0.0f;
        _sanctuaryStillnessSec = 0.0f;
        _lastSanctuaryBit = 0u;
        _activeKindredAllyCount = 0u;
        _focusActorFormId = 0u;
        _focusRegionId = 0u;
        _focusDwellSec = 0.0f;
        _focusLossGraceSec = 0.0f;
        _lastLoggedDwellBand = DwellBand::None;
        _loggedDemeanorInit = false;
        _focusIsMutual = false;
        _focusRapidEscalation = false;
        _lastInterpretation = Interpretation::None;
        _lastPersuasionVector = PersuasionVector::None;
        _lastAffinityTier = AffinityTier::Like;
        _lastBribeCostGold = 0;
        _activeNeutralWitnessCount = 0u;
        _activeLawWitnessCount = 0u;
        _probeTellSent = false;
        _txCommittedForHold = false;
        _lastStrength = 0.0f;
        _frostVignetteAlpha = 0.0f;
        _magnetismX = 0.0f;
        _magnetismY = 0.0f;
        _thirdPersonPulseDwell = 0.0f;
        _rapportTargetFormId = 0u;
        _rapportExpiresSessionSec = 0.0f;
        _tacticalBuffExpiresSessionSec = 0.0f;
        _tacticalBuffActive = false;
        _stateDirty = false;
        _hadFocusedActor = false;
        _lastDiscoveryCheckSec = -100.0f;
        _lastNoteInventoryCheckSec = -100.0f;
        _visitedSleepingGiantThisSession = false;
        _innLeftGameDay = -1.0f;
        _pendingInnNoteHandoff = false;
        _pendingInnNoteSpeakerId = 0u;
        ExpandedPlayModifiers::Get().SetTransientTacticalBonuses({});
        ResponseLadder::ClearAllTransientExpressions();
    }

    void ExpandedPlayManager::OnGameLoaded() noexcept
    {
        const auto saved = CoSaveSerializer::Get().SnapshotState();
        bool innRumorDone = false;
        AwakeningState loadedAwakening{};
        {
            std::lock_guard lock(g_diagMutex);
            _presenceSig.Load(saved.sign);
            _ledger.LoadFromVector(saved.ledger);
            _awakening = saved.awakening;
            loadedAwakening = _awakening;
            innRumorDone = (_awakening.discoveryHintsMask & DiscoveryHintBits::kInnRumor) != 0u;
            _demeanorCtrl.Update(saved.sign.demeanorMode, saved.sign.activeDemeanor,
                                 _lastDemeanorCtx, 0.0f, nullptr);
        }
        AwakeningRitual::Get().EnsurePilgrimageQuestRunning();
        AwakeningRitual::Get().SyncPilgrimageQuestProgress(loadedAwakening, false);
        AwakeningRitual::Get().SetInnRumorInfoSaidOnce(innRumorDone);
    }

    void ExpandedPlayManager::OnBeforeSave() noexcept
    {
        SyncPersistentStateToSerializer();
    }

    void ExpandedPlayManager::ForceConsoleAwakeningUnlock() noexcept
    {
        {
            std::lock_guard lock(g_diagMutex);
            ForceUnlockAllForConsole(_awakening);
#if __has_include(<RE/Skyrim.h>)
            _presenceSig.TriggerCanticleAwakening(QueryGameDaysPassed());
#else
            _presenceSig.TriggerCanticleAwakening(0.0f);
#endif
            _stateDirty = true;
            SyncPersistentStateToSerializer();
        }
        const auto ep = Engine::ConfigManager::GetSingleton().SnapshotExpandedPlay();
        ExpandedPlayModifiers::Get().SyncFromPersistentState(ep.enabled);
        GazeReticle::Get().NotifyMilestone(
            "The First Canticle stands open - full TrueGaze awakened [Stage 4].", 0xFFD966u,
            "UILevelUpSD", true);
        Engine::TraceLogger::Get().LogExpandedPlayEvent(_sessionElapsedSec, "AWAKENING_UNLOCK", 0u,
                                                        "", 0u, 0.0f, "console:Stage4");
        logger::info(
            "[TrueGaze::ExpandedPlay] Console unlock applied: Stage 4 (Canticle Awakened).");
    }

    void ExpandedPlayManager::PinManualDemeanor(Demeanor chosen) noexcept
    {
        std::lock_guard lock(g_diagMutex);
        _demeanorCtrl.PinManualDemeanor(chosen, _lastDemeanorCtx);
    }

    bool ExpandedPlayManager::StartOrAbortStaringContest(uint32_t targetActorFormId) noexcept
    {
        std::lock_guard lock(g_diagMutex);
        if (_contest.IsActive())
        {
            const uint32_t prevOpp = _contest.OpponentFormId();
            _contest.Abort();
            _frostVignetteAlpha = 0.0f;
            GazeReticle::Get().PushWhisper("Staring contest forfeited.", 0xA6B3CCu);
            Engine::TraceLogger::Get().LogExpandedPlayEvent(
                _sessionElapsedSec, "CONTEST_END", prevOpp, "", Region::LeftEye, 0.0f, "aborted");
            logger::info("[TrueGaze::ExpandedPlay] Staring contest ABORTED (target={:08X}).",
                         prevOpp);
            return false;
        }
        if (targetActorFormId == 0u)
        {
            return false;
        }
        {
            const auto ep = Engine::ConfigManager::GetSingleton().SnapshotExpandedPlay();
            const auto caps = ResolveStageCapabilities(_awakening, ep.unlockMode);
            if (!caps.allowStaringContest)
            {
                GazeReticle::Get().PushWhisper("Staring contests require Stage 3: Living Glyph (3 "
                                               "keys of the First Canticle).",
                                               0xA6B3CCu);
                return false;
            }
        }
        _contest.Start(targetActorFormId);
        _frostVignetteAlpha = 0.0f;
        GazeReticle::Get().PushWhisper("Staring contest begun! Hold their eyes.", 0xFFD966u);
        Engine::TraceLogger::Get().LogExpandedPlayEvent(_sessionElapsedSec, "CONTEST_START",
                                                        targetActorFormId, "", Region::LeftEye,
                                                        0.0f, "round=1");
        logger::info("[TrueGaze::ExpandedPlay] Staring contest STARTED (target={:08X}).",
                     targetActorFormId);
        return true;
    }

    void ExpandedPlayManager::Tick(float deltaSeconds) noexcept
    {
        const auto ep = Engine::ConfigManager::GetSingleton().SnapshotExpandedPlay();
        if (!ep.enabled)
        {
            GazeReticle::Get().UpdateFrame(ReticleHudSnapshot{});
            return;
        }
        if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f || deltaSeconds >= 0.5f)
        {
            return;
        }

#if __has_include(<RE/Skyrim.h>)
        auto* ui = RE::UI::GetSingleton();
        if (!ui || ui->GameIsPaused() || ui->IsMenuOpen(RE::MainMenu::MENU_NAME))
        {
            GazeReticle::Get().UpdateFrame(ReticleHudSnapshot{});
            return;
        }

        const float gameDays = QueryGameDaysPassed();
        {
            std::lock_guard lock(g_diagMutex);
            _sessionElapsedSec += deltaSeconds;
            _focusAccumSec += deltaSeconds;
            _witnessAccumSec += deltaSeconds;
        }

        bool runFocus = false;
        bool runWitness = false;
        {
            std::lock_guard lock(g_diagMutex);
            if (_focusAccumSec >= kFocusPeriodSec)
            {
                _focusAccumSec = std::fmod(_focusAccumSec, kFocusPeriodSec);
                runFocus = true;
            }
            if (_witnessAccumSec >= kWitnessPeriodSec)
            {
                _witnessAccumSec = std::fmod(_witnessAccumSec, kWitnessPeriodSec);
                runWitness = true;
            }
        }

        if (runFocus)
        {
            EvaluateFocus10Hz(gameDays);
        }
        if (runWitness)
        {
            EvaluateWitnesses2Hz(gameDays);
        }

        auto* camera = RE::PlayerCamera::GetSingleton();
        const bool isThirdPerson = camera && camera->IsInThirdPerson();

        // Maintain the open First Canticle book visual in the player's left hand (1st & 3rd person)
        // whenever Sight of the First Canticle is equipped in the right hand with hands raised.
        AwakeningRitual::Get().UpdateLeftHandBookVisual(ep.enabled);

        // Handle diegetic equip/power toggle from AwakeningRitual
        bool nextDiegeticToggle = true;
        if (AwakeningRitual::Get().ConsumePendingDiegeticToggle(nextDiegeticToggle))
        {
            StageCapabilities newCaps{};
            {
                std::lock_guard lock(g_diagMutex);
                _awakening.diegeticToggleOn = nextDiegeticToggle;
                newCaps = ResolveStageCapabilities(_awakening, ep.unlockMode);
                SyncPersistentStateToSerializer();
            }
            ExpandedPlayModifiers::Get().SyncFromPersistentState(ep.enabled);
            if (nextDiegeticToggle)
            {
                if (newCaps.stage == AwakeningStage::Dormant)
                {
                    GazeReticle::Get().NotifyMilestone(
                        "Your Vessel stirs - cast Sight of the First Canticle to inspect the four "
                        "keys.",
                        0xA6D8FFu, "MAGAltarsBlessingFireA2D", true);
                }
                else
                {
                    GazeReticle::Get().NotifyMilestone(
                        std::format("Moth-silk stills your sight - TrueGaze active [Stage {}: {}].",
                                    static_cast<unsigned>(newCaps.stage),
                                    AwakeningStageName(newCaps.stage)),
                        0xFFD966u, "MAGAltarsBlessingFireA2D", true);
                }
            }
            else
            {
                GazeReticle::Get().NotifyMilestone("You veil the Sight - TrueGaze rests.",
                                                   0xA6B3CCu, "UIMenuCancelSD", false);
            }
            Engine::TraceLogger::Get().LogExpandedPlayEvent(_sessionElapsedSec, "DIEGETIC_TOGGLE",
                                                            0u, "", 0u, 0.0f,
                                                            nextDiegeticToggle ? "on" : "off");
            logger::info("[TrueGaze::ExpandedPlay] Diegetic relic toggle -> {}",
                         nextDiegeticToggle ? "ON" : "OFF");
        }

        ReticleHudSnapshot hud{};
        {
            std::lock_guard lock(g_diagMutex);
            const auto caps = ResolveStageCapabilities(_awakening, ep.unlockMode);
            hud.enabled = caps.expandedActive;
            hud.isThirdPerson = isThirdPerson;
            hud.showReticle = ep.enableReticle && caps.allowReticle;
            hud.immersiveHideReticle = ep.immersiveHideReticle;
            hud.targetValid = _hadFocusedActor && caps.expandedActive;
            hud.gazeRegionId = _focusRegionId;
            hud.dwellSec = _focusDwellSec;
            hud.rapidEscalation = _focusRapidEscalation;
            hud.isMutualGaze = _focusIsMutual;
            hud.opacity = ep.reticleOpacity;
            hud.colorBlindMode = ep.colorBlindMode;
            hud.magnetismX = _magnetismX;
            hud.magnetismY = _magnetismY;
            hud.activeDemeanor = ClampDemeanorToStage(_demeanorCtrl.ActiveDemeanor(), caps);
            hud.demeanorMode = ep.demeanorMode;
            hud.demeanorPinned = _demeanorCtrl.IsPinned();
            hud.demeanorPinRemainingSec = _demeanorCtrl.PinRemainingSec();
            hud.frostVignetteAlpha = _frostVignetteAlpha;
            hud.enablePersuasionVisuals = ep.enablePersuasionVisuals;
            hud.persuasionVectorId = static_cast<uint8_t>(_lastPersuasionVector);
            hud.persuasionAffinityId = static_cast<uint8_t>(_lastAffinityTier);
            hud.persuasionProbeActive = _persuasionProbeActive && _hadFocusedActor;
            hud.persuasionGuardGlance = _persuasionGuardGlanceActive && _hadFocusedActor;
            hud.guardGlanceDirSign = _activeLawWitnessDirSign;
            hud.slyBribeArmed = _slyBribeArmedNow && _hadFocusedActor;
        }
        GazeReticle::Get().UpdateFrame(hud);
#else
        (void)deltaSeconds;
#endif
    }

    void ExpandedPlayManager::EvaluateFocus10Hz(float gameDays) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
        {
            return;
        }

        const auto ep = Engine::ConfigManager::GetSingleton().SnapshotExpandedPlay();
        auto* camera = RE::PlayerCamera::GetSingleton();
        const bool isThirdPerson = camera && camera->IsInThirdPerson();
        const ActorHeldPosture playerPosture = InspectActorHeldPosture(player);
        const bool playerHandsRaised = playerPosture.handsRaised;
        const bool playerWeaponDrawn = playerPosture.IsLethalBrandish();
        const bool playerSneaking = player->IsSneaking();
        const bool playerInCombat = player->IsInCombat();
        auto* playerLoc = player->GetCurrentLocation();
        const uint32_t locId = playerLoc ? static_cast<uint32_t>(playerLoc->GetFormID()) : 0u;

        auto* uiMgr = RE::UI::GetSingleton();
        const bool inDialogue = uiMgr && uiMgr->IsMenuOpen(RE::DialogueMenu::MENU_NAME);
        auto* topicMgr = RE::MenuTopicManager::GetSingleton();
        uint32_t dialogueActorId = 0u;
        if (inDialogue && topicMgr && topicMgr->speaker)
        {
            if (auto speakerRef = topicMgr->speaker.get())
            {
                dialogueActorId = static_cast<uint32_t>(speakerRef->GetFormID());
            }
        }

        bool fullHelm = false;
        bool hooded = false;
        bool darkBrotherhood = false;
        InspectPlayerHeadwear(player, fullHelm, hooded, darkBrotherhood);

        const auto& tuning = Engine::GazeEngine::Get().Tuning();
        Engine::PlayerGazeResolver::Params params{};
        params.baseToleranceDeg = tuning.crosshairToleranceDeg;
        params.maxRangeMeters = tuning.crosshairMaxRangeMeters;
        params.pointBlankMeters = tuning.crosshairPointBlankMeters;
        const auto gaze = Engine::PlayerGazeResolver::Resolve(params);

        uint32_t focusFormId = gaze.targetFormId != 0u ? gaze.targetFormId : dialogueActorId;
        RE::Actor* target =
            focusFormId != 0u
                ? RE::TESForm::LookupByID<RE::Actor>(static_cast<RE::FormID>(focusFormId))
                : nullptr;
        if (target && (target->IsPlayerRef() || target->IsDead()))
        {
            target = nullptr;
            focusFormId = 0u;
        }

        const auto savedSnapshot = CoSaveSerializer::Get().SnapshotState();

        // Probe Sanctuary location, player focus relic, studied Canticle, and spell cast
        // outside g_diagMutex so RE inventory/location queries never hold our internal lock.
        const uint8_t sanctuaryBit = AwakeningRitual::Get().CurrentPlayerSanctuaryBit();
        const std::string sanctuaryName = (sanctuaryBit != SanctuaryBits::kNone)
                                              ? AwakeningRitual::Get().CurrentPlayerSanctuaryName()
                                              : std::string{};
        const bool playerWearingFocus = AwakeningRitual::Get().IsPlayerWearingFocus();
        const bool ceremonyCastNow = AwakeningRitual::Get().ConsumePendingCeremonyCast();
        const bool hasStudiedBookNow =
            ceremonyCastNow ? AwakeningRitual::Get().IsPlayerCarryingStudiedCanticle() : false;
        const bool targetCarriesRelic =
            (focusFormId != 0u) ? AwakeningRitual::Get().DoesActorCarrySightRelic(focusFormId)
                                : false;

        // Zero-cell-conflict Sanctuary Relic Discovery (outside g_diagMutex so RE
        // container/placement calls stay lock-free). Sleeping Giant Inn hands its note
        // exclusively through speaking with Orgnar or Delphine below.
        uint8_t newlyDiscoveredSanctuaryBit = SanctuaryBits::kNone;
        std::string newlyDiscoveredRelicName;
        bool newlyDiscoveredPlaced = false;
        if (sanctuaryBit != SanctuaryBits::kNone &&
            sanctuaryBit != SanctuaryBits::kSleepingGiantInn && !playerInCombat && !inDialogue &&
            AwakeningRitual::Get().IsEslPluginLoaded() &&
            (savedSnapshot.awakening.discoveredSanctuaryRelicsMask & sanctuaryBit) == 0u &&
            _lastSanctuaryBit == sanctuaryBit && _sanctuaryStillnessSec >= 1.5f)
        {
            newlyDiscoveredSanctuaryBit = sanctuaryBit;
            newlyDiscoveredPlaced = AwakeningRitual::Get().BestowSanctuaryRelicIfEslLoaded(
                sanctuaryBit, newlyDiscoveredRelicName);
        }

        // Sight of the First Canticle is taught ONLY once Key I (The Word) has been earned
        // by finding and studying The First Canticle of the Eye or an Elder Scroll tome.
        bool newlyLearnedCanticleSpell = false;
        if (AwakeningRitual::Get().IsEslPluginLoaded() &&
            (savedSnapshot.awakening.tumblerMask & TumblerBits::kWord) != 0u)
        {
            newlyLearnedCanticleSpell = AwakeningRitual::Get().EnsureCanticleSpellLearned();
        }

        // Ensure "The Moth Priest's Pilgrimage" quest is running and keep journal objectives synced.
        AwakeningRitual::Get().SyncPilgrimageQuestProgress(savedSnapshot.awakening, false);

        // =========================================================================
        // THREE-METHOD QUEST & NOTE DISCOVERY (WP-11D / v2.1.0):
        //   Method 1: Sleeping Giant Inn — physical wax-sealed note placed right on
        //             the bar counter beside Orgnar (pick up or read) OR ask Orgnar /
        //             Delphine the dedicated top-level topic ("Have any quiet travelers
        //             or Moth Priests passed through?").
        //   Method 2: Vanilla Skyrim Courier (WICourier 0x00039F82) — if you enter
        //             the Sleeping Giant Inn and leave without taking the note, Skyrim's
        //             real Courier tracks you down within 1 Skyrim day with the note.
        //   Method 3: Direct Sanctuary Stillness (any of Skyrim's other 6 Sanctuaries)
        //             OR Elder Scroll / Moth quest discovery (MQ205, DA04, Dawnguard
        //             Prophet) — grants the living checklist note & begins the Quest
        //             even if you never set foot in Riverwood.
        // Note: NONE of these methods teaches Sight of the First Canticle early; the
        //       spell is learned ONLY by finding & studying The First Canticle of the
        //       Eye (Arcanaeum) or an Elder Scroll tome (Key I: The Word).
        // =========================================================================
        bool deliverInnNoteNow = false;
        bool innNoteDelivered = false;
        const char* questStartSource = "BarNote";
        uint32_t innNoteSpeakerId = dialogueActorId;
        bool markBarNotePlacedNow = false;
        bool markInnLeftNow = false;
        bool markCourierSentNow = false;

        const bool questAlreadyStarted =
            (savedSnapshot.awakening.discoveryHintsMask & DiscoveryHintBits::kInnRumor) != 0u;

        if (!questAlreadyStarted)
        {
            // Method 1A: Place the physical sealed note right on the Sleeping Giant Inn bar
            if (sanctuaryBit == SanctuaryBits::kSleepingGiantInn)
            {
                _visitedSleepingGiantThisSession = true;
                if (AwakeningRitual::Get().IsEslPluginLoaded())
                {
                    if (AwakeningRitual::Get().PlaceNoteOnSleepingGiantBar())
                    {
                        markBarNotePlacedNow = true;
                    }
                }
            }
            else if (_visitedSleepingGiantThisSession &&
                     (savedSnapshot.awakening.discoveryHintsMask &
                      DiscoveryHintBits::kInnVisitedLeft) == 0u)
            {
                // Player visited the Sleeping Giant Inn and left without taking the note!
                markInnLeftNow = true;
                _innLeftGameDay = gameDays;
            }

            // Method 1B: Orgnar / Delphine dialogue completion ("It's all in this note.")
            const uint32_t noteInfoId = AwakeningRitual::Get().GetInnNoteInfoFormId();
            if (noteInfoId != 0u && topicMgr)
            {
                const bool spokenNow =
                    (topicMgr->currentTopicInfo &&
                     static_cast<uint32_t>(topicMgr->currentTopicInfo->GetFormID()) == noteInfoId) ||
                    (topicMgr->lastTopicInfo &&
                     static_cast<uint32_t>(topicMgr->lastTopicInfo->GetFormID()) == noteInfoId);
                if (spokenNow)
                {
                    deliverInnNoteNow = true;
                    questStartSource = "OrgnarDialogue";
                    innNoteDelivered =
                        AwakeningRitual::Get().GrantInnNoteToPlayerOnly(questStartSource);
                    AwakeningRitual::Get().RemovePlacedBarNoteIfStillInWorld();
                }
            }

            // Method 1C & 2B: Player picked up the note from the bar, read it on the bar,
            // OR received it from Skyrim's vanilla Courier!
            if (!deliverInnNoteNow && (_sessionElapsedSec - _lastNoteInventoryCheckSec) >= 0.5f)
            {
                _lastNoteInventoryCheckSec = _sessionElapsedSec;
                const bool openedNote = AwakeningRitual::Get().ConsumeCassianNoteOpened();
                const bool hasNote = AwakeningRitual::Get().IsPlayerCarryingCassianNote();
                if (hasNote || openedNote)
                {
                    deliverInnNoteNow = true;
                    innNoteDelivered = hasNote;
                    questStartSource =
                        ((savedSnapshot.awakening.discoveryHintsMask &
                          DiscoveryHintBits::kCourierSent) != 0u)
                            ? "SkyrimCourier"
                            : "SleepingGiantBar";
                    if (!hasNote && openedNote)
                    {
                        innNoteDelivered =
                            AwakeningRitual::Get().GrantInnNoteToPlayerOnly(questStartSource);
                    }
                    AwakeningRitual::Get().RemovePlacedBarNoteIfStillInWorld();
                }
            }

            // Method 2A: Dispatch Skyrim's real vanilla WICourier within 1 Skyrim day after
            // leaving the Sleeping Giant Inn without taking the note
            if (!deliverInnNoteNow && sanctuaryBit != SanctuaryBits::kSleepingGiantInn &&
                !playerInCombat && !inDialogue &&
                ((savedSnapshot.awakening.discoveryHintsMask &
                  DiscoveryHintBits::kInnVisitedLeft) != 0u ||
                 markInnLeftNow) &&
                (savedSnapshot.awakening.discoveryHintsMask & DiscoveryHintBits::kCourierSent) ==
                    0u)
            {
                if (_innLeftGameDay < 0.0f)
                {
                    // Loaded from a save where the player already left the Inn earlier
                    _innLeftGameDay = gameDays;
                }
                const float daysSinceLeft = gameDays - _innLeftGameDay;
                // 0.25 Skyrim days = 6 in-game hours (reliably delivers within 1 Skyrim day!)
                if (daysSinceLeft >= 0.25f)
                {
                    if (AwakeningRitual::Get().DispatchCourierWithCassianNote())
                    {
                        markCourierSentNow = true;
                    }
                }
            }

            // Method 3A: Direct Sanctuary Stillness at any of Skyrim's other 6 Sanctuaries
            // (zero Riverwood visit needed!)
            if (!deliverInnNoteNow && sanctuaryBit != SanctuaryBits::kNone &&
                sanctuaryBit != SanctuaryBits::kSleepingGiantInn && !playerInCombat && !inDialogue &&
                _lastSanctuaryBit == sanctuaryBit && _sanctuaryStillnessSec >= 3.0f)
            {
                deliverInnNoteNow = true;
                questStartSource = "SanctuaryDiscovery";
                innNoteDelivered =
                    AwakeningRitual::Get().GrantInnNoteToPlayerOnly(questStartSource);
                AwakeningRitual::Get().RemovePlacedBarNoteIfStillInWorld();
            }

            if (deliverInnNoteNow)
            {
                AwakeningState startedState = savedSnapshot.awakening;
                startedState.discoveryHintsMask = static_cast<uint8_t>(
                    startedState.discoveryHintsMask | DiscoveryHintBits::kInnRumor);
                AwakeningRitual::Get().SyncPilgrimageQuestProgress(startedState, true);
            }
        }
        else
        {
            _pendingInnNoteHandoff = false;
            _pendingInnNoteSpeakerId = 0u;
        }

        // Method 3B: Throttled in-world discovery check for vanilla Elder Scroll / Moth quests
        // (MQ205 Elder Knowledge, DA04 Discerning the Transmundane, Dawnguard Prophet)
        bool checkElderScrollQuestNow = false;
        if ((_sessionElapsedSec - _lastDiscoveryCheckSec) >= 4.0f)
        {
            _lastDiscoveryCheckSec = _sessionElapsedSec;
            if ((savedSnapshot.awakening.discoveryHintsMask &
                 DiscoveryHintBits::kElderScrollQuest) == 0u)
            {
                checkElderScrollQuestNow = HasStartedElderScrollOrMothQuest();
                if (checkElderScrollQuestNow && !questAlreadyStarted && !deliverInnNoteNow)
                {
                    deliverInnNoteNow = true;
                    questStartSource = "ElderScrollQuest";
                    innNoteDelivered =
                        AwakeningRitual::Get().GrantInnNoteToPlayerOnly(questStartSource);
                    AwakeningState startedState = savedSnapshot.awakening;
                    startedState.discoveryHintsMask = static_cast<uint8_t>(
                        startedState.discoveryHintsMask | DiscoveryHintBits::kInnRumor);
                    AwakeningRitual::Get().SyncPilgrimageQuestProgress(startedState, true);
                }
            }
        }

        // WP-11C: Inspect player & focused NPC factions and worn attire outside g_diagMutex
        const uint32_t playerFactionBits =
            ep.moduleFactionSight ? InspectActorFactionBits(player, true) : FactionBits::kNone;
        const uint32_t targetFactionBits = (ep.moduleFactionSight && target)
                                               ? InspectActorFactionBits(target, false)
                                               : FactionBits::kNone;
        bool pDbGear = false, pTgGear = false, pCollegeRobes = false, pImpArmor = false,
             pScArmor = false, pHunterGear = false;
        if (ep.moduleFactionSight && playerFactionBits != FactionBits::kNone)
        {
            InspectWornFactionArmor(player, pDbGear, pTgGear, pCollegeRobes, pImpArmor, pScArmor,
                                    pHunterGear);
        }
        const bool targetIsPriestOrVigilant =
            target && (IsInFactionId(target, VanillaFactionIds::kJobPriest) ||
                       IsInFactionId(target, VanillaFactionIds::kVigilantOfStendarr));
        const bool targetIsMerchant =
            target && (IsInFactionId(target, VanillaFactionIds::kJobMerchant) ||
                       IsInFactionId(target, VanillaFactionIds::kJobInnkeeper));
        const bool targetIsUndead =
            target && ((targetFactionBits & FactionBits::kVampire) != 0u ||
                       (RE::TESForm::LookupByID<RE::BGSKeyword>(
                            static_cast<RE::FormID>(VanillaFactionIds::kUndeadKeyword)) &&
                        target->HasKeyword(RE::TESForm::LookupByID<RE::BGSKeyword>(
                            static_cast<RE::FormID>(VanillaFactionIds::kUndeadKeyword)))));
        const bool isDaylight = IsDaylightNow();

        std::unique_lock lock(g_diagMutex);
        // Preserve booksFullyStudiedCount / Tumbler A (The Word) updates from BookStudyTracker.
        _presenceSig.Load(savedSnapshot.sign);
        _awakening.tumblerMask =
            static_cast<uint8_t>(_awakening.tumblerMask | savedSnapshot.awakening.tumblerMask);
        _awakening.visitedSanctuariesMask = static_cast<uint8_t>(
            _awakening.visitedSanctuariesMask | savedSnapshot.awakening.visitedSanctuariesMask);
        _awakening.discoveredSanctuaryRelicsMask =
            static_cast<uint8_t>(_awakening.discoveredSanctuaryRelicsMask |
                                 savedSnapshot.awakening.discoveredSanctuaryRelicsMask);
        _awakening.discoveryHintsMask = static_cast<uint8_t>(
            _awakening.discoveryHintsMask | savedSnapshot.awakening.discoveryHintsMask);
        if (savedSnapshot.awakening.ceremonyCompleted)
        {
            _awakening.ceremonyCompleted = true;
        }

        // WP-11D: Track bar note placement, Inn departure courier timer, and Courier dispatch
        if (markBarNotePlacedNow &&
            MarkDiscoveryHintShown(_awakening, DiscoveryHintBits::kBarNotePlaced))
        {
            _stateDirty = true;
            SyncPersistentStateToSerializer();
            GazeReticle::Get().NotifyMilestone(
                "A wax-sealed note rests on the Sleeping Giant's bar beside Orgnar.", 0xA6D8FFu, "",
                false);
            Engine::TraceLogger::Get().LogExpandedPlayEvent(
                _sessionElapsedSec, "BAR_NOTE_PLACED", 0u, "SleepingGiantInn", 0u, 0.0f,
                "bar_note_placed_beside_orgnar");
        }
        if (markInnLeftNow &&
            MarkDiscoveryHintShown(_awakening, DiscoveryHintBits::kInnVisitedLeft))
        {
            _stateDirty = true;
            SyncPersistentStateToSerializer();
            Engine::TraceLogger::Get().LogExpandedPlayEvent(
                _sessionElapsedSec, "INN_LEFT_COURIER_ARMED", 0u, "SleepingGiantInn", 0u, gameDays,
                "courier_armed_within_one_day");
        }
        if (markCourierSentNow &&
            MarkDiscoveryHintShown(_awakening, DiscoveryHintBits::kCourierSent))
        {
            _stateDirty = true;
            SyncPersistentStateToSerializer();
            Engine::TraceLogger::Get().LogExpandedPlayEvent(
                _sessionElapsedSec, "COURIER_DISPATCHED", 0x00039F82u, "WICourier", 0u, gameDays,
                "cassian_note_sent_to_courier");
        }

        // WP-11D: Three-Method Quest & Note Discovery -> Living Note in inventory + Moth Priest
        // Presence Blessing (+2 Max Presence) + "The Moth Priest's Pilgrimage" begins in Journal!
        if (deliverInnNoteNow &&
            MarkDiscoveryHintShown(_awakening, DiscoveryHintBits::kInnRumor))
        {
            if (checkElderScrollQuestNow)
            {
                (void)MarkDiscoveryHintShown(_awakening, DiscoveryHintBits::kElderScrollQuest);
            }
            _presenceSig.AwardMaxPresence(2.0f, gameDays, false);
            _stateDirty = true;
            SyncPersistentStateToSerializer();

            const std::string_view srcView(questStartSource);
            std::string innMsg;
            if (srcView == "SleepingGiantBar")
            {
                innMsg = "Moth Priest's Presence blesses you -- Found Brother Cassian's Note on "
                         "the Sleeping Giant's bar. Read it to begin the TrueGaze Pilgrimage.";
            }
            else if (srcView == "SkyrimCourier")
            {
                innMsg = "Moth Priest's Presence blesses you -- The Courier delivered Brother "
                         "Cassian's Note. Read it in your inventory to begin the TrueGaze "
                         "Pilgrimage.";
            }
            else if (srcView == "SanctuaryDiscovery")
            {
                innMsg = "Moth Priest's Presence blesses you at this Sanctuary -- Received Brother "
                         "Cassian's Note. Read it in your inventory to begin the TrueGaze "
                         "Pilgrimage.";
            }
            else if (srcView == "ElderScrollQuest")
            {
                innMsg = "Moth Priest's Presence stirs with the Elder Scrolls -- Received Brother "
                         "Cassian's Note. Read it in your inventory to begin the TrueGaze "
                         "Pilgrimage.";
            }
            else if (innNoteDelivered)
            {
                innMsg = "Moth Priest's Presence blesses you -- Received Brother Cassian's Note. "
                         "Read it in your inventory to begin the TrueGaze Pilgrimage.";
            }
            else
            {
                innMsg = "Moth Priest's Presence blesses you at the Sleeping Giant Inn -- "
                         "The TrueGaze Pilgrimage begins.";
            }

            GazeReticle::Get().NotifyMilestone(innMsg, 0xFFD966u, "MAGAltarsBlessingFireA2D", true);
            Engine::TraceLogger::Get().LogExpandedPlayEvent(
                _sessionElapsedSec, "INN_NOTE_BLESSING", innNoteSpeakerId, questStartSource, 0u,
                0.0f, "note_received_and_presence_blessed");
        }
        else if (checkElderScrollQuestNow &&
                 MarkDiscoveryHintShown(_awakening, DiscoveryHintBits::kElderScrollQuest))
        {
            _stateDirty = true;
            SyncPersistentStateToSerializer();
            GazeReticle::Get().NotifyMilestone(
                "The Elder Scrolls demand stillness before vision - seek The First Canticle of "
                "the Eye in Skyrim's Sanctuaries.",
                0xA6D8FFu, "UIQuestUpdate", true);
        }

        if (newlyDiscoveredSanctuaryBit != SanctuaryBits::kNone &&
            MarkSanctuaryRelicDiscovered(_awakening, newlyDiscoveredSanctuaryBit))
        {
            _stateDirty = true;
            SyncPersistentStateToSerializer();
            if (newlyDiscoveredPlaced)
            {
                const auto spec = GetSanctuaryRelicSpec(newlyDiscoveredSanctuaryBit);
                GazeReticle::Get().NotifyMilestone(
                    spec.discoveryWhisper && spec.discoveryWhisper[0] != '\0'
                        ? std::string(spec.discoveryWhisper)
                        : std::format("Relic discovered: {}.", newlyDiscoveredRelicName),
                    0xFFD966u, "MAGAltarsBlessingFireA2D", true);
                Engine::TraceLogger::Get().LogExpandedPlayEvent(
                    _sessionElapsedSec, "SANCTUARY_RELIC_DISCOVERED", 0u,
                    spec.sanctuaryTitle ? spec.sanctuaryTitle : "", 0u, 0.0f,
                    newlyDiscoveredRelicName.c_str());
            }
        }
        if (newlyLearnedCanticleSpell)
        {
            GazeReticle::Get().NotifyMilestone(
                "Spell learned: Sight of the First Canticle (Illusion, 0 Magicka).", 0xA6D8FFu,
                "UISkillsGlowSD", true);
            Engine::TraceLogger::Get().LogExpandedPlayEvent(_sessionElapsedSec,
                                                            "CANTICLE_SPELL_LEARNED", 0u, "", 0u,
                                                            0.0f, "Sight of the First Canticle");
        }

        const auto publishAwakeningEvent = [&](const AwakeningEventResult& ev, const char* tag)
        {
            if (!ev.progressed)
            {
                return;
            }
            _stateDirty = true;
            if (!ev.whisper.empty())
            {
                const char* soundId = "UIQuestUpdate";
                if (std::strcmp(tag, "FinalCeremony") == 0 ||
                    ev.newStage == AwakeningStage::CanticleAwakened)
                {
                    soundId = "UILevelUpSD";
                }
                else if (ev.stageAdvanced)
                {
                    soundId = "UISkillIncreaseSD";
                }
                GazeReticle::Get().NotifyMilestone(ev.whisper, ev.whisperColorRgb, soundId, true);
            }
            const std::string detail = std::format(
                "tag={} stage={}({}) glyphs={}", tag, static_cast<unsigned>(ev.newStage),
                AwakeningStageName(ev.newStage), FormatTumblerGlyphs(_awakening));
            Engine::TraceLogger::Get().LogExpandedPlayEvent(
                _sessionElapsedSec, "AWAKENING_PROGRESS", focusFormId, SafeActorName(target),
                _focusRegionId, _focusDwellSec, detail.c_str());
            logger::info("[TrueGaze::ExpandedPlay] Awakening progress: {}", detail);
            SyncPersistentStateToSerializer();
            ExpandedPlayModifiers::Get().SyncFromPersistentState(ep.enabled);
        };

        // Tumbler D (The Pilgrimage): mindful stillness at 2+ base-Skyrim Sanctuaries
        if (sanctuaryBit != SanctuaryBits::kNone && !playerInCombat && !inDialogue)
        {
            if (sanctuaryBit != _lastSanctuaryBit)
            {
                _lastSanctuaryBit = sanctuaryBit;
                _sanctuaryStillnessSec = 0.0f;
            }
            _sanctuaryStillnessSec += kFocusPeriodSec;
            if (_sanctuaryStillnessSec >= 3.0f)
            {
                const auto pilgEv =
                    RecordSanctuaryStillness(_awakening, sanctuaryBit, ep.unlockMode);
                publishAwakeningEvent(pilgEv, "Pilgrimage");
            }
        }
        else
        {
            _sanctuaryStillnessSec = 0.0f;
            _lastSanctuaryBit = SanctuaryBits::kNone;
        }

        // Sight of the First Canticle cast & Final Sanctuary Ceremony handler:
        //   - Post-Stage-4 cast -> toggles TrueGaze Expanded ON/OFF diegetically
        //   - All requirements met -> completes Final Sanctuary Ceremony (Stage 4)
        //   - Missing any requirement -> shows Skyrim parchment MessageBox checklist!
        if (ceremonyCastNow)
        {
            if (_awakening.ceremonyCompleted)
            {
                _awakening.diegeticToggleOn = !_awakening.diegeticToggleOn;
                _stateDirty = true;
                SyncPersistentStateToSerializer();
                ExpandedPlayModifiers::Get().SyncFromPersistentState(ep.enabled);
                if (_awakening.diegeticToggleOn)
                {
                    GazeReticle::Get().NotifyMilestone(
                        "The First Canticle opens - TrueGaze Expanded active [Stage 4].", 0xFFD966u,
                        "MAGAltarsBlessingFireA2D", true);
                }
                else
                {
                    GazeReticle::Get().NotifyMilestone("You veil the Sight - TrueGaze rests.",
                                                       0xA6B3CCu, "UIMenuCancelSD", false);
                }
                Engine::TraceLogger::Get().LogExpandedPlayEvent(
                    _sessionElapsedSec, "CANTICLE_CAST_TOGGLE", 0u, "", 0u, 0.0f,
                    _awakening.diegeticToggleOn ? "on" : "off");
            }
            else
            {
                const bool inSanctuary = (sanctuaryBit != SanctuaryBits::kNone);
                const auto cerEv = TryCompleteSanctuaryCeremony(
                    _awakening, inSanctuary, playerWearingFocus, hasStudiedBookNow, ep.unlockMode);
                if (cerEv.progressed)
                {
                    _presenceSig.TriggerCanticleAwakening(gameDays);
                    if (focusFormId != 0u)
                    {
                        _ledger.RecordNotableEvent(focusFormId, NotableEventType::CanticleAwakening,
                                                   gameDays, locId, 0u);
                    }
                    publishAwakeningEvent(cerEv, "FinalCeremony");
                    RE::DebugMessageBox(
                        "Sight of the First Canticle\n\n"
                        "All four keys turn as one within sacred walls!\n\n"
                        "Stage 4: Canticle Awakened (+5.0 Max Presence).\n"
                        "Full TrueGaze Expanded Play is now unlocked.\n\n"
                        "Cast Sight of the First Canticle anytime hereafter to veil or unveil "
                        "TrueGaze.");
                }
                else
                {
                    const std::string checklist = FormatRitualStatusChecklist(
                        _awakening, ep.unlockMode, inSanctuary, sanctuaryName, playerWearingFocus,
                        hasStudiedBookNow);
                    GazeReticle::Get().NotifyMilestone(
                        "The First Canticle is not yet aligned - inspect the remaining keys.",
                        0xA6D8FFu, "UIMenuFocus", true);
                    RE::DebugMessageBox(checklist.c_str());

                    const std::string detail = std::format(
                        "incomplete glyphs={} sanctuary={} vessel={} book={}",
                        FormatTumblerGlyphs(_awakening), inSanctuary ? "yes" : "no",
                        playerWearingFocus ? "yes" : "no", hasStudiedBookNow ? "yes" : "no");
                    Engine::TraceLogger::Get().LogExpandedPlayEvent(_sessionElapsedSec,
                                                                    "RITUAL_CHECK_INCOMPLETE", 0u,
                                                                    "", 0u, 0.0f, detail.c_str());
                    logger::info("[TrueGaze::ExpandedPlay] Sight of the First Canticle cast "
                                 "(incomplete): {}",
                                 detail);
                }
            }
        }
        else if (!_awakening.ceremonyCompleted &&
                 (_awakening.tumblerMask & TumblerBits::kAllFour) == TumblerBits::kAllFour &&
                 sanctuaryBit != SanctuaryBits::kNone && playerWearingFocus &&
                 _sanctuaryStillnessSec >= 4.5f)
        {
            // Passive stillness fallback for DLL-only players without the optional ESL spell
            const bool hasStudiedBook = AwakeningRitual::Get().IsPlayerCarryingStudiedCanticle();
            const auto cerEv = TryCompleteSanctuaryCeremony(_awakening, true, playerWearingFocus,
                                                            hasStudiedBook, ep.unlockMode);
            if (cerEv.progressed)
            {
                _presenceSig.TriggerCanticleAwakening(gameDays);
                if (focusFormId != 0u)
                {
                    _ledger.RecordNotableEvent(focusFormId, NotableEventType::CanticleAwakening,
                                               gameDays, locId, 0u);
                }
                publishAwakeningEvent(cerEv, "FinalCeremony");
            }
        }

        const auto stageCaps = ResolveStageCapabilities(_awakening, ep.unlockMode);

        DemeanorContext ctx{};
        ctx.inJarlCourtOrTemple = IsCourtOrTempleLocation(playerLoc);
        ctx.weaponDrawn = playerWeaponDrawn;
        ctx.sneaking = playerSneaking;
        ctx.inCombat = playerInCombat;
        ctx.wearingDarkBrotherhoodArmor = darkBrotherhood;
        ctx.predatoryUnlocked =
            stageCaps.allowPredatoryDemeanor && _presenceSig.CurrentTier() >= PresenceTier::Steady;
        ctx.inPeacefulTown = IsPeacefulTownLocation(playerLoc);
        ctx.currentLocationFormId = locId;
        ctx.dialogueTargetFormId = dialogueActorId;
        ctx.targetIsChild = target ? target->IsChild() : false;
        const GazeLedgerEntry* existingEntry = target ? _ledger.Find(focusFormId) : nullptr;
        ctx.targetRelationship = ClassifyRelationshipTier(target, existingEntry);
        _lastDemeanorCtx = ctx;

        const Demeanor rawDemeanor =
            _demeanorCtrl.Update(ep.demeanorMode, ep.activeDemeanor, ctx, kFocusPeriodSec, nullptr);
        const Demeanor activeDemeanor = ClampDemeanorToStage(rawDemeanor, stageCaps);
        if (!_loggedDemeanorInit || activeDemeanor != _lastLoggedDemeanor)
        {
            _loggedDemeanorInit = true;
            _lastLoggedDemeanor = activeDemeanor;
            const std::string detail = std::format(
                "demeanor={} mode={} pinned={}", DemeanorName(activeDemeanor),
                DemeanorModeName(ep.demeanorMode), _demeanorCtrl.IsPinned() ? "yes" : "no");
            Engine::TraceLogger::Get().LogExpandedPlayEvent(
                _sessionElapsedSec, "DEMEANOR_CHANGE", focusFormId, SafeActorName(target),
                _focusRegionId, _focusDwellSec, detail.c_str());
            logger::info("[TrueGaze::ExpandedPlay] Demeanor: {}", detail);
        }
        const DemeanorProfile demeanorProfile =
            BuildDemeanorProfile(activeDemeanor, isThirdPerson, ep.thirdPersonEfficacy);

        if (ep.modulePresenceProgression)
        {
            _presenceSig.TickRegeneration(
                kFocusPeriodSec, gameDays, IsPeacefulTownLocation(playerLoc),
                ClassifyScholarshipRank(savedSnapshot.sign.booksFullyStudiedCount));
        }

        // --- Active Staring Contest takes priority over passive evaluation ---
        if (_contest.IsActive())
        {
            const uint32_t oppId = _contest.OpponentFormId();
            const bool lookingAtOpponent = (focusFormId == oppId) && gaze.onFace;
            if (auto* oppRt = Engine::GazeEngine::Get().FindActor(oppId))
            {
                oppRt->hcepMode = 0u; // LOGIC challenge lock
                oppRt->hasModeOverride = true;
                oppRt->modeOverrideTimerSec = 0.5f;
                oppRt->crosshairHoldTimerSec = 0.5f;
            }
            const auto hcepIntent = Engine::PlayerGazeResolver::LastIntent();
            const bool playerBlinked =
                hcepIntent.valid && !hcepIntent.stale && hcepIntent.blinkSuppressed;
            const auto status = _contest.Tick(kFocusPeriodSec, lookingAtOpponent, playerBlinked,
                                              _presenceSig.Presence().maximum);
            _frostVignetteAlpha = _contest.BlinkPressure01() * 0.65f;
            _focusActorFormId = oppId;
            _focusRegionId = Region::LeftEye;
            _focusDwellSec = _contest.ElapsedSec();
            _focusIsMutual = lookingAtOpponent;
            _hadFocusedActor = true;

            if (status == ContestStatus::Won)
            {
                _presenceSig.RecordStareDownOutcome(true, gameDays);
                _ledger.RecordNotableEvent(oppId, NotableEventType::StareDownWon, gameDays, locId,
                                           0u);
                _frostVignetteAlpha = 0.0f;
                GazeReticle::Get().PushWhisper("Victory! Your unblinking gaze (+1.0 Presence).",
                                               0xFFD966u);
                Engine::TraceLogger::Get().LogExpandedPlayEvent(
                    _sessionElapsedSec, "CONTEST_END", oppId, "", Region::LeftEye,
                    _contest.ElapsedSec(), "won:+1.0Presence");
                logger::info("[TrueGaze::ExpandedPlay] Staring contest WON vs {:08X} ({:.1f}s).",
                             oppId, _contest.ElapsedSec());
                SyncPersistentStateToSerializer();
                ExpandedPlayModifiers::Get().SyncFromPersistentState(ep.enabled);
            }
            else if (status == ContestStatus::LostLookedAway ||
                     status == ContestStatus::LostBlinked)
            {
                _presenceSig.RecordStareDownOutcome(false, gameDays);
                _ledger.RecordNotableEvent(oppId, NotableEventType::StareDownLost, gameDays, locId,
                                           0u);
                _frostVignetteAlpha = 0.0f;
                GazeReticle::Get().PushWhisper("You broke gaze first.", 0xFF9966u);
                const char* lossReason =
                    (status == ContestStatus::LostBlinked) ? "lost:blinked" : "lost:lookedAway";
                Engine::TraceLogger::Get().LogExpandedPlayEvent(_sessionElapsedSec, "CONTEST_END",
                                                                oppId, "", Region::LeftEye,
                                                                _contest.ElapsedSec(), lossReason);
                logger::info(
                    "[TrueGaze::ExpandedPlay] Staring contest LOST vs {:08X} ({}, {:.1f}s).", oppId,
                    lossReason, _contest.ElapsedSec());
                SyncPersistentStateToSerializer();
            }
            else
            {
                SyncPersistentStateToSerializer();
            }
            return;
        }

        _frostVignetteAlpha = 0.0f;

        // Tick smooth FaceGen expression ramps & neutral resets for any NPCs we nudged
        ResponseLadder::TickExpressionResets(_sessionElapsedSec, kFocusPeriodSec, focusFormId);

        // Expire transient Combat Sight / Creature Sight tactical modifiers cleanly
        if (_tacticalBuffActive && _sessionElapsedSec >= _tacticalBuffExpiresSessionSec)
        {
            _tacticalBuffActive = false;
            _tacticalBuffExpiresSessionSec = 0.0f;
            ExpandedPlayModifiers::Get().SetTransientTacticalBonuses({});
        }

        // Expire or clear transient persuasion rapport bonus when not in dialogue/barter
        {
            auto* uiNow = RE::UI::GetSingleton();
            const bool inBarter = uiNow && uiNow->IsMenuOpen(RE::BarterMenu::MENU_NAME);
            if (_rapportTargetFormId != 0u)
            {
                if (inDialogue || inBarter)
                {
                    // Pause expiry while actively negotiating / bartering
                    _rapportExpiresSessionSec =
                        std::max(_rapportExpiresSessionSec, _sessionElapsedSec + 10.0f);
                }
                else
                {
                    const bool switchedNpc =
                        (focusFormId != 0u && focusFormId != _rapportTargetFormId);
                    const bool expired = (_sessionElapsedSec >= _rapportExpiresSessionSec);
                    if (switchedNpc || expired)
                    {
                        _rapportTargetFormId = 0u;
                        _rapportExpiresSessionSec = 0.0f;
                        ExpandedPlayModifiers::Get().SetTransientRapportSpeechBonus(0.0f);
                    }
                }
            }
        }

        // During DialogueMenu, CrosshairPickData is inactive (gaze.faceX/Y/Z are zeroed)
        // and Skyrim's UI/FaceGen dialogue systems own the cursor and speaker face.
        // Never draw the 1st-person reticle or classify against zeroed coordinates in dialogue.
        if (inDialogue || !target || gaze.targetFormId == 0u)
        {
            _magnetismX = 0.0f;
            _magnetismY = 0.0f;
            if (!inDialogue && _focusActorFormId != 0u && _focusLossGraceSec < 0.25f)
            {
                // Brief crosshair drop-out grace prevents distant sway from strobing dwell/faces
                _focusLossGraceSec += kFocusPeriodSec;
                return;
            }
            _focusActorFormId = 0u;
            _focusDwellSec = 0.0f;
            _focusLossGraceSec = 0.0f;
            _lastLoggedDwellBand = DwellBand::None;
            _focusIsMutual = false;
            _focusRapidEscalation = false;
            _hadFocusedActor = false;
            _ledgerCommitAccumSec = 0.0f;
            _lastInterpretation = Interpretation::None;
            _lastPersuasionVector = PersuasionVector::None;
            _lastBribeCostGold = 0;
            _activeNeutralWitnessCount = 0u;
            _activeLawWitnessCount = 0u;
            _probeTellSent = false;
            _txCommittedForHold = false;
            _lastStrength = 0.0f;
            if (_stateDirty)
            {
                SyncPersistentStateToSerializer();
                _stateDirty = false;
            }
            return;
        }
        _focusLossGraceSec = 0.0f;

        // Compute soft 1st-person reticle magnetism toward the target's face anchor
        _magnetismX = 0.0f;
        _magnetismY = 0.0f;
        if (!isThirdPerson && ep.enableReticle && !ep.immersiveHideReticle &&
            ep.reticleMagnetismStrength > 0.0f)
        {
            if (auto* worldCam = RE::Main::WorldRootCamera())
            {
                float sx = 0.0f;
                float sy = 0.0f;
                float sz = 0.0f;
                const RE::NiPoint3 facePt{gaze.faceX, gaze.faceY, gaze.faceZ};
                if (worldCam->WorldPtToScreenPt3(facePt, sx, sy, sz, 0.0001f) &&
                    std::isfinite(sx) && std::isfinite(sy) && std::isfinite(sz) && sz > 0.0f)
                {
                    const float dirX = sx * 1280.0f - 640.0f;
                    const float dirY = (1.0f - sy) * 720.0f - 360.0f;
                    const auto mag = ComputeSoftMagnetismOffset(
                        gaze.faceAngleDeg, dirX, dirY, ep.reticleMagnetismStrength,
                        playerHandsRaised || playerInCombat);
                    _magnetismX = mag.offsetX;
                    _magnetismY = mag.offsetY;
                }
            }
        }

        uint8_t regionId = ClassifyPlayerCrosshairRegion(player, target, gaze, _focusRegionId);
        if (isThirdPerson)
        {
            if (activeDemeanor == Demeanor::Deferential || activeDemeanor == Demeanor::Evasive)
            {
                regionId = Region::Ground;
            }
            else if (activeDemeanor == Demeanor::Warm &&
                     static_cast<int>(_sessionElapsedSec) % 3 == 0)
            {
                regionId = Region::Mouth;
            }
            else
            {
                regionId = Region::LeftEye;
            }
        }

        const char* actorName = SafeActorName(target);
        if (_focusActorFormId != focusFormId || _focusRegionId != regionId)
        {
            const uint8_t prevReg = _focusRegionId;
            const bool actorChanged = (_focusActorFormId != focusFormId);
            if (actorChanged)
            {
                _focusDwellSec = 0.0f;
                _lastLoggedDwellBand = DwellBand::None;
                _ledgerCommitAccumSec = 0.0f;
                _activeNeutralWitnessCount = 0u;
                _activeLawWitnessCount = 0u;
                _probeTellSent = false;
                _txCommittedForHold = false;
            }
            else if ((regionId == Region::RightHand || regionId == Region::LeftHand ||
                      regionId == Region::Torso) &&
                     (_focusRegionId == Region::LeftEye || _focusRegionId == Region::RightEye ||
                      _focusRegionId == Region::Forehead))
            {
                // Scanning between face and purse/torso starts a fresh pre-commit probe
                _probeTellSent = false;
                _txCommittedForHold = false;
            }
            _focusActorFormId = focusFormId;
            _focusRegionId = regionId;

            const std::string detail = std::format(
                "actorChanged={} from={} to={} onFace={} angle={:.1f}", actorChanged ? "yes" : "no",
                ShortRegionLabel(prevReg), ShortRegionLabel(regionId), gaze.onFace ? "yes" : "no",
                gaze.faceAngleDeg);
            Engine::TraceLogger::Get().LogExpandedPlayEvent(_sessionElapsedSec, "FOCUS_CHANGE",
                                                            focusFormId, actorName, regionId,
                                                            _focusDwellSec, detail.c_str());
            if (actorChanged)
            {
                logger::info("[TrueGaze::ExpandedPlay] Focus '{}' ({:08X}): {}", actorName,
                             focusFormId, detail);
            }
        }
        _focusDwellSec += kFocusPeriodSec;
        _hadFocusedActor = true;

        auto* targetRt = Engine::GazeEngine::Get().FindActor(focusFormId);
        const bool isMutual = targetRt && targetRt->trackedTargetFormId == player->GetFormID() &&
                              targetRt->mutualGazeHoldSec > 0.0f;
        _focusIsMutual = isMutual;

        // Tumbler B (The Vessel): holding >= 3.0s face/mutual gaze while wearing a Focus Relic
        const bool attuningEyeContact = isMutual || gaze.onFace;
        if (attuningEyeContact && playerWearingFocus &&
            (_awakening.tumblerMask & TumblerBits::kVessel) == 0u)
        {
            if (_focusDwellSec >= 1.2f && _focusDwellSec < 1.35f)
            {
                GazeReticle::Get().PushWhisper(
                    "Your Vessel warms - hold their eyes a moment longer...", 0xFFD966u);
            }
            if (_focusDwellSec >= 3.0f)
            {
                const auto vesEv = MarkTumbler(_awakening, TumblerBits::kVessel, ep.unlockMode,
                                               "Your relic stills the gaze between you");
                publishAwakeningEvent(vesEv, "Vessel");
            }
        }

        // Tumbler C (Shared Sight): face/mutual gaze with a Follower/NPC carrying a Sight Relic,
        // OR holding gaze with a friendly NPC while a Sight Relic is dropped beside them (~2.6 m).
        if (attuningEyeContact && _focusDwellSec >= 1.5f &&
            (_awakening.tumblerMask & TumblerBits::kSharedSight) == 0u)
        {
            bool kindredReady = targetCarriesRelic;
            if (!kindredReady)
            {
                kindredReady = AwakeningRitual::Get().TryOfferNearbySightRelicToActor(focusFormId);
            }
            if (kindredReady)
            {
                const auto shEv = MarkTumbler(_awakening, TumblerBits::kSharedSight, ep.unlockMode,
                                              "Kindred eyes answer the Sight relic you shared");
                publishAwakeningEvent(shEv, "SharedSight");
            }
        }

        // Re-evaluate stage capabilities immediately if a tumbler just lit on this frame
        const auto liveCaps = ResolveStageCapabilities(_awakening, ep.unlockMode);
        if (!liveCaps.expandedActive)
        {
            _hadFocusedActor = false;
            return;
        }

        // Sandbox convenience: keep the Sleeping Giant Inn First Canticle whisper in Sandbox mode
        if (ep.unlockMode == UnlockMode::Sandbox && ep.modulePresenceProgression &&
            !_presenceSig.Presence().canticleAwakened && isMutual && _focusDwellSec >= 4.5f &&
            IsSleepingGiantInn(player))
        {
            if (_presenceSig.TriggerCanticleAwakening(gameDays))
            {
                _ledger.RecordNotableEvent(focusFormId, NotableEventType::CanticleAwakening,
                                           gameDays, locId, 0u);
                if (ep.enableImpressionWhispers)
                {
                    GazeReticle::Get().PushWhisper(
                        "Your gaze stills—the First Canticle awakens (+5.0 Presence).", 0xFFD966u);
                }
                Engine::TraceLogger::Get().LogExpandedPlayEvent(
                    _sessionElapsedSec, "CANTICLE_AWAKEN", focusFormId, actorName, regionId,
                    _focusDwellSec, "+5.0Presence");
                logger::info("[TrueGaze::ExpandedPlay] First Canticle Awakened with '{}' ({:08X}).",
                             actorName, focusFormId);
            }
        }

        RE::NiPoint3 pPos = player->GetPosition();
        RE::NiPoint3 tPos = target->GetPosition();
        float rawDistUnits = pPos.GetDistance(tPos);
        if (rawDistUnits < 10.0f)
        {
            // Passengers sharing a vehicle/furniture root (e.g., Helgen opening carriage)
            // report identical GetPosition() coordinates; resolve using camera/face anchors.
            TrueGaze::Anchors::CameraBasis cam{};
            if (TrueGaze::Anchors::ResolveCameraBasis(cam))
            {
                pPos = cam.pos;
                tPos = RE::NiPoint3{gaze.faceX, gaze.faceY, gaze.faceZ};
                rawDistUnits = pPos.GetDistance(tPos);
            }
        }
        const float distanceMeters = rawDistUnits / kUnitsPerMeter;

        // --- WP-10: Smart Brandishing, Combat Sight & Creature Sight ----------------
        const CreatureArchetype creatureType = InspectActorCreatureArchetype(target);
        const ActorHeldPosture targetPosture = InspectActorHeldPosture(target);
        auto* targetState = target->AsActorState();
        const bool targetInCombat = target->IsInCombat();
        const bool targetHostile = target->IsHostileToActor(player);

        // 1. Creature Sight (Prey Gentle Sight, Predator Kyne's Stillness, Dragon Dov's Weight)
        if (creatureType != CreatureArchetype::Humanoid && ep.moduleSocialReactions)
        {
            const bool alreadyAwarded =
                std::find(_awardedCreatureMilestones.begin(), _awardedCreatureMilestones.end(),
                          focusFormId) != _awardedCreatureMilestones.end();
            CreatureSightInput cin{};
            cin.stage = liveCaps.stage;
            cin.archetype = creatureType;
            cin.playerLethalBrandish = playerWeaponDrawn;
            cin.activeDemeanor = activeDemeanor;
            cin.regionId = regionId;
            cin.dwellSec = _focusDwellSec;
            cin.distanceMeters = distanceMeters;
            cin.alreadyAwardedMilestone = alreadyAwarded;

            const auto cres = EvaluateCreatureSight(cin, actorName);
            if (cres.triggered &&
                _creatureSightCooldown.TryConsume(focusFormId, _sessionElapsedSec, 5.0f))
            {
                if (cres.buffs.HasAny())
                {
                    ExpandedPlayModifiers::Get().SetTransientTacticalBonuses(cres.buffs);
                    _tacticalBuffActive = true;
                    _tacticalBuffExpiresSessionSec = _sessionElapsedSec + cres.buffs.durationSec;
                }
                if (cres.markMilestone && !alreadyAwarded)
                {
                    _awardedCreatureMilestones.push_back(focusFormId);
                    if (ep.modulePresenceProgression && cres.presenceAward > 0.0f)
                    {
                        _presenceSig.AwardMaxPresence(cres.presenceAward, gameDays, false);
                        _stateDirty = true;
                    }
                    GazeReticle::Get().NotifyMilestone(cres.whisper, cres.colorRgb,
                                                       "UISkillsGlowSD", true);
                }
                else if (ep.enableImpressionWhispers && !cres.whisper.empty())
                {
                    GazeReticle::Get().PushWhisper(cres.whisper, cres.colorRgb);
                }
                const std::string cDetail = std::format(
                    "archetype={} dist={:.1f}m +presence={:.2f}",
                    static_cast<unsigned>(creatureType), distanceMeters, cres.presenceAward);
                Engine::TraceLogger::Get().LogExpandedPlayEvent(
                    _sessionElapsedSec, "CREATURE_SIGHT", focusFormId, actorName, regionId,
                    _focusDwellSec, cDetail.c_str());
                logger::info("[TrueGaze::ExpandedPlay] Creature Sight '{}' ({:08X}): {}", actorName,
                             focusFormId, cDetail);
            }
            if (!targetInCombat)
            {
                SyncPersistentStateToSerializer();
                return;
            }
        }

        // 2. Combat Sight (Weapon Wind-Up, Spell Charge, Shield Rhythm, Footwork) + Pre-Combat
        // Brandish
        if (ep.moduleSocialReactions &&
            (targetInCombat || (!targetInCombat && targetPosture.IsLethalBrandish())))
        {
            CombatSightInput ksin{};
            ksin.stage = liveCaps.stage;
            ksin.targetInCombat = targetInCombat;
            ksin.targetHostile = targetHostile || targetInCombat;
            ksin.regionId = regionId;
            ksin.dwellSec = _focusDwellSec;
            ksin.distanceMeters = distanceMeters;
            ksin.targetPosture = targetPosture;
            if (targetState)
            {
                const auto atk = targetState->GetAttackState();
                ksin.targetAttacking =
                    (atk == RE::ATTACK_STATE_ENUM::kDraw || atk == RE::ATTACK_STATE_ENUM::kSwing ||
                     atk == RE::ATTACK_STATE_ENUM::kBowDraw ||
                     atk == RE::ATTACK_STATE_ENUM::kBowDrawn ||
                     atk == RE::ATTACK_STATE_ENUM::kBowReleasing);
            }
            ksin.targetCasting = target->IsCasting(nullptr);
            ksin.targetBlocking = target->IsBlocking();
            RE::NiPoint3 vel{};
            target->GetLinearVelocity(vel);
            ksin.targetSpeedUnitsPerSec = std::sqrt(vel.x * vel.x + vel.y * vel.y);
            ksin.playerPresenceMax = _presenceSig.Presence().maximum;

            const auto csRes = EvaluateCombatSight(ksin, actorName);
            if (csRes.triggered &&
                _combatSightCooldown.TryConsume(focusFormId, _sessionElapsedSec, 4.0f))
            {
                if (csRes.buffs.HasAny())
                {
                    ExpandedPlayModifiers::Get().SetTransientTacticalBonuses(csRes.buffs);
                    _tacticalBuffActive = true;
                    _tacticalBuffExpiresSessionSec = _sessionElapsedSec + csRes.buffs.durationSec;
                }
                if (ep.enableImpressionWhispers && !csRes.whisper.empty())
                {
                    GazeReticle::Get().PushWhisper(csRes.whisper, csRes.colorRgb);
                    if (targetInCombat)
                    {
                        GazeReticle::Get().TriggerAwakeningPulse(csRes.colorRgb);
                    }
                }
                const std::string csDetail =
                    std::format("kind={} blk={:.0f} ward={:.0f} off={:.0f} arm={:.0f} dist={:.1f}m",
                                static_cast<unsigned>(csRes.kind), csRes.buffs.blockBonus,
                                csRes.buffs.resistMagicBonus, csRes.buffs.offenseBonus,
                                csRes.buffs.armorBonus, distanceMeters);
                Engine::TraceLogger::Get().LogExpandedPlayEvent(_sessionElapsedSec, "COMBAT_SIGHT",
                                                                focusFormId, actorName, regionId,
                                                                _focusDwellSec, csDetail.c_str());
                logger::info("[TrueGaze::ExpandedPlay] Combat Sight '{}' ({:08X}): {}", actorName,
                             focusFormId, csDetail);
            }

            // While fighting, keep Combat Sight crisp and never spam social persuasion/whispers
            if (targetInCombat)
            {
                SyncPersistentStateToSerializer();
                return;
            }
        }

        PerceptionInput pin{};
        pin.observerAlive = !target->IsDead();
        pin.observerConscious = targetState ? !targetState->IsUnconscious() : true;
        pin.observerSleeping =
            targetState &&
            (targetState->GetSitSleepState() == RE::SIT_SLEEP_STATE::kIsSleeping ||
             targetState->GetSitSleepState() == RE::SIT_SLEEP_STATE::kWaitingForSleepAnim);
        pin.observerRagdolled = target->IsInRagdollState();
        pin.observerInCombat = target->IsInCombat();
        pin.observerInScene = (target->GetCurrentScene() != nullptr);
        pin.distanceMeters =
            (liveCaps.allowKindredSight && ep.enableKindredSight && targetCarriesRelic)
                ? (distanceMeters / 1.35f)
                : distanceMeters;
        bool losArg = false;
        pin.hasLineOfSight = target->HasLineOfSight(player, losArg);
        const float obsYaw = target->GetAngleZ();
        pin.observerForwardX = std::sin(obsYaw);
        pin.observerForwardY = std::cos(obsYaw);
        pin.observerForwardZ = 0.0f;
        pin.dirToPlayerX = pPos.x - tPos.x;
        pin.dirToPlayerY = pPos.y - tPos.y;
        pin.dirToPlayerZ = pPos.z - tPos.z;
        pin.lightLevel01 = 1.0f;
        const auto culture = MapObserverCulture(target);
        pin.observerHasNightEye =
            (culture == ObserverCulture::Khajiit || culture == ObserverCulture::Dunmer);
        pin.playerFullFaceHelmet = fullHelm;
        pin.playerHooded = hooded;

        const auto perception = PerceptionGate::Evaluate(pin);
        if (!perception.perceived)
        {
            static uint32_t s_lastBlockedId = 0u;
            static PerceptionReason s_lastBlockedReason = PerceptionReason::Perceived;
            if (s_lastBlockedId != focusFormId || s_lastBlockedReason != perception.reason)
            {
                s_lastBlockedId = focusFormId;
                s_lastBlockedReason = perception.reason;
                logger::info("[TrueGaze::ExpandedPlay] Perception gated '{}' ({:08X}): reason={} "
                             "dist={:.1f}m los={} scene={}",
                             actorName, focusFormId, PerceptionReasonName(perception.reason),
                             distanceMeters, pin.hasLineOfSight ? "yes" : "no",
                             pin.observerInScene ? "yes" : "no");
            }
            _lastInterpretation = Interpretation::None;
            _lastStrength = 0.0f;
            SyncPersistentStateToSerializer();
            return;
        }

        InterpretationContext ictx{};
        ictx.isChild = target->IsChild();
        ictx.matureReactionsEnabled = ep.moduleMatureReactions;
        ictx.tier = ctx.targetRelationship;
        ictx.culture = culture;
        ictx.isMutual = isMutual;
        ictx.firstMutualWithHistory = isMutual && existingEntry && existingEntry->recognized;
        const float hpMax = target->GetActorValueMax(RE::ActorValue::kHealth);
        auto* targetAv = target->AsActorValueOwner();
        const float hpNow = targetAv ? targetAv->GetActorValue(RE::ActorValue::kHealth) : hpMax;
        ictx.compassionContext = (hpMax > 1.0f && (hpNow / hpMax) < 0.75f);
        ictx.playerWeaponDrawn = playerWeaponDrawn;
        ictx.regionSpecific = perception.regionSpecific;
        ictx.perceptionIntensity = perception.intensity;
        ictx.facialBondScale = perception.facialBondScale;
        ictx.demeanorEfficacy = demeanorProfile.efficacyMultiplier;

        _focusRapidEscalation = (regionId == Region::Torso) && ep.moduleMatureReactions &&
                                !ictx.isChild && !ictx.compassionContext && !ictx.emblemContext;

        const auto interpResult =
            InterpretationEngine::EvaluateLook(regionId, _focusDwellSec, ictx);
        _lastInterpretation = interpResult.interpretation;
        _lastStrength = interpResult.strength;

        if (interpResult.band != _lastLoggedDwellBand)
        {
            _lastLoggedDwellBand = interpResult.band;
            const std::string detail = std::format(
                "band={} interp={} strength={:.2f} mutual={} dist={:.1f}m",
                DwellBandName(interpResult.band), InterpretationName(interpResult.interpretation),
                interpResult.strength, isMutual ? "yes" : "no", distanceMeters);
            Engine::TraceLogger::Get().LogExpandedPlayEvent(_sessionElapsedSec, "DWELL_BAND",
                                                            focusFormId, actorName, regionId,
                                                            _focusDwellSec, detail.c_str());
        }

        if (interpResult.allowSocialBump && ep.moduleRelationshipMemory)
        {
            _ledgerCommitAccumSec += kFocusPeriodSec;
            if (_ledgerCommitAccumSec >= 1.0f)
            {
                _ledgerCommitAccumSec = 0.0f;
                _stateDirty = true;
                _ledger.RecordInterpretation(focusFormId, interpResult.interpretation,
                                             interpResult.strength, gameDays, locId, isMutual);
                if (ep.modulePresenceProgression)
                {
                    if (interpResult.interpretation == Interpretation::IntrusionLeer &&
                        interpResult.strength >= 0.4f)
                    {
                        _presenceSig.RecordLeerCaught(gameDays);
                    }
                    else
                    {
                        _presenceSig.ApplyInterpretationHabit(interpResult.interpretation,
                                                              interpResult.strength);
                    }
                }
            }
        }

        // --- Oblivion-Inspired Real-Time Persuasion Wheel & Multi-Transaction Engine ---
        _handEyeTracker.Observe(focusFormId, regionId, playerWeaponDrawn, _sessionElapsedSec);
        const bool slyBribeCadence =
            _handEyeTracker.IsSlyBribeCadenceActive(focusFormId, _sessionElapsedSec);
        const bool standDownGrace =
            _handEyeTracker.IsStandDownGraceActive(focusFormId, _sessionElapsedSec);

        const auto* targetBaseObj = target->GetActorBase();
        const uint32_t targetBaseLow24 =
            targetBaseObj ? (static_cast<uint32_t>(targetBaseObj->GetFormID()) & 0x00FFFFFFu) : 0u;
        const auto affinityProfile = ResolveNpcAffinityProfile(
            targetBaseLow24, culture, ctx.targetRelationship, target->IsGuard(), ictx.isChild);
        const PersuasionVector activeVec = ClassifyPersuasionVector(
            regionId, activeDemeanor, playerWeaponDrawn, ictx.isChild, ictx.playerIsThane,
            _presenceSig.CurrentTier(), ictx.compassionContext, ictx.emblemContext, slyBribeCadence,
            standDownGrace);

        _lastPersuasionVector = activeVec;
        _lastAffinityTier = affinityProfile.ForVector(activeVec);
        _slyBribeArmedNow = slyBribeCadence;

        // WP-11C: Evaluate Faction Sight (strengths, authentic drawbacks & civilian masking)
        FactionSightOutcome factionOut{};
        if (ep.moduleFactionSight && playerFactionBits != FactionBits::kNone &&
            liveCaps.expandedActive)
        {
            FactionSightInput fin{};
            fin.playerFactions = playerFactionBits;
            fin.observerFactions = targetFactionBits;
            fin.stage = liveCaps.stage;
            fin.wearingDarkBrotherhoodGear = pDbGear;
            fin.wearingThievesGuildGear = pTgGear;
            fin.wearingCollegeRobes = pCollegeRobes;
            fin.wearingImperialArmor = pImpArmor;
            fin.wearingStormcloakArmor = pScArmor;
            fin.wearingDawnguardOrVigilantGear = pHunterGear;
            fin.observerIsGuard = target->IsGuard();
            fin.observerIsMerchant = targetIsMerchant;
            fin.observerIsPriestOrVigilant = targetIsPriestOrVigilant;
            fin.observerIsUndeadOrVampire = targetIsUndead;
            fin.observerIsNordTraditionalist = (culture == ObserverCulture::Nord);
            fin.observerIsChild = ictx.isChild;
            fin.inJarlCourtOrShop = ctx.inJarlCourtOrTemple || targetIsMerchant;
            fin.isDaylight = isDaylight;
            fin.playerSneaking = playerSneaking;
            fin.inCombat = playerInCombat;
            fin.regionId = regionId;
            fin.activeDemeanor = activeDemeanor;
            fin.dwellSec = _focusDwellSec;
            fin.distanceMeters = distanceMeters;
            fin.playerDominanceAxis = _presenceSig.Signature().dominance;

            factionOut = EvaluateFactionSight(fin, actorName);
            _activeGuardGlanceRangeMeters = factionOut.guardGlanceRangeMeters;

            if (factionOut.tacticalBuffs.HasAny())
            {
                _tacticalBuffExpiresSessionSec = _sessionElapsedSec + 4.0f;
                _tacticalBuffActive = true;
                ExpandedPlayModifiers::Get().SetTransientTacticalBonuses(factionOut.tacticalBuffs);
            }

            if ((factionOut.strengthTriggered || factionOut.drawbackTriggered) &&
                _factionSightCooldown.TryConsume(focusFormId, _sessionElapsedSec, 14.0f))
            {
                if (ep.moduleRelationshipMemory)
                {
                    _ledger.ApplyImpressionDelta(focusFormId, factionOut.socialDelta, gameDays,
                                                 locId, isMutual);
                    _stateDirty = true;
                }
                if (ep.enableImpressionWhispers && !factionOut.whisper.empty())
                {
                    GazeReticle::Get().PushWhisper(factionOut.whisper, factionOut.whisperColorRgb);
                }
                Engine::TraceLogger::Get().LogExpandedPlayEvent(
                    _sessionElapsedSec,
                    factionOut.drawbackTriggered ? "FACTION_SIGHT_DRAWBACK" : "FACTION_SIGHT",
                    focusFormId, actorName, regionId, _focusDwellSec, factionOut.whisper.c_str());
            }
        }
        else
        {
            _activeGuardGlanceRangeMeters = 12.0f;
        }

        // Deferred voice bark request (dispatched cleanly after unlocking g_diagMutex)
        uint32_t pendingVoiceActorId = 0u;
        VoiceEventKind pendingVoiceKind = VoiceEventKind::None;
        if (factionOut.suggestVoiceScrutiny)
        {
            pendingVoiceActorId = focusFormId;
            pendingVoiceKind = VoiceEventKind::FactionScrutiny;
        }
        else if (interpResult.interpretation == Interpretation::IntrusionLeer &&
                 interpResult.strength >= 0.45f)
        {
            pendingVoiceActorId = focusFormId;
            pendingVoiceKind = VoiceEventKind::IntrusionStare;
        }

        // Evaluate Phase-1 Probe BEFORE Rung-2 ResponseLadder so only ONE authoritative
        // FaceGen expression writer runs per tick (eliminating 10 Hz expression fighting).
        ProbeTellResult probe{};
        if (ep.moduleSocialReactions && liveCaps.allowPersuasionProbes &&
            activeVec != PersuasionVector::None)
        {
            probe = EvaluateProbeTell(activeVec, affinityProfile, _focusDwellSec,
                                      _activeLawWitnessCount, ictx.isChild);
        }
        _persuasionProbeActive = probe.active;
        _persuasionGuardGlanceActive = probe.active && probe.guardGlanceInterrupt;

        const Impressions imp =
            _ledger.Find(focusFormId) ? _ledger.Find(focusFormId)->impressions : Impressions{};
        const bool canBroadcast = ep.moduleSocialReactions &&
                                  interpResult.band >= DwellBand::Gaze &&
                                  interpResult.strength >= 0.35f &&
                                  _barkCooldown.TryConsume(focusFormId, _sessionElapsedSec);

        ResponseDecision decision = EvaluateResponseLadder(
            interpResult, activeDemeanor, _presenceSig.CurrentTier(), imp, ep.moduleSocialReactions,
            ep.enableImpressionWhispers, ep.emitAiNpcBridgeEvents, canBroadcast);

        // Suppress Rung-2 FaceGen when a Phase-1 probe owns the face or beyond 6 m conversational
        // range
        constexpr float kMaxFaceExpressionRangeMeters = 6.0f;
        if (probe.active || distanceMeters > kMaxFaceExpressionRangeMeters)
        {
            decision.rung2.applyExpression = false;
        }
        if (probe.active && probe.applyModeOverride)
        {
            decision.rung1.applyModeOverride = false;
        }

        ResponseLadder::ApplyDecision(focusFormId, actorName, regionId, _focusDwellSec,
                                      interpResult, decision, _sessionElapsedSec);

        // Phase 1 Pre-Commit Tell (0.35s..1.99s): telegraph Love/Like/Dislike/Hate & Guard's Glance
        if (probe.active)
        {
            if (probe.applyModeOverride)
            {
                if (auto* rt = Engine::GazeEngine::Get().FindActor(focusFormId))
                {
                    rt->hcepMode = probe.hcepMode;
                    rt->hasModeOverride = true;
                    rt->modeOverrideTimerSec = 1.2f;
                    if (probe.lockMutualReciprocity)
                    {
                        rt->crosshairHoldTimerSec = std::max(rt->crosshairHoldTimerSec, 0.35f);
                    }
                    if (probe.forceAversion)
                    {
                        rt->cgaActive = true;
                    }
                }
            }
            if (distanceMeters <= kMaxFaceExpressionRangeMeters && probe.morphWeight > 0.005f)
            {
                // WP-11A: Slightly stronger, clearly readable Phase-1 micro-expressions (up to
                // 0.62)
                float boostedWeight = probe.morphWeight;
                if (probe.affinity == AffinityTier::Love)
                {
                    boostedWeight = std::max(boostedWeight, 0.58f);
                }
                else if (probe.affinity == AffinityTier::Hate || probe.guardGlanceInterrupt)
                {
                    boostedWeight = std::max(boostedWeight, 0.56f);
                }
                ResponseLadder::ApplyTransientExpression(
                    focusFormId, probe.morphMood, std::clamp(boostedWeight, 0.0f, 0.62f), 1.5f,
                    _sessionElapsedSec);
            }
            if (!_probeTellSent)
            {
                // WP-11A: Physical NPC micro-gesture & Guard's Glance eye-dart
                if (auto* rt = Engine::GazeEngine::Get().FindActor(focusFormId))
                {
                    if (probe.guardGlanceInterrupt)
                    {
                        rt->persuasionYawBiasDeg = (_activeLawWitnessDirSign < 0) ? -24.0f : 24.0f;
                        rt->persuasionPitchBiasDeg = 0.0f;
                        rt->persuasionGestureDurationSec = 0.65f;
                        rt->persuasionGestureTimerSec = 0.65f;
                    }
                    else if (probe.affinity == AffinityTier::Love ||
                             probe.affinity == AffinityTier::Like)
                    {
                        rt->persuasionYawBiasDeg = 0.0f;
                        rt->persuasionPitchBiasDeg =
                            (probe.affinity == AffinityTier::Love) ? -6.5f : -4.0f;
                        rt->persuasionGestureDurationSec = 0.55f;
                        rt->persuasionGestureTimerSec = 0.55f;
                    }
                    else
                    {
                        rt->persuasionYawBiasDeg =
                            (probe.affinity == AffinityTier::Hate) ? 12.0f : 7.0f;
                        rt->persuasionPitchBiasDeg = 2.0f;
                        rt->persuasionGestureDurationSec = 0.70f;
                        rt->persuasionGestureTimerSec = 0.70f;
                    }
                }

                if (probe.guardGlanceInterrupt)
                {
                    pendingVoiceActorId =
                        (_activeLawWitnessActorId != 0u) ? _activeLawWitnessActorId : focusFormId;
                    pendingVoiceKind = VoiceEventKind::GuardGlance;
                }
                else if (probe.affinity == AffinityTier::Hate)
                {
                    pendingVoiceActorId = focusFormId;
                    pendingVoiceKind = VoiceEventKind::IntrusionStare;
                }

                const std::string detail = std::format(
                    "vec={} aff={} guardGlance={} mood={} weight={:.2f} dist={:.1f}m",
                    PersuasionVectorName(probe.vector), AffinityTierName(probe.affinity),
                    probe.guardGlanceInterrupt ? "yes" : "no",
                    static_cast<unsigned>(probe.morphMood), probe.morphWeight, distanceMeters);
                Engine::TraceLogger::Get().LogExpandedPlayEvent(_sessionElapsedSec, "PROBE_TELL",
                                                                focusFormId, actorName, regionId,
                                                                _focusDwellSec, detail.c_str());
                logger::info("[TrueGaze::ExpandedPlay] Probe '{}' ({:08X}): {}", actorName,
                             focusFormId, detail);
                if (probe.suggestWhisper && ep.enableImpressionWhispers)
                {
                    const auto msg = FormatProbeWhisperText(actorName, probe);
                    if (!msg.empty())
                    {
                        GazeReticle::Get().PushWhisper(msg, probe.whisperColorRgb);
                    }
                }
                _probeTellSent = true;
            }
        }

        // Phase 2/3 Transaction Valuation Commit (>= 2.00s Gaze+) — unlocked at Stage 3 (Living
        // Glyph)
        if (ep.moduleSocialReactions && liveCaps.allowTransactions &&
            activeVec != PersuasionVector::None && _focusDwellSec >= 2.00f && !_txCommittedForHold)
        {
            auto* playerAv = player->AsActorValueOwner();
            const float playerSpeech =
                playerAv ? playerAv->GetActorValue(RE::ActorValue::kSpeech) : 15.0f;
            const float npcSpeech =
                targetAv ? targetAv->GetActorValue(RE::ActorValue::kSpeech) : 20.0f;

            TransactionInput txIn{};
            txIn.vector = activeVec;
            txIn.affinity = _lastAffinityTier;
            txIn.dwellSec = _focusDwellSec;
            txIn.standDownGrace = standDownGrace;
            txIn.isChild = ictx.isChild;
            txIn.isMutual = isMutual;
            txIn.playerSpeechcraft = playerSpeech;
            txIn.npcSpeechcraft = npcSpeech;
            txIn.playerLevel = static_cast<int32_t>(player->GetLevel());
            txIn.npcLevel = static_cast<int32_t>(target->GetLevel());
            txIn.presenceMax = _presenceSig.Presence().maximum;
            txIn.scholarshipRank =
                ClassifyScholarshipRank(savedSnapshot.sign.booksFullyStudiedCount);
            txIn.relTier = ctx.targetRelationship;
            txIn.currentImpressions =
                _ledger.Find(focusFormId) ? &_ledger.Find(focusFormId)->impressions : nullptr;
            txIn.neutralWitnessCount = _activeNeutralWitnessCount;
            txIn.lawWitnessCount = _activeLawWitnessCount;
            txIn.noveltyMultiplier =
                _txSaturation.NoveltyMultiplier(focusFormId, activeVec, _sessionElapsedSec);

            auto txOut = EvaluateTransactionCommit(txIn);
            // Apply WP-11C Faction Sight speech & bribe multipliers
            if (ep.moduleFactionSight)
            {
                if (txOut.computedBribeCostGold > 0)
                {
                    txOut.computedBribeCostGold =
                        std::max<int32_t>(10, static_cast<int32_t>(std::lround(
                                                  static_cast<float>(txOut.computedBribeCostGold) *
                                                  factionOut.bribeCostMultiplier)));
                }
                txOut.speechBarterDelta *= factionOut.barterSpeechMultiplier;
                if (activeVec == PersuasionVector::Coerce)
                {
                    txOut.speechBarterDelta *= factionOut.coerceAuthorityMultiplier;
                }
            }
            if (txOut.computedBribeCostGold > 0)
            {
                _lastBribeCostGold = txOut.computedBribeCostGold;
            }
            if (txOut.committed)
            {
                _txCommittedForHold = true;
                _txSaturation.RecordCommit(focusFormId, activeVec, _sessionElapsedSec);

                if (ep.moduleRelationshipMemory)
                {
                    _ledger.ApplyImpressionDelta(focusFormId, txOut.ledgerDelta, gameDays, locId,
                                                 isMutual);
                }

                if (ep.modulePresenceProgression && txOut.presenceDelta > 0.001f)
                {
                    _presenceSig.AwardMaxPresence(txOut.presenceDelta, gameDays, false);
                }

                _rapportTargetFormId = focusFormId;
                _rapportExpiresSessionSec = _sessionElapsedSec + 45.0f;
                ExpandedPlayModifiers::Get().SetTransientRapportSpeechBonus(
                    txOut.speechBarterDelta);
                _stateDirty = true;

                // WP-11A Persuasion Commit Bloom & Sound + WP-11B Voice Response selection
                if (txOut.type == TransactionType::SlyBribePrime)
                {
                    if (ep.enablePersuasionVisuals)
                    {
                        GazeReticle::Get().TriggerPersuasionBloom(0xFFD966u, "ITMGoldUpSD");
                    }
                    pendingVoiceActorId = focusFormId;
                    pendingVoiceKind = VoiceEventKind::WarmCommit;
                }
                else if (txOut.type == TransactionType::StandDownGrace)
                {
                    if (ep.enablePersuasionVisuals)
                    {
                        GazeReticle::Get().TriggerPersuasionBloom(0xE6D8B8u, "UIMenuOKSD");
                    }
                    pendingVoiceActorId = focusFormId;
                    pendingVoiceKind = VoiceEventKind::StandDownGrace;
                }
                else if (activeVec == PersuasionVector::Coerce)
                {
                    if (ep.enablePersuasionVisuals)
                    {
                        GazeReticle::Get().TriggerPersuasionBloom(0xFF9966u, "UIMenuFocus");
                    }
                    pendingVoiceActorId = focusFormId;
                    pendingVoiceKind = VoiceEventKind::ArmedCoerce;
                }
                else
                {
                    if (ep.enablePersuasionVisuals)
                    {
                        GazeReticle::Get().TriggerPersuasionBloom(0x5CE68Au, "UISkillsGlowSD");
                    }
                    pendingVoiceActorId = focusFormId;
                    pendingVoiceKind = VoiceEventKind::WarmCommit;
                }

                const std::string txDetail =
                    std::format("type={} vec={} aff={} bribe={}g speech={:+.1f} +pres={:.2f} "
                                "lawWit={} neuWit={}",
                                TransactionTypeName(txOut.type), PersuasionVectorName(activeVec),
                                AffinityTierName(_lastAffinityTier), txOut.computedBribeCostGold,
                                txOut.speechBarterDelta, txOut.presenceDelta,
                                _activeLawWitnessCount, _activeNeutralWitnessCount);
                Engine::TraceLogger::Get().LogExpandedPlayEvent(_sessionElapsedSec, "TX_COMMIT",
                                                                focusFormId, actorName, regionId,
                                                                _focusDwellSec, txDetail.c_str());
                logger::info("[TrueGaze::ExpandedPlay] Tx commit '{}' ({:08X}): {}", actorName,
                             focusFormId, txDetail);

                if (ep.enableImpressionWhispers && txOut.type == TransactionType::SlyBribePrime)
                {
                    GazeReticle::Get().PushWhisper(
                        std::format("{} catches your look (~{}g quiet price).", actorName,
                                    txOut.computedBribeCostGold),
                        0xFFD966u);
                }
                else if (ep.enableImpressionWhispers &&
                         txOut.type == TransactionType::StandDownGrace)
                {
                    GazeReticle::Get().PushWhisper(
                        std::format("{} acknowledges your sheathed steel.", actorName), 0xE6D8B8u);
                }
            }
            else if (txOut.blockedByWitness)
            {
                const std::string blkDetail =
                    std::format("vec={} lawWitnesses={}", PersuasionVectorName(activeVec),
                                _activeLawWitnessCount);
                Engine::TraceLogger::Get().LogExpandedPlayEvent(_sessionElapsedSec, "TX_BLOCKED",
                                                                focusFormId, actorName, regionId,
                                                                _focusDwellSec, blkDetail.c_str());
                logger::info("[TrueGaze::ExpandedPlay] Tx blocked '{}' ({:08X}): {}", actorName,
                             focusFormId, blkDetail);
                if (ep.enablePersuasionVisuals)
                {
                    GazeReticle::Get().TriggerPersuasionBloom(0xFF4D4Du, "UIMenuCancelSD");
                }
                pendingVoiceActorId =
                    (_activeLawWitnessActorId != 0u) ? _activeLawWitnessActorId : focusFormId;
                pendingVoiceKind = VoiceEventKind::GuardGlance;
                if (ep.enableImpressionWhispers && !_probeTellSent)
                {
                    GazeReticle::Get().PushWhisper(
                        std::format("{}'s eyes flick toward the guard—too exposed!", actorName),
                        0xFF6666u);
                    _probeTellSent = true;
                }
            }
        }

        EyeContactGreetingInput greet{};
        greet.enabled = ep.enabled && ep.moduleSocialReactions && liveCaps.allowEyeContactGreeting;
        greet.featureToggle = ep.enableEyeContactInitiate;
        greet.presenceTier = _presenceSig.CurrentTier();
        greet.inTutorialInn = IsSleepingGiantInn(player);
        greet.isMutual = isMutual;
        greet.interpretation = interpResult.interpretation;
        greet.dwellSec = _focusDwellSec;
        greet.distanceMeters = distanceMeters;
        greet.inCombat = playerInCombat || target->IsInCombat();
        greet.inDialogue = inDialogue;
        greet.inScene = (target->GetCurrentScene() != nullptr);
        greet.targetIsChild = ictx.isChild;
        greet.playerWeaponDrawn = playerWeaponDrawn;
        greet.cooldownReady = _greetingCooldown.IsReady(focusFormId, _sessionElapsedSec, 30.0f);

        if (ShouldInitiateEyeContactGreeting(greet) &&
            _greetingCooldown.TryConsume(focusFormId, _sessionElapsedSec, 30.0f))
        {
            const bool activated = ResponseLadder::TryInitiateEyeContactGreeting(
                focusFormId, actorName, ep.enableImpressionWhispers);
            const std::string gDetail =
                std::format("activated={} interp={} dist={:.1f}m", activated ? "yes" : "no",
                            InterpretationName(interpResult.interpretation), distanceMeters);
            Engine::TraceLogger::Get().LogExpandedPlayEvent(_sessionElapsedSec, "GREETING_TRIGGER",
                                                            focusFormId, actorName, regionId,
                                                            _focusDwellSec, gDetail.c_str());
            logger::info("[TrueGaze::ExpandedPlay] Eye-contact greeting '{}' ({:08X}): {}",
                         actorName, focusFormId, gDetail);
        }

        SyncPersistentStateToSerializer();
        const float voiceNowSec = _sessionElapsedSec;
        lock.unlock();

        // WP-11B: Restrained vanilla voiced NPC reaction (dispatched outside g_diagMutex)
        if (pendingVoiceActorId != 0u && pendingVoiceKind != VoiceEventKind::None)
        {
            NpcVoiceResponder::Get().TrySpeak(pendingVoiceActorId, pendingVoiceKind, voiceNowSec,
                                              inDialogue, playerSneaking,
                                              ep.enableNpcVoiceResponses);
        }
#else
        (void)gameDays;
#endif
    }

    void ExpandedPlayManager::EvaluateWitnesses2Hz(float gameDays) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        const auto ep = Engine::ConfigManager::GetSingleton().SnapshotExpandedPlay();
        if (!ep.moduleSocialReactions)
        {
            return;
        }

        Interpretation activeInterp = Interpretation::None;
        float activeStrength = 0.0f;
        uint32_t targetId = 0u;
        {
            std::lock_guard lock(g_diagMutex);
            activeInterp = _lastInterpretation;
            activeStrength = _lastStrength;
            targetId = _focusActorFormId;
        }

        if (targetId == 0u)
        {
            std::lock_guard lock(g_diagMutex);
            _activeNeutralWitnessCount = 0u;
            _activeLawWitnessCount = 0u;
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* target = RE::TESForm::LookupByID<RE::Actor>(static_cast<RE::FormID>(targetId));
        auto* processLists = RE::ProcessLists::GetSingleton();
        if (!player || !target || !processLists)
        {
            return;
        }

        const auto* targetBase = target->GetActorBase();
        const uint32_t targetBaseLow =
            targetBase ? (static_cast<uint32_t>(targetBase->GetFormID()) & 0x00FFFFFFu) : 0u;
        const bool targetIsCamilla = (targetBaseLow == kBaseCamillaValerius);
        const auto* loc = player->GetCurrentLocation();
        const uint32_t locId = loc ? static_cast<uint32_t>(loc->GetFormID()) : 0u;
        const RE::NiPoint3 pPos = player->GetPosition();

        int evaluated = 0;
        constexpr int kMaxWitnessChecks = 6;
        uint32_t neutralWitnesses = 0u;
        uint32_t lawWitnesses = 0u;
        uint32_t nearestLawWitnessId = 0u;
        int8_t nearestLawWitnessDirSign = 0;
        float nearestLawWitnessDist = 999999.0f;
        uint32_t kindredAllies = 0u;
        uint32_t flankingHostiles = 0u;
        RE::Actor* firstKindredAlly = nullptr;

        StageCapabilities witnessCaps{};
        float guardRangeMeters = 12.0f;
        {
            std::lock_guard lock(g_diagMutex);
            witnessCaps = ResolveStageCapabilities(_awakening, ep.unlockMode);
            guardRangeMeters = std::clamp(_activeGuardGlanceRangeMeters, 12.0f, 20.0f);
        }

        const bool playerInCombatNow = player->IsInCombat();
        const bool trackSocialTriangle =
            !playerInCombatNow && witnessCaps.expandedActive && ep.moduleRelationshipMemory &&
            activeStrength > 0.25f &&
            (activeInterp == Interpretation::Warmth || activeInterp == Interpretation::Attraction ||
             activeInterp == Interpretation::IntrusionLeer);

        for (auto& handle : processLists->highActorHandles)
        {
            if (evaluated >= kMaxWitnessChecks)
            {
                break;
            }
            auto actorPtr = handle.get();
            auto* other = actorPtr.get();
            if (!other || other == player || other == target || other->IsDead() || other->IsChild())
            {
                continue;
            }
                const float distUnits = pPos.GetDistance(other->GetPosition());
            if (distUnits > guardRangeMeters * kUnitsPerMeter)
            {
                continue;
            }
            ++evaluated;

            const uint32_t otherId = static_cast<uint32_t>(other->GetFormID());
            if (ep.enableKindredSight && AwakeningRitual::Get().DoesActorCarrySightRelic(otherId))
            {
                ++kindredAllies;
                if (!firstKindredAlly)
                {
                    firstKindredAlly = other;
                }
            }

            if (other->IsInCombat())
            {
                if (playerInCombatNow && other->IsHostileToActor(player))
                {
                    ++flankingHostiles;
                }
                continue;
            }

            bool losPlayer = false;
            bool losTarget = false;
            if (!other->HasLineOfSight(player, losPlayer) ||
                !other->HasLineOfSight(target, losTarget))
            {
                continue;
            }

            if (other->IsGuard())
            {
                ++lawWitnesses;
                if (distUnits < nearestLawWitnessDist)
                {
                    nearestLawWitnessDist = distUnits;
                    nearestLawWitnessId = otherId;
                    const RE::NiPoint3 gPos = other->GetPosition();
                    const float pYaw = player->GetAngleZ();
                    const float rightX = std::cos(pYaw);
                    const float rightY = -std::sin(pYaw);
                    const float sideDot = (gPos.x - pPos.x) * rightX + (gPos.y - pPos.y) * rightY;
                    nearestLawWitnessDirSign = (sideDot < 0.0f) ? -1 : 1;
                }
            }
                else if (distUnits <= 12.0f * kUnitsPerMeter)
                {
                    ++neutralWitnesses;
                }

            if (!trackSocialTriangle)
            {
                continue;
            }

            const auto* otherBase = other->GetActorBase();
            const uint32_t otherBaseLow =
                otherBase ? (static_cast<uint32_t>(otherBase->GetFormID()) & 0x00FFFFFFu) : 0u;

            WitnessRole role = WitnessRole::Bystander;
            if (IsPlayerSpouse(other))
            {
                role = WitnessRole::Spouse;
            }
            else if (targetIsCamilla && (otherBaseLow == kBaseSven || otherBaseLow == kBaseFaendal))
            {
                role = WitnessRole::LoveTriangleRival;
            }

            bool triggered = false;
            bool whisperNow = false;
            {
                std::lock_guard lock(g_diagMutex);
                triggered = _ledger.RecordWitnessedInteraction(
                    otherId, role, targetId, activeInterp, activeStrength, gameDays, locId);
                if (triggered)
                {
                    whisperNow = ep.enableImpressionWhispers &&
                                 _witnessCooldown.TryConsume(otherId, _sessionElapsedSec, 25.0f);
                }
            }

            if (whisperNow)
            {
                const char* otherName = SafeActorName(other);
                const std::string wDetail =
                    std::format("role={} target=0x{:08X} interp={}", static_cast<unsigned>(role),
                                targetId, InterpretationName(activeInterp));
                Engine::TraceLogger::Get().LogExpandedPlayEvent(_sessionElapsedSec, "WITNESS_EVENT",
                                                                otherId, otherName, 0u, 0.0f,
                                                                wDetail.c_str());
                logger::info("[TrueGaze::ExpandedPlay] Witness '{}' ({:08X}): {}", otherName,
                             otherId, wDetail);
                if (role == WitnessRole::Spouse)
                {
                    GazeReticle::Get().PushWhisper(
                        std::format("{} saw you lingering too warmly.", otherName), 0xFF6666u);
                }
                else if (role == WitnessRole::LoveTriangleRival)
                {
                    GazeReticle::Get().PushWhisper(
                        std::format("{} watched your exchange with a tightening jaw.", otherName),
                        0xFFCC66u);
                }
                else
                {
                    GazeReticle::Get().PushWhisper(std::format("{} noticed your stare.", otherName),
                                                   0xFF9966u);
                }
            }
        }

        // Kindred Sight (Stage 3+): a follower/ally bearing a Sight Relic warns you when a
        // hold guard watches your back during social play, OR when a hostile flanks you in combat.
        if (witnessCaps.allowKindredSight && ep.enableKindredSight && firstKindredAlly &&
            (lawWitnesses > 0u || (playerInCombatNow && flankingHostiles > 0u)))
        {
            const uint32_t allyId = static_cast<uint32_t>(firstKindredAlly->GetFormID());
            const bool isCombatFlank = playerInCombatNow && flankingHostiles > 0u;
            bool emitKindred = false;
            {
                std::lock_guard lock(g_diagMutex);
                if (_kindredCooldown.TryConsume(allyId, _sessionElapsedSec,
                                                isCombatFlank ? 12.0f : 25.0f))
                {
                    ++_awakening.followerRelicWitnessCount;
                    _stateDirty = true;
                    emitKindred = true;
                }
            }
            if (emitKindred)
            {
                const char* allyName = SafeActorName(firstKindredAlly);
                Engine::TraceLogger::Get().LogExpandedPlayEvent(
                    _sessionElapsedSec, "KINDRED_SIGHT", allyId, allyName, 0u, 0.0f,
                    isCombatFlank ? "second_pair_of_eyes:combat_flank"
                                  : "second_pair_of_eyes:guard_watch");
                if (isCombatFlank)
                {
                    GazeReticle::Get().NotifyMilestone(
                        std::format("{}'s glance warns you - hostile closing on your flank!",
                                    allyName),
                        0xFF9926u, "UIMenuFocus", true);
                }
                else
                {
                    GazeReticle::Get().NotifyMilestone(
                        std::format(
                            "{}'s quiet glance warns you - a guard's eyes are on your back.",
                            allyName),
                        0xA6D8FFu, "UIMenuFocus", true);
                }
            }
        }

        {
            std::lock_guard lock(g_diagMutex);
            _activeNeutralWitnessCount = neutralWitnesses;
            _activeLawWitnessCount = lawWitnesses;
            _activeLawWitnessActorId = nearestLawWitnessId;
            _activeLawWitnessDirSign = nearestLawWitnessDirSign;
            _activeKindredAllyCount = kindredAllies;
        }
#else
        (void)gameDays;
#endif
    }

    void ExpandedPlayManager::SyncPersistentStateToSerializer() noexcept
    {
        auto state = CoSaveSerializer::Get().SnapshotState();
        const auto ep = Engine::ConfigManager::GetSingleton().SnapshotExpandedPlay();
        const auto caps = ResolveStageCapabilities(_awakening, ep.unlockMode);
        _presenceSig.SaveInto(state.sign);
        if (ep.unlockMode == UnlockMode::Ritual &&
            state.sign.presence.maximum > caps.maxPresenceCap)
        {
            state.sign.presence.maximum = caps.maxPresenceCap;
            state.sign.presence.current =
                std::min(state.sign.presence.current, state.sign.presence.maximum);
        }
        state.sign.activeDemeanor = _demeanorCtrl.ActiveDemeanor();
        state.sign.demeanorMode = ep.demeanorMode;
        state.ledger = _ledger.ExportVector();
        state.awakening = _awakening;
        CoSaveSerializer::Get().SetState(std::move(state));
    }

    AwakeningState ExpandedPlayManager::CurrentAwakening() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        return _awakening;
    }

    AwakeningStage ExpandedPlayManager::CurrentAwakeningStage() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        const auto ep = Engine::ConfigManager::GetSingleton().SnapshotExpandedPlay();
        return ComputeAwakeningStage(_awakening, ep.unlockMode);
    }

    StageCapabilities ExpandedPlayManager::CurrentCapabilities() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        const auto ep = Engine::ConfigManager::GetSingleton().SnapshotExpandedPlay();
        return ResolveStageCapabilities(_awakening, ep.unlockMode);
    }

    uint32_t ExpandedPlayManager::ActiveKindredAllyCount() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        return _activeKindredAllyCount;
    }

    Demeanor ExpandedPlayManager::ActiveDemeanor() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        return _demeanorCtrl.ActiveDemeanor();
    }

    bool ExpandedPlayManager::IsDemeanorPinned() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        return _demeanorCtrl.IsPinned();
    }

    float ExpandedPlayManager::DemeanorPinRemainingSec() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        return _demeanorCtrl.PinRemainingSec();
    }

    ContestStatus ExpandedPlayManager::ContestState() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        return _contest.Status();
    }

    bool ExpandedPlayManager::IsContestActive() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        return _contest.IsActive();
    }

    uint8_t ExpandedPlayManager::ContestRound() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        return _contest.CurrentRound();
    }

    float ExpandedPlayManager::ContestElapsedSec() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        return _contest.ElapsedSec();
    }

    float ExpandedPlayManager::ContestBlinkPressure01() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        return _contest.BlinkPressure01();
    }

    size_t ExpandedPlayManager::LedgerEntryCount() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        return _ledger.PersistentCount();
    }

    PresenceState ExpandedPlayManager::CurrentPresence() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        return _presenceSig.Presence();
    }

    SignatureAxes ExpandedPlayManager::CurrentSignature() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        return _presenceSig.Signature();
    }

    SignatureTitle ExpandedPlayManager::CurrentSignatureTitle() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        return _presenceSig.CurrentTitle();
    }

    Interpretation ExpandedPlayManager::LastInterpretation() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        return _lastInterpretation;
    }

    PersuasionVector ExpandedPlayManager::LastPersuasionVector() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        return _lastPersuasionVector;
    }

    AffinityTier ExpandedPlayManager::LastAffinityTier() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        return _lastAffinityTier;
    }

    int32_t ExpandedPlayManager::LastBribeCostGold() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        return _lastBribeCostGold;
    }

    float ExpandedPlayManager::ActiveRapportSpeechBonus() const noexcept
    {
        return ExpandedPlayModifiers::Get().TransientRapportSpeechBonus();
    }

    uint32_t ExpandedPlayManager::ActiveLawWitnessCount() const noexcept
    {
        std::lock_guard lock(g_diagMutex);
        return _activeLawWitnessCount;
    }
} // namespace TrueGaze::ExpandedPlay
