#include "ExpandedPlay/AwakeningRitual.hpp"

#include "Engine/ConfigManager.hpp"
#include "Engine/GazeAnchors.hpp"
#include "Engine/TraceLogger.hpp"
#include "ExpandedPlay/CoSaveSerializer.hpp"
#include "ExpandedPlay/ExpandedPlayModifiers.hpp"
#include "ExpandedPlay/GazeReticle.hpp"
#include "PCH.h"

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#endif

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>

namespace TrueGaze::ExpandedPlay
{
    namespace
    {
        constexpr const char* kOptionalEslPluginName = "TrueGaze_ExpandedPlay.esp";

        // Local FormIDs inside TrueGaze_ExpandedPlay.esp (0x801..0x808 ESL compact range)
        constexpr uint32_t kEslLocalPowerCanticle = 0x000802u;
        constexpr uint32_t kEslLocalBookCanticle = 0x000803u;
        constexpr uint32_t kEslLocalAmuletMoth = 0x000804u;
        constexpr uint32_t kEslLocalBrowbandMoth = 0x000805u;
        constexpr uint32_t kEslLocalRingQuietSight = 0x000806u;
        constexpr uint32_t kEslLocalSilverMoth = 0x000807u;
        constexpr uint32_t kEslLocalMothRibbon = 0x000808u;
        constexpr uint32_t kEslLocalCassianNote = 0x000809u;
        constexpr uint32_t kEslLocalNoteTopic = 0x00080Au;
        constexpr uint32_t kEslLocalNoteInfo = 0x00080Bu;
        constexpr uint32_t kEslLocalPilgrimageQuest = 0x00080Cu;
        constexpr uint32_t kEslLocalRumorTopic = 0x00080Eu;
        constexpr uint32_t kEslLocalRumorInfo = 0x00080Fu;

        // Verified base Skyrim.esm vanilla fallback FormIDs (zero DLC dependency)
        constexpr uint32_t kVanillaBookEffectsOfScrolls = 0x0003010Bu;
        constexpr uint32_t kVanillaAmuletJulianos = 0x000C8917u;
        constexpr uint32_t kVanillaAmuletKynareth = 0x000C8919u;
        constexpr uint32_t kVanillaCirclet01 = 0x000166D6u;
        constexpr uint32_t kVanillaRingSilver = 0x0003B97Cu;

        std::atomic<bool> g_eslLoaded{false};
        std::atomic<uint32_t> g_eslFormPower{0};
        std::atomic<uint32_t> g_eslFormBook{0};
        std::atomic<uint32_t> g_eslFormAmulet{0};
        std::atomic<uint32_t> g_eslFormBrowband{0};
        std::atomic<uint32_t> g_eslFormRing{0};
        std::atomic<uint32_t> g_eslFormMedallion{0};
        std::atomic<uint32_t> g_eslFormRibbon{0};
        std::atomic<uint32_t> g_eslFormCassianNote{0};
        std::atomic<uint32_t> g_eslFormNoteTopic{0};
        std::atomic<uint32_t> g_eslFormNoteInfo{0};
        std::atomic<uint32_t> g_eslFormPilgrimageQuest{0};
        std::atomic<uint32_t> g_eslFormRumorTopic{0};
        std::atomic<uint32_t> g_eslFormRumorInfo{0};
        std::atomic<bool> g_questStartLogged{false};
        std::atomic<uint32_t> g_lastDispatchedQuestStage{0};
        std::atomic<uint32_t> g_dispatchedObjDisplayedMask{0};
        std::atomic<uint32_t> g_dispatchedObjCompletedMask{0};
        std::atomic<uint32_t> g_placedBarNoteRefId{0};
        std::atomic<bool> g_cassianNoteOpenedFlag{false};

        std::atomic<bool> g_ceremonyCastPending{false};
        std::atomic<bool> g_diegeticTogglePending{false};
        std::atomic<bool> g_diegeticToggleDesired{true};
        std::atomic<bool> g_canticleSpellCheckedLearned{false};

#if __has_include(<RE/Skyrim.h>)
        constexpr const char* kOpenBookNifPath = "meshes\\animobjects\\animobjectbook.nif";
        constexpr const char* kLeftHandBookNodeName = "TrueGaze_LeftHandBook";

        std::mutex g_equipMutex;
        std::chrono::steady_clock::time_point g_lastRelicToggleAt{};
        bool g_sinksInstalled{false};
        // CRIT-08: Immortal heap holder so DLL_PROCESS_DETACH never invokes
        // NiRefObject::DecRefCount() after Skyrim's heap allocator has torn down.
        RE::NiPointer<RE::NiNode>& g_openBookTemplate = *(new RE::NiPointer<RE::NiNode>{});
        bool g_openBookDemandAttempted{false};
        bool g_leftBookAttached1P{false};
        bool g_leftBookAttached3P{false};

        [[nodiscard]] RE::NiMatrix3 MakeEulerMatrix(float rxDeg, float ryDeg, float rzDeg) noexcept
        {
            constexpr float kDegToRad = 0.017453292519943295f;
            const float cx = std::cos(rxDeg * kDegToRad);
            const float sx = std::sin(rxDeg * kDegToRad);
            const float cy = std::cos(ryDeg * kDegToRad);
            const float sy = std::sin(ryDeg * kDegToRad);
            const float cz = std::cos(rzDeg * kDegToRad);
            const float sz = std::sin(rzDeg * kDegToRad);

            RE::NiMatrix3 m{};
            m.entry[0][0] = cy * cz;
            m.entry[0][1] = cz * sx * sy - cx * sz;
            m.entry[0][2] = cx * cz * sy + sx * sz;
            m.entry[1][0] = cy * sz;
            m.entry[1][1] = cx * cz + sx * sy * sz;
            m.entry[1][2] = cx * sy * sz - cz * sx;
            m.entry[2][0] = -sy;
            m.entry[2][1] = cy * sx;
            m.entry[2][2] = cx * cy;
            return m;
        }

        [[nodiscard]] RE::NiNode* FindLeftHandAnchor(RE::NiAVObject* rootObj,
                                                     bool& outUsedAnimObject) noexcept
        {
            outUsedAnimObject = false;
            if (!rootObj)
            {
                return nullptr;
            }
            if (auto* ao = rootObj->GetObjectByName(RE::BSFixedString("AnimObjectL")))
            {
                if (auto* node = ao->AsNode())
                {
                    outUsedAnimObject = true;
                    return node;
                }
            }
            if (auto* hand = rootObj->GetObjectByName(RE::BSFixedString("NPC L Hand [LHnd]")))
            {
                if (auto* node = hand->AsNode())
                {
                    return node;
                }
            }
            return nullptr;
        }

        void DetachBookFromSkeleton(RE::NiAVObject* rootObj) noexcept
        {
            if (!rootObj)
            {
                return;
            }
            auto* existing = rootObj->GetObjectByName(RE::BSFixedString(kLeftHandBookNodeName));
            if (existing && existing->parent)
            {
                auto* parent = existing->parent;
                parent->DetachChild(existing);
                RE::NiUpdateData upd{};
                parent->Update(upd);
            }
        }

        bool EnsureBookAttachedToSkeleton(RE::NiAVObject* rootObj, bool isFirstPerson) noexcept
        {
            if (!rootObj)
            {
                return false;
            }
            if (rootObj->GetObjectByName(RE::BSFixedString(kLeftHandBookNodeName)) != nullptr)
            {
                return true;
            }

            if (!g_openBookTemplate && !g_openBookDemandAttempted)
            {
                g_openBookDemandAttempted = true;
                RE::BSModelDB::DBTraits::ArgsType args{};
                const auto err = RE::BSModelDB::Demand(kOpenBookNifPath, g_openBookTemplate, args);
                if (err != RE::BSResource::ErrorCode::kNone || !g_openBookTemplate)
                {
                    logger::warn("[TrueGaze::ExpandedPlay] Could not load '{}' (err={}); "
                                 "left-hand open book visual unavailable.",
                                 kOpenBookNifPath, static_cast<int>(err));
                    return false;
                }
                logger::info("[TrueGaze::ExpandedPlay] Loaded vanilla open book mesh '{}' for "
                             "left-hand ritual casting.",
                             kOpenBookNifPath);
            }
            if (!g_openBookTemplate)
            {
                return false;
            }

            bool usedAnimObject = false;
            RE::NiNode* anchor = FindLeftHandAnchor(rootObj, usedAnimObject);
            if (!anchor)
            {
                return false;
            }

            auto* clonedObj = g_openBookTemplate->Clone();
            auto* clonedNode = clonedObj ? clonedObj->AsNode() : nullptr;
            if (!clonedNode)
            {
                return false;
            }

            // Flip BookAnimObject 180 deg around its own local Y axis (the exact book spine:
            // diag(-1, +1, -1), proper right-handed rotation det = +1). In animobjectbook.nif,
            // local X=[-17..18] is page width, Y=[-11.6..11.3] is the spine, and +Z=[-2..+7.8]
            // is the open-pages normal (which natively maps to -Y in AnimObjectL, away from the
            // player). Negating columns 0 and 2 flips the open parchment pages 180 deg directly
            // toward the player's eyes while keeping the spine 100% upright!
            if (auto* bookMeshObj =
                    clonedNode->GetObjectByName(RE::BSFixedString("BookAnimObject")))
            {
                auto& r = bookMeshObj->local.rotate;
                for (int row = 0; row < 3; ++row)
                {
                    r.entry[row][0] = -r.entry[row][0];
                    r.entry[row][2] = -r.entry[row][2];
                }
            }

            auto* wrapper = RE::NiNode::Create(1);
            if (!wrapper)
            {
                return false;
            }
            wrapper->name = RE::BSFixedString(kLeftHandBookNodeName);
            wrapper->AttachChild(clonedNode, true);

            // Hover the open book cleanly above the left hand and slightly outward (~3-4 inches)
            // away from the player's body so it never clips the fist or torso while keeping the
            // open parchment pages facing the player in both 1st person and 3rd person.
            // AnimObjectL axes: +X = wrist->fingertips, +Y = up (pinky->thumb / open-pages normal),
            // +Z = inward (back-of-hand->palm toward body), so +Y lifts above the hand and -Z
            // shifts outward away from the body (and slightly forward in 1st person).
            if (usedAnimObject)
            {
                wrapper->local.scale = isFirstPerson ? 0.62f : 0.85f;
                wrapper->local.translate = isFirstPerson ? RE::NiPoint3{-1.5f, 8.0f, -5.0f}
                                                         : RE::NiPoint3{1.0f, 9.5f, -6.5f};
                wrapper->local.rotate = isFirstPerson ? MakeEulerMatrix(8.0f, 0.0f, -8.0f)
                                                      : MakeEulerMatrix(0.0f, 0.0f, 0.0f);
            }
            else
            {
                wrapper->local.scale = isFirstPerson ? 0.62f : 0.85f;
                wrapper->local.translate = isFirstPerson ? RE::NiPoint3{-5.0f, 4.5f, 6.0f}
                                                         : RE::NiPoint3{-6.0f, 5.5f, 6.0f};
                wrapper->local.rotate = MakeEulerMatrix(10.0f, 0.0f, 90.0f);
            }

            anchor->AttachChild(wrapper, true);
            RE::NiUpdateData upd{};
            anchor->Update(upd);
            logger::info("[TrueGaze::ExpandedPlay] Attached open Canticle book to {} ({}, "
                         "spine-flipped pages-facing)",
                         isFirstPerson ? "1st-person" : "3rd-person",
                         usedAnimObject ? "AnimObjectL" : "NPC L Hand [LHnd]");
            return true;
        }

        [[nodiscard]] RelicKind InspectFormRelicKind(const RE::TESForm* form) noexcept
        {
            if (!form)
            {
                return RelicKind::None;
            }
            // Fast exact FormID match when optional ESL TrueGaze_ExpandedPlay.esp is loaded
            const uint32_t fid = static_cast<uint32_t>(form->GetFormID());
            if (fid != 0u)
            {
                if (fid == g_eslFormBook.load(std::memory_order_relaxed))
                    return RelicKind::CanticleBook;
                if (fid == g_eslFormAmulet.load(std::memory_order_relaxed) ||
                    fid == g_eslFormBrowband.load(std::memory_order_relaxed))
                    return RelicKind::PlayerToggleRelic;
                if (fid == g_eslFormRing.load(std::memory_order_relaxed) ||
                    fid == g_eslFormMedallion.load(std::memory_order_relaxed) ||
                    fid == g_eslFormRibbon.load(std::memory_order_relaxed))
                    return RelicKind::GiftableSightRelic;
            }
            const char* name = form->GetName();
            if (!name || name[0] == '\0')
            {
                return RelicKind::None;
            }
            const auto ft = form->GetFormType();
            const bool isBook = (ft == RE::FormType::Book);
            const bool isArmor = (ft == RE::FormType::Armor);
            const bool isMisc = (ft == RE::FormType::Misc);
            return ClassifyRelicByName(name, isBook, isArmor, isMisc);
        }

        [[nodiscard]] bool IsCanticlePowerSpell(const RE::SpellItem* spell) noexcept
        {
            if (!spell)
            {
                return false;
            }
            const uint32_t fid = static_cast<uint32_t>(spell->GetFormID());
            if (fid != 0u && fid == g_eslFormPower.load(std::memory_order_relaxed))
            {
                return true;
            }
            const char* name = spell->GetName();
            if (!name || name[0] == '\0')
            {
                return false;
            }
            return AwakeningDetail::ContainsICase(name, "Sight of the First Canticle") ||
                   AwakeningDetail::ContainsICase(name, "TrueGaze");
        }

        class RelicEquipSink final : public RE::BSTEventSink<RE::TESEquipEvent>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event,
                                                  RE::BSTEventSource<RE::TESEquipEvent>*) override
            {
                if (!a_event || !a_event->actor || a_event->baseObject == 0u)
                {
                    return RE::BSEventNotifyControl::kContinue;
                }
                auto* actor =
                    a_event->actor.get() ? a_event->actor.get()->As<RE::Actor>() : nullptr;
                if (!actor || !actor->IsPlayerRef())
                {
                    return RE::BSEventNotifyControl::kContinue;
                }

                auto* baseForm = RE::TESForm::LookupByID(a_event->baseObject);
                const RelicKind kind = InspectFormRelicKind(baseForm);
                if (kind != RelicKind::PlayerToggleRelic && kind != RelicKind::GiftableSightRelic &&
                    kind != RelicKind::VanillaFocus)
                {
                    return RE::BSEventNotifyControl::kContinue;
                }

                auto& cfg = Engine::ConfigManager::GetSingleton();
                // Bespoke ESL toggle relic (Amulet of the Ancestor Moth / Moth-Silk Browband)
                // can wake Expanded Play diegetically even if bEnabled was left false; vanilla
                // items strictly respect bEnabled=false so visual-only players are untouched.
                if (kind == RelicKind::PlayerToggleRelic)
                {
                    std::lock_guard lock(g_equipMutex);
                    const auto now = std::chrono::steady_clock::now();
                    const float sinceLastSec =
                        std::chrono::duration<float>(now - g_lastRelicToggleAt).count();
                    if (g_lastRelicToggleAt.time_since_epoch().count() != 0 && sinceLastSec < 0.75f)
                    {
                        return RE::BSEventNotifyControl::kContinue;
                    }
                    g_lastRelicToggleAt = now;

                    if (a_event->equipped && !cfg.expandedPlayEnabled)
                    {
                        // CRIT-08: Marshal ConfigManager mutation onto the game thread
                        // instead of writing non-atomic bool from Skyrim's event thread.
                        if (auto* tasks = SKSE::GetTaskInterface())
                        {
                            const std::string relicName =
                                (baseForm && baseForm->GetName()) ? baseForm->GetName() : "Relic";
                            tasks->AddTask(
                                [relicName]()
                                {
                                    auto& mainCfg = Engine::ConfigManager::GetSingleton();
                                    if (!mainCfg.expandedPlayEnabled)
                                    {
                                        mainCfg.expandedPlayEnabled = true;
                                        logger::info("[TrueGaze::ExpandedPlay] Bespoke relic '{}' awakened "
                                                     "Expanded Play diegetically.",
                                                     relicName);
                                    }
                                });
                        }
                    }

                    // Equipping toggles ON; unequipping after Stage 4 lets the player quiet
                    // Expanded Play diegetically without touching the console.
                    g_diegeticToggleDesired.store(a_event->equipped, std::memory_order_release);
                    g_diegeticTogglePending.store(true, std::memory_order_release);
                }

                return RE::BSEventNotifyControl::kContinue;
            }
        };

        class CanticleSpellCastSink final : public RE::BSTEventSink<RE::TESSpellCastEvent>
        {
        public:
            RE::BSEventNotifyControl
            ProcessEvent(const RE::TESSpellCastEvent* a_event,
                         RE::BSTEventSource<RE::TESSpellCastEvent>*) override
            {
                if (!a_event || !a_event->object || a_event->spell == 0u)
                {
                    return RE::BSEventNotifyControl::kContinue;
                }
                auto* caster =
                    a_event->object.get() ? a_event->object.get()->As<RE::Actor>() : nullptr;
                if (!caster || !caster->IsPlayerRef())
                {
                    return RE::BSEventNotifyControl::kContinue;
                }
                auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(a_event->spell);
                if (!IsCanticlePowerSpell(spell))
                {
                    return RE::BSEventNotifyControl::kContinue;
                }

                const auto& cfg = Engine::ConfigManager::GetSingleton();
                if (!cfg.expandedPlayEnabled)
                {
                    // CRIT-08: Marshal ConfigManager mutation onto the game thread.
                    if (auto* tasks = SKSE::GetTaskInterface())
                    {
                        tasks->AddTask(
                            []()
                            {
                                Engine::ConfigManager::GetSingleton().expandedPlayEnabled = true;
                            });
                    }
                }
                // All Cast events (missing requirements check, Final Ceremony, or post-Stage-4
                // diegetic toggle) are handled cleanly on the game tick inside ExpandedPlayManager.
                g_ceremonyCastPending.store(true, std::memory_order_release);

                logger::info("[TrueGaze::ExpandedPlay] Spell '{}' cast by player.",
                             spell->GetName() ? spell->GetName() : "Sight of the First Canticle");
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        RelicEquipSink g_equipSink;
        CanticleSpellCastSink g_spellSink;
#endif
    } // namespace

    AwakeningRitual& AwakeningRitual::Get() noexcept
    {
        static AwakeningRitual instance;
        return instance;
    }

    void AwakeningRitual::Install() noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (auto* data = RE::TESDataHandler::GetSingleton())
        {
            const bool eslFound = (data->LookupModByName(kOptionalEslPluginName) != nullptr);
            g_eslLoaded.store(eslFound, std::memory_order_release);
            if (eslFound)
            {
                g_eslFormPower.store(static_cast<uint32_t>(data->LookupFormID(
                                         kEslLocalPowerCanticle, kOptionalEslPluginName)),
                                     std::memory_order_relaxed);
                g_eslFormBook.store(static_cast<uint32_t>(data->LookupFormID(
                                        kEslLocalBookCanticle, kOptionalEslPluginName)),
                                    std::memory_order_relaxed);
                g_eslFormAmulet.store(static_cast<uint32_t>(data->LookupFormID(
                                          kEslLocalAmuletMoth, kOptionalEslPluginName)),
                                      std::memory_order_relaxed);
                g_eslFormBrowband.store(static_cast<uint32_t>(data->LookupFormID(
                                            kEslLocalBrowbandMoth, kOptionalEslPluginName)),
                                        std::memory_order_relaxed);
                g_eslFormRing.store(static_cast<uint32_t>(data->LookupFormID(
                                        kEslLocalRingQuietSight, kOptionalEslPluginName)),
                                    std::memory_order_relaxed);
                g_eslFormMedallion.store(static_cast<uint32_t>(data->LookupFormID(
                                             kEslLocalSilverMoth, kOptionalEslPluginName)),
                                         std::memory_order_relaxed);
                g_eslFormRibbon.store(static_cast<uint32_t>(data->LookupFormID(
                                          kEslLocalMothRibbon, kOptionalEslPluginName)),
                                      std::memory_order_relaxed);
                g_eslFormCassianNote.store(static_cast<uint32_t>(data->LookupFormID(
                                               kEslLocalCassianNote, kOptionalEslPluginName)),
                                           std::memory_order_relaxed);
                g_eslFormNoteTopic.store(static_cast<uint32_t>(data->LookupFormID(
                                             kEslLocalNoteTopic, kOptionalEslPluginName)),
                                         std::memory_order_relaxed);
                g_eslFormNoteInfo.store(static_cast<uint32_t>(data->LookupFormID(
                                            kEslLocalNoteInfo, kOptionalEslPluginName)),
                                        std::memory_order_relaxed);
                g_eslFormPilgrimageQuest.store(
                    static_cast<uint32_t>(
                        data->LookupFormID(kEslLocalPilgrimageQuest, kOptionalEslPluginName)),
                    std::memory_order_relaxed);
                g_eslFormRumorTopic.store(static_cast<uint32_t>(data->LookupFormID(
                                              kEslLocalRumorTopic, kOptionalEslPluginName)),
                                          std::memory_order_relaxed);
                g_eslFormRumorInfo.store(static_cast<uint32_t>(data->LookupFormID(
                                             kEslLocalRumorInfo, kOptionalEslPluginName)),
                                         std::memory_order_relaxed);
                logger::info(
                    "[TrueGaze::ExpandedPlay] Optional ESL '{}' DETECTED (Power={:08X}, "
                    "Book={:08X}, Amulet={:08X}, Browband={:08X}, Ring={:08X}, Medallion={:08X}, "
                    "Ribbon={:08X}, Quest={:08X}, RumorTopic={:08X}, RumorInfo={:08X}, "
                    "NoteInfo={:08X}).",
                    kOptionalEslPluginName, g_eslFormPower.load(), g_eslFormBook.load(),
                    g_eslFormAmulet.load(), g_eslFormBrowband.load(), g_eslFormRing.load(),
                    g_eslFormMedallion.load(), g_eslFormRibbon.load(),
                    g_eslFormPilgrimageQuest.load(), g_eslFormRumorTopic.load(),
                    g_eslFormRumorInfo.load(), g_eslFormNoteInfo.load());
            }
            else
            {
                logger::info("[TrueGaze::ExpandedPlay] AwakeningRitual active (optional ESL '{}': "
                             "not installed - vanilla Skyrim.esm hybrid mode).",
                             kOptionalEslPluginName);
            }
        }

        if (!g_sinksInstalled)
        {
            if (auto* holder = RE::ScriptEventSourceHolder::GetSingleton())
            {
                holder->AddEventSink<RE::TESEquipEvent>(&g_equipSink);
                holder->AddEventSink<RE::TESSpellCastEvent>(&g_spellSink);
                g_sinksInstalled = true;
                logger::info("[TrueGaze::ExpandedPlay] Registered zero-script TESEquipEvent & "
                             "TESSpellCastEvent Awakening observers.");
            }
        }
#endif
    }

    void AwakeningRitual::Reset() noexcept
    {
        DetachLeftHandBookVisual();
        g_ceremonyCastPending.store(false, std::memory_order_release);
        g_diegeticTogglePending.store(false, std::memory_order_release);
        g_diegeticToggleDesired.store(true, std::memory_order_release);
        g_canticleSpellCheckedLearned.store(false, std::memory_order_release);
        g_questStartLogged.store(false, std::memory_order_release);
        g_lastDispatchedQuestStage.store(0u, std::memory_order_release);
        g_dispatchedObjDisplayedMask.store(0u, std::memory_order_release);
        g_dispatchedObjCompletedMask.store(0u, std::memory_order_release);
        g_placedBarNoteRefId.store(0u, std::memory_order_release);
        g_cassianNoteOpenedFlag.store(false, std::memory_order_release);
    }

    bool AwakeningRitual::IsEslPluginLoaded() const noexcept
    {
        return g_eslLoaded.load(std::memory_order_acquire);
    }

    bool AwakeningRitual::IsCassianNoteFormId(uint32_t formId,
                                              std::string_view title) const noexcept
    {
        const uint32_t noteId = g_eslFormCassianNote.load(std::memory_order_relaxed);
        if (noteId != 0u && formId == noteId)
        {
            return true;
        }
        return !title.empty() && AwakeningDetail::ContainsICase(title, "Moth Priest's Passage");
    }

    bool AwakeningRitual::IsPlayerWearingFocus() const noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
        {
            return false;
        }
        using Slot = RE::BGSBipedObjectForm::BipedObjectSlot;
        constexpr Slot kSlots[] = {Slot::kAmulet, Slot::kCirclet, Slot::kRing};
        for (const auto slot : kSlots)
        {
            if (auto* worn = player->GetWornArmor(slot, true))
            {
                const RelicKind kind = InspectFormRelicKind(worn);
                if (kind == RelicKind::PlayerToggleRelic || kind == RelicKind::GiftableSightRelic ||
                    kind == RelicKind::VanillaFocus)
                {
                    return true;
                }
            }
        }
#endif
        return false;
    }

    bool AwakeningRitual::IsPlayerCarryingStudiedCanticle() const noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
        {
            return false;
        }
        const auto saved = CoSaveSerializer::Get().SnapshotState();
        const auto inv = player->GetInventory();
        for (const auto& [obj, pair] : inv)
        {
            if (!obj || pair.first <= 0 || obj->GetFormType() != RE::FormType::Book)
            {
                continue;
            }
            if (InspectFormRelicKind(obj) != RelicKind::CanticleBook)
            {
                continue;
            }
            const uint32_t bookId = static_cast<uint32_t>(obj->GetFormID());
            for (const auto& entry : saved.books)
            {
                if (entry.bookFormId == bookId && entry.fullStudyAwarded)
                {
                    return true;
                }
            }
        }
#endif
        return false;
    }

    uint8_t AwakeningRitual::CurrentPlayerSanctuaryBit() const noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
        {
            return SanctuaryBits::kNone;
        }
        if (auto* loc = player->GetCurrentLocation())
        {
            if (const char* name = loc->GetName(); name && name[0] != '\0')
            {
                if (const uint8_t bit = MatchSanctuaryBit(name); bit != SanctuaryBits::kNone)
                {
                    return bit;
                }
            }
        }
        if (auto* cell = player->GetParentCell())
        {
            if (const char* cname = cell->GetName(); cname && cname[0] != '\0')
            {
                return MatchSanctuaryBit(cname);
            }
        }
#endif
        return SanctuaryBits::kNone;
    }

    std::string AwakeningRitual::CurrentPlayerSanctuaryName() const
    {
#if __has_include(<RE/Skyrim.h>)
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
        {
            return {};
        }
        if (auto* loc = player->GetCurrentLocation())
        {
            if (const char* name = loc->GetName(); name && name[0] != '\0')
            {
                if (MatchSanctuaryBit(name) != SanctuaryBits::kNone)
                {
                    return std::string(name);
                }
            }
        }
        if (auto* cell = player->GetParentCell())
        {
            if (const char* cname = cell->GetName(); cname && cname[0] != '\0')
            {
                if (MatchSanctuaryBit(cname) != SanctuaryBits::kNone)
                {
                    return std::string(cname);
                }
            }
        }
#endif
        return {};
    }

    void AwakeningRitual::DetachLeftHandBookVisual() noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (auto* player = RE::PlayerCharacter::GetSingleton())
        {
            DetachBookFromSkeleton(player->Get3D(true));
            DetachBookFromSkeleton(player->Get3D(false));
        }
        g_leftBookAttached1P = false;
        g_leftBookAttached3P = false;
#endif
    }

    void AwakeningRitual::UpdateLeftHandBookVisual(bool expandedEnabled) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !expandedEnabled)
        {
            if (g_leftBookAttached1P || g_leftBookAttached3P)
            {
                DetachLeftHandBookVisual();
            }
            return;
        }

        auto* actorState = player->AsActorState();
        const bool handsRaised = actorState && actorState->IsWeaponDrawn();

        auto* rightObj = player->GetEquippedObject(false);
        auto* rightSpell = rightObj ? rightObj->As<RE::SpellItem>() : nullptr;
        const bool canticleEquippedRight = IsCanticlePowerSpell(rightSpell);

        // Keep left hand open for the Canticle book unless the player explicitly equipped
        // another weapon/shield/spell in the left hand.
        auto* leftObj = player->GetEquippedObject(true);
        const bool leftHandFree = (leftObj == nullptr);

        const bool wantBook = handsRaised && canticleEquippedRight && leftHandFree;
        if (wantBook)
        {
            g_leftBookAttached1P = EnsureBookAttachedToSkeleton(player->Get3D(true), true);
            g_leftBookAttached3P = EnsureBookAttachedToSkeleton(player->Get3D(false), false);
        }
        else if (g_leftBookAttached1P || g_leftBookAttached3P)
        {
            DetachLeftHandBookVisual();
        }
#else
        (void)expandedEnabled;
#endif
    }

    bool AwakeningRitual::DoesActorCarrySightRelic(uint32_t actorFormId) const noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (actorFormId == 0u)
        {
            return false;
        }
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(static_cast<RE::FormID>(actorFormId));
        if (!actor || actor->IsPlayerRef() || actor->IsDead())
        {
            return false;
        }

        // Fast check 1: Worn Amulet / Circlet / Ring
        using Slot = RE::BGSBipedObjectForm::BipedObjectSlot;
        constexpr Slot kSlots[] = {Slot::kAmulet, Slot::kCirclet, Slot::kRing};
        const bool isFollower = actor->IsPlayerTeammate();
        for (const auto slot : kSlots)
        {
            if (auto* worn = actor->GetWornArmor(slot, true))
            {
                const RelicKind kind = InspectFormRelicKind(worn);
                if (kind == RelicKind::GiftableSightRelic || kind == RelicKind::PlayerToggleRelic)
                {
                    return true;
                }
                if (isFollower && kind == RelicKind::VanillaFocus)
                {
                    return true;
                }
            }
        }

        // Check 2: Carried pocket talismans (Moth-Silk Ribbon or gifted relic in inventory)
        const auto inv = actor->GetInventory();
        for (const auto& [obj, pair] : inv)
        {
            if (!obj || pair.first <= 0)
            {
                continue;
            }
            const RelicKind kind = InspectFormRelicKind(obj);
            if (kind == RelicKind::GiftableSightRelic || kind == RelicKind::PlayerToggleRelic)
            {
                return true;
            }
            if (isFollower && kind == RelicKind::VanillaFocus)
            {
                return true;
            }
        }
#else
        (void)actorFormId;
#endif
        return false;
    }

    bool AwakeningRitual::TryOfferNearbySightRelicToActor(uint32_t actorFormId) const noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (actorFormId == 0u)
        {
            return false;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(static_cast<RE::FormID>(actorFormId));
        auto* tes = RE::TES::GetSingleton();
        if (!player || !actor || !tes || actor->IsPlayerRef() || actor->IsDead() ||
            actor->IsChild() || actor->IsHostileToActor(player))
        {
            return false;
        }

        RE::TESObjectREFR* droppedRelic = nullptr;
        RE::TESBoundObject* relicBase = nullptr;
        tes->ForEachReferenceInRange(actor, 185.0f, // ~2.6 m around the NPC
                                     [&](RE::TESObjectREFR* ref) -> RE::BSContainer::ForEachResult
                                     {
                                         if (!ref || ref->IsDisabled() || ref->IsDeleted())
                                         {
                                             return RE::BSContainer::ForEachResult::kContinue;
                                         }
                                         auto* base = ref->GetBaseObject();
                                         if (!base)
                                         {
                                             return RE::BSContainer::ForEachResult::kContinue;
                                         }
                                         const RelicKind kind = InspectFormRelicKind(base);
                                         if (kind == RelicKind::GiftableSightRelic)
                                         {
                                             droppedRelic = ref;
                                             relicBase = base;
                                             return RE::BSContainer::ForEachResult::kStop;
                                         }
                                         return RE::BSContainer::ForEachResult::kContinue;
                                     });

        if (!droppedRelic || !relicBase)
        {
            return false;
        }

        actor->AddObjectToContainer(relicBase, nullptr, 1, nullptr);
        droppedRelic->Disable();
        droppedRelic->SetDelete(true);

        const char* actorName = actor->GetName() ? actor->GetName() : "Ally";
        const char* relicName = relicBase->GetName() ? relicBase->GetName() : "Sight Relic";
        logger::info("[TrueGaze::ExpandedPlay] Offered dropped Sight Relic '{}' to '{}' ({:08X}) "
                     "for Shared Sight.",
                     relicName, actorName, actorFormId);
        return true;
#else
        (void)actorFormId;
        return false;
#endif
    }

    bool AwakeningRitual::ConsumePendingCeremonyCast() noexcept
    {
        return g_ceremonyCastPending.exchange(false, std::memory_order_acq_rel);
    }

    bool AwakeningRitual::ConsumePendingDiegeticToggle(bool& outNewState) noexcept
    {
        if (!g_diegeticTogglePending.exchange(false, std::memory_order_acq_rel))
        {
            return false;
        }
        outNewState = g_diegeticToggleDesired.load(std::memory_order_acquire);
        return true;
    }

    bool AwakeningRitual::EnsureCanticleSpellLearned() noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (!IsEslPluginLoaded() || g_canticleSpellCheckedLearned.load(std::memory_order_acquire))
        {
            return false;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        const uint32_t spellId = g_eslFormPower.load(std::memory_order_relaxed);
        if (!player || spellId == 0u)
        {
            return false;
        }
        auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(static_cast<RE::FormID>(spellId));
        if (!spell)
        {
            return false;
        }
        if (player->HasSpell(spell))
        {
            g_canticleSpellCheckedLearned.store(true, std::memory_order_release);
            return false;
        }
        player->AddSpell(spell);
        g_canticleSpellCheckedLearned.store(true, std::memory_order_release);
        logger::info("[TrueGaze::ExpandedPlay] Automatically taught Right-Hand Spell '{}' ({:08X}) "
                     "to player.",
                     spell->GetName() ? spell->GetName() : "Sight of the First Canticle", spellId);
        return true;
#else
        return false;
#endif
    }

    uint32_t AwakeningRitual::GetInnRumorInfoFormId() const noexcept
    {
        return g_eslFormRumorInfo.load(std::memory_order_relaxed);
    }

    uint32_t AwakeningRitual::GetInnNoteInfoFormId() const noexcept
    {
        return g_eslFormNoteInfo.load(std::memory_order_relaxed);
    }

    void AwakeningRitual::EnsurePilgrimageQuestRunning() noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        const uint32_t questId = g_eslFormPilgrimageQuest.load(std::memory_order_relaxed);
        if (questId == 0u)
        {
            return;
        }
        auto* quest = RE::TESForm::LookupByID<RE::TESQuest>(static_cast<RE::FormID>(questId));
        if (!quest)
        {
            return;
        }
        if (!quest->IsRunning() && !quest->IsCompleted())
        {
            bool started = false;
            quest->EnsureQuestStarted(started, true);
            if (!quest->IsRunning())
            {
                quest->Start();
            }
            if (!quest->IsRunning())
            {
                if (auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton())
                {
                    if (auto* policy = vm->GetObjectHandlePolicy())
                    {
                        const auto handle =
                            policy->GetHandleForObject(RE::TESQuest::FORMTYPE, quest);
                        if (handle != policy->EmptyHandle())
                        {
                            auto* args = RE::MakeFunctionArguments();
                            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb;
                            vm->DispatchMethodCall(handle, RE::BSFixedString("Quest"),
                                                   RE::BSFixedString("Start"), args, cb);
                        }
                    }
                }
            }
        }
        if (!g_questStartLogged.exchange(true, std::memory_order_acq_rel))
        {
            const uint32_t topicId = g_eslFormRumorTopic.load(std::memory_order_relaxed);
            const uint32_t noteTopicId = g_eslFormNoteTopic.load(std::memory_order_relaxed);
            const uint32_t rumorInfoId = g_eslFormRumorInfo.load(std::memory_order_relaxed);
            const uint32_t noteInfoId = g_eslFormNoteInfo.load(std::memory_order_relaxed);
            auto* rumorTopic =
                (topicId != 0u)
                    ? RE::TESForm::LookupByID<RE::TESTopic>(static_cast<RE::FormID>(topicId))
                    : nullptr;
            auto* noteTopic =
                (noteTopicId != 0u)
                    ? RE::TESForm::LookupByID<RE::TESTopic>(static_cast<RE::FormID>(noteTopicId))
                    : nullptr;
            auto* rumorInfo = (rumorInfoId != 0u)
                                  ? RE::TESForm::LookupByID<RE::TESTopicInfo>(
                                        static_cast<RE::FormID>(rumorInfoId))
                                  : nullptr;
            auto* noteInfo =
                (noteInfoId != 0u)
                    ? RE::TESForm::LookupByID<RE::TESTopicInfo>(static_cast<RE::FormID>(noteInfoId))
                    : nullptr;
            const std::size_t branchCount = quest->branchedDialogue[0].size();
            logger::info(
                "[TrueGaze::ExpandedPlay] Pilgrimage Quest {:08X} ('{}'): running={} "
                "completed={} stage={} prio={} branches={} rumorTopic={:08X} rumorInfos={} "
                "noteTopic={:08X} noteInfos={} rumorInfoLoaded={} noteInfoLoaded={}",
                questId, quest->GetName() ? quest->GetName() : "TGZ_Pilgr",
                quest->IsRunning() ? "yes" : "no", quest->IsCompleted() ? "yes" : "no",
                quest->GetCurrentStageID(), static_cast<int>(quest->data.priority), branchCount,
                topicId, rumorTopic ? rumorTopic->numTopicInfos : 0u, noteTopicId,
                noteTopic ? noteTopic->numTopicInfos : 0u, rumorInfo ? "yes" : "no",
                noteInfo ? "yes" : "no");
        }
#endif
    }

    void AwakeningRitual::SyncPilgrimageQuestProgress(const AwakeningState& state,
                                                      bool startJournalIfEligible) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        const uint32_t questId = g_eslFormPilgrimageQuest.load(std::memory_order_relaxed);
        if (questId == 0u)
        {
            return;
        }
        auto* quest = RE::TESForm::LookupByID<RE::TESQuest>(static_cast<RE::FormID>(questId));
        if (!quest)
        {
            return;
        }

        EnsurePilgrimageQuestRunning();

        const bool shouldBeInJournal =
            startJournalIfEligible ||
            ((state.discoveryHintsMask & DiscoveryHintBits::kInnRumor) != 0u) ||
            (state.tumblerMask != 0u) || state.ceremonyCompleted;
        if (!shouldBeInJournal)
        {
            return;
        }

        // CRIT-07: SE/AE/VR-safe quest progression.
        // On SE/AE, resolve the native TESQuest helpers with address != 0 checks and
        // pass the real IVirtualMachine* (never nullptr) to SetObjectiveDisplayed /
        // SetObjectiveCompleted. On Skyrim VR (or if any relocation ID is absent from
        // VR Address Library), cleanly fall back to Skyrim's built-in Quest Papyrus
        // native interface via DispatchMethodCall(questHandle, "Quest", ...) so VR
        // players get full Pilgrimage journal progression with zero crashes.
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
        const auto questHandle =
            (vm && policy) ? policy->GetHandleForObject(RE::TESQuest::FORMTYPE, quest) : 0ULL;
        const bool hasQuestHandle = (vm && policy && questHandle != policy->EmptyHandle());

        auto tryResolveAddr = [](REL::RelocationID id) noexcept -> std::uintptr_t
        {
            try
            {
                return id.address();
            }
            catch (...)
            {
                return 0;
            }
        };

        using NativeSetStageFn = bool (*)(RE::TESQuest*, std::uint16_t);
        using NativeSetActiveFn = void (*)(RE::TESQuest*, bool);
        using NativeSetObjDisplayedFn = void (*)(RE::BSScript::IVirtualMachine*, std::uint32_t,
                                                 RE::TESQuest*, std::uint16_t, bool, bool);
        using NativeSetObjCompletedFn = void (*)(RE::BSScript::IVirtualMachine*, std::uint32_t,
                                                 RE::TESQuest*, std::uint16_t, bool);

        static const std::uintptr_t kSetStageAddr =
            REL::Module::IsVR() ? 0 : tryResolveAddr(RELOCATION_ID(24482, 25004));
        static const std::uintptr_t kSetActiveAddr =
            REL::Module::IsVR() ? 0 : tryResolveAddr(RELOCATION_ID(24471, 24993));
        static const std::uintptr_t kSetObjDisplayedAddr =
            REL::Module::IsVR() ? 0 : tryResolveAddr(RELOCATION_ID(55635, 56682));
        static const std::uintptr_t kSetObjCompletedAddr =
            REL::Module::IsVR() ? 0 : tryResolveAddr(RELOCATION_ID(55634, 56681));

        auto dispatchSetStage = [&](std::uint16_t stage) -> bool
        {
            if (kSetStageAddr != 0)
            {
                return reinterpret_cast<NativeSetStageFn>(kSetStageAddr)(quest, stage);
            }
            if (hasQuestHandle)
            {
                std::int32_t stageArg = static_cast<std::int32_t>(stage);
                auto* args = RE::MakeFunctionArguments(std::move(stageArg));
                RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb;
                return vm->DispatchMethodCall(questHandle, RE::BSFixedString("Quest"),
                                              RE::BSFixedString("SetStage"), args, cb);
            }
            return false;
        };
        auto dispatchSetActive = [&](bool active)
        {
            if (kSetActiveAddr != 0)
            {
                reinterpret_cast<NativeSetActiveFn>(kSetActiveAddr)(quest, active);
                return;
            }
            if (hasQuestHandle)
            {
                bool activeArg = active;
                auto* args = RE::MakeFunctionArguments(std::move(activeArg));
                RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb;
                (void)vm->DispatchMethodCall(questHandle, RE::BSFixedString("Quest"),
                                             RE::BSFixedString("SetActive"), args, cb);
            }
        };
        auto dispatchObjDisplayed = [&](std::uint16_t objIdx)
        {
            if (kSetObjDisplayedAddr != 0 && vm != nullptr)
            {
                reinterpret_cast<NativeSetObjDisplayedFn>(kSetObjDisplayedAddr)(
                    vm, 0u, quest, objIdx, true, false);
                return;
            }
            if (hasQuestHandle)
            {
                std::int32_t objArg = static_cast<std::int32_t>(objIdx);
                bool dispArg = true;
                bool forceArg = false;
                auto* args = RE::MakeFunctionArguments(std::move(objArg), std::move(dispArg),
                                                       std::move(forceArg));
                RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb;
                (void)vm->DispatchMethodCall(questHandle, RE::BSFixedString("Quest"),
                                             RE::BSFixedString("SetObjectiveDisplayed"), args, cb);
            }
        };
        auto dispatchObjCompleted = [&](std::uint16_t objIdx)
        {
            if (kSetObjCompletedAddr != 0 && vm != nullptr)
            {
                reinterpret_cast<NativeSetObjCompletedFn>(kSetObjCompletedAddr)(
                    vm, 0u, quest, objIdx, true);
                return;
            }
            if (hasQuestHandle)
            {
                std::int32_t objArg = static_cast<std::int32_t>(objIdx);
                bool compArg = true;
                auto* args = RE::MakeFunctionArguments(std::move(objArg), std::move(compArg));
                RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb;
                (void)vm->DispatchMethodCall(questHandle, RE::BSFixedString("Quest"),
                                             RE::BSFixedString("SetObjectiveCompleted"), args, cb);
            }
        };

        auto isObjDisplayedInEngine = [&](std::uint16_t idx) -> bool
        {
            for (auto* obj : quest->objectives)
            {
                if (obj && obj->index == idx)
                {
                    const auto st = obj->state.get();
                    return st == RE::QUEST_OBJECTIVE_STATE::kDisplayed ||
                           st == RE::QUEST_OBJECTIVE_STATE::kCompleted ||
                           st == RE::QUEST_OBJECTIVE_STATE::kCompletedDisplayed;
                }
            }
            return false;
        };
        auto isObjCompletedInEngine = [&](std::uint16_t idx) -> bool
        {
            for (auto* obj : quest->objectives)
            {
                if (obj && obj->index == idx)
                {
                    const auto st = obj->state.get();
                    return st == RE::QUEST_OBJECTIVE_STATE::kCompleted ||
                           st == RE::QUEST_OBJECTIVE_STATE::kCompletedDisplayed;
                }
            }
            return false;
        };

        const uint32_t engineStage = quest->GetCurrentStageID();
        if (engineStage < 10u)
        {
            const bool stageOk = dispatchSetStage(10u);
            dispatchSetActive(true);
            g_lastDispatchedQuestStage.store(quest->GetCurrentStageID(), std::memory_order_relaxed);
            logger::info(
                "[TrueGaze::ExpandedPlay] Started journal quest 'The Moth Priest's Pilgrimage' "
                "(SetStage(10)={}, currentStage={}, active={}).",
                stageOk ? "ok" : "false", quest->GetCurrentStageID(),
                quest->IsActive() ? "yes" : "no");
        }

        uint32_t dispMask = g_dispatchedObjDisplayedMask.load(std::memory_order_relaxed);
        uint32_t compMask = g_dispatchedObjCompletedMask.load(std::memory_order_relaxed);

        struct ObjKeyPair
        {
            std::uint16_t objIndex;
            uint32_t bit;
            bool done;
        };
        const ObjKeyPair keyObjs[4] = {
            {10u, 1u << 0, (state.tumblerMask & TumblerBits::kWord) != 0u},
            {20u, 1u << 1, (state.tumblerMask & TumblerBits::kVessel) != 0u},
            {30u, 1u << 2, (state.tumblerMask & TumblerBits::kSharedSight) != 0u},
            {40u, 1u << 3, (state.tumblerMask & TumblerBits::kPilgrimage) != 0u},
        };
        for (const auto& ko : keyObjs)
        {
            if (!isObjDisplayedInEngine(ko.objIndex))
            {
                dispMask |= ko.bit;
                dispatchObjDisplayed(ko.objIndex);
            }
            if ((ko.done || state.ceremonyCompleted) && !isObjCompletedInEngine(ko.objIndex))
            {
                compMask |= ko.bit;
                dispatchObjCompleted(ko.objIndex);
            }
        }

        const bool allFourKeysLit =
            (CountLitTumblers(state.tumblerMask) >= 4u) || state.ceremonyCompleted;
        if (allFourKeysLit)
        {
            if (quest->GetCurrentStageID() < 50u && !state.ceremonyCompleted)
            {
                (void)dispatchSetStage(50u);
                g_lastDispatchedQuestStage.store(quest->GetCurrentStageID(),
                                                 std::memory_order_relaxed);
            }
            constexpr uint32_t kObj50Bit = 1u << 4;
            if (!isObjDisplayedInEngine(50u))
            {
                dispMask |= kObj50Bit;
                dispatchObjDisplayed(50u);
            }
            if (state.ceremonyCompleted)
            {
                if (!isObjCompletedInEngine(50u))
                {
                    compMask |= kObj50Bit;
                    dispatchObjCompleted(50u);
                }
                if (quest->GetCurrentStageID() < 100u && !quest->IsCompleted())
                {
                    (void)dispatchSetStage(100u);
                    g_lastDispatchedQuestStage.store(quest->GetCurrentStageID(),
                                                     std::memory_order_relaxed);
                    logger::info("[TrueGaze::ExpandedPlay] Completed journal quest 'The Moth "
                                 "Priest's Pilgrimage' (currentStage={}).",
                                 quest->GetCurrentStageID());
                }
            }
        }

        g_dispatchedObjDisplayedMask.store(dispMask, std::memory_order_relaxed);
        g_dispatchedObjCompletedMask.store(compMask, std::memory_order_relaxed);
#else
        (void)state;
        (void)startJournalIfEligible;
#endif
    }

    void AwakeningRitual::SetInnRumorInfoSaidOnce(bool said) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (const uint32_t rumorId = g_eslFormRumorInfo.load(std::memory_order_relaxed);
            rumorId != 0u)
        {
            if (auto* info =
                    RE::TESForm::LookupByID<RE::TESTopicInfo>(static_cast<RE::FormID>(rumorId)))
            {
                info->saidOnce = said;
            }
        }
        if (const uint32_t noteId = g_eslFormNoteInfo.load(std::memory_order_relaxed);
            noteId != 0u)
        {
            if (auto* info =
                    RE::TESForm::LookupByID<RE::TESTopicInfo>(static_cast<RE::FormID>(noteId)))
            {
                info->saidOnce = said;
            }
        }
#else
        (void)said;
#endif
    }

    bool AwakeningRitual::IsPlayerCarryingCassianNote() const noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !IsEslPluginLoaded())
        {
            return false;
        }
        const uint32_t noteId = g_eslFormCassianNote.load(std::memory_order_relaxed);
        if (noteId == 0u)
        {
            return false;
        }
        const auto inv = player->GetInventory();
        for (const auto& [item, pair] : inv)
        {
            if (item && pair.first > 0 && static_cast<uint32_t>(item->GetFormID()) == noteId)
            {
                return true;
            }
        }
#endif
        return false;
    }

    void AwakeningRitual::MarkCassianNoteOpened() noexcept
    {
        g_cassianNoteOpenedFlag.store(true, std::memory_order_release);
    }

    bool AwakeningRitual::ConsumeCassianNoteOpened() noexcept
    {
        return g_cassianNoteOpenedFlag.exchange(false, std::memory_order_acq_rel);
    }

    bool AwakeningRitual::PlaceNoteOnSleepingGiantBar() noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !IsEslPluginLoaded() || IsPlayerCarryingCassianNote())
        {
            return false;
        }
        if (g_placedBarNoteRefId.load(std::memory_order_acquire) != 0u)
        {
            return true;
        }
        auto* cell = player->GetParentCell();
        if (!cell)
        {
            return false;
        }
        const uint32_t noteId = g_eslFormCassianNote.load(std::memory_order_relaxed);
        auto* noteObj =
            (noteId != 0u)
                ? RE::TESForm::LookupByID<RE::TESBoundObject>(static_cast<RE::FormID>(noteId))
                : nullptr;
        if (!noteObj)
        {
            return false;
        }

        // Check if an unpicked copy already sits in the Sleeping Giant Inn cell (after save/reload)
        bool alreadyInCell = false;
        cell->ForEachReference(
            [&](RE::TESObjectREFR* ref) -> RE::BSContainer::ForEachResult
            {
                if (ref && !ref->IsDisabled() && !ref->IsDeleted() && ref->GetBaseObject() &&
                    static_cast<uint32_t>(ref->GetBaseObject()->GetFormID()) == noteId)
                {
                    if (auto* playerBase = player->GetActorBase())
                    {
                        ref->extraList.SetOwner(playerBase);
                    }
                    g_placedBarNoteRefId.store(static_cast<uint32_t>(ref->GetFormID()),
                                               std::memory_order_release);
                    alreadyInCell = true;
                    return RE::BSContainer::ForEachResult::kStop;
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
        if (alreadyInCell)
        {
            return true;
        }

        // Exact verified Skyrim.esm bar counter surface in RiverwoodSleepingGiantInn (0x000133C6):
        //   BarStraight01 runs along X=-1159, bar top Z=78.3; Orgnar leans behind the bar at
        //   CounterBarLeanMarker (-1213.2, 78.7). Front-row items sit along X=-1144; placing the
        //   wax-sealed note at (-1144.0, 52.0, 79.5) puts it squarely on the open wooden counter
        //   directly in front of Orgnar between the salmon platter (Y=38.3) and wine carafe (Y=65.8).
        const RE::NiPoint3 barPos{-1144.0f, 52.0f, 79.5f};
        const RE::NiPoint3 barAng{0.0f, 0.0f, 1.57f};

        auto placedPtr = player->PlaceObjectAtMe(noteObj, false);
        auto* placed = placedPtr.get();
        if (!placed)
        {
            return false;
        }
        placed->SetPosition(barPos);
        placed->SetAngle(barAng);
        // Set explicit player ownership so reading/taking the note on the bar is NEVER marked as Steal!
        if (auto* playerBase = player->GetActorBase())
        {
            placed->extraList.SetOwner(playerBase);
        }
        g_placedBarNoteRefId.store(static_cast<uint32_t>(placed->GetFormID()),
                                   std::memory_order_release);
        logger::info(
            "[TrueGaze::ExpandedPlay] Placed Brother Cassian's Note (ref={:08X}) on the "
            "Sleeping Giant Inn bar counter at ({:.1f}, {:.1f}, {:.1f}) with player ownership.",
            static_cast<uint32_t>(placed->GetFormID()), barPos.x, barPos.y, barPos.z);
        return true;
#else
        return false;
#endif
    }

    void AwakeningRitual::RemovePlacedBarNoteIfStillInWorld() noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        const uint32_t refId = g_placedBarNoteRefId.exchange(0u, std::memory_order_acq_rel);
        if (refId == 0u)
        {
            return;
        }
        auto* ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(static_cast<RE::FormID>(refId));
        if (!ref || ref->IsDeleted() || ref->IsDisabled() || !ref->GetParentCell())
        {
            return;
        }
        ref->Disable();
        ref->SetDelete(true);
#endif
    }

    bool AwakeningRitual::DispatchCourierWithCassianNote() noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (!IsEslPluginLoaded() || IsPlayerCarryingCassianNote())
        {
            return false;
        }
        const uint32_t noteId = g_eslFormCassianNote.load(std::memory_order_relaxed);
        auto* noteForm =
            (noteId != 0u) ? RE::TESForm::LookupByID(static_cast<RE::FormID>(noteId)) : nullptr;
        // Verified Skyrim.esm QUST WICourier = 0x00039F82 (script: WICourierScript)
        auto* courierQuest = RE::TESForm::LookupByID<RE::TESQuest>(static_cast<RE::FormID>(0x00039F82u));
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
        if (!noteForm || !courierQuest || !vm || !policy)
        {
            return false;
        }
        const auto handle = policy->GetHandleForObject(RE::TESQuest::FORMTYPE, courierQuest);
        if (handle == policy->EmptyHandle())
        {
            return false;
        }
        RE::TESForm* itemArg = noteForm;
        auto* args = RE::MakeFunctionArguments(std::move(itemArg));
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb;
        const bool ok = vm->DispatchMethodCall(handle, RE::BSFixedString("WICourierScript"),
                                               RE::BSFixedString("addItemToContainer"), args, cb);
        logger::info("[TrueGaze::ExpandedPlay] Dispatched vanilla Skyrim WICourier (00039F82) "
                     "with Brother Cassian's Note ({:08X}): dispatched={}",
                     noteId, ok ? "yes" : "no");
        return ok;
#else
        return false;
#endif
    }

    bool AwakeningRitual::GrantInnNoteToPlayerOnly(const char* source) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !IsEslPluginLoaded())
        {
            return false;
        }
        const uint32_t noteId = g_eslFormCassianNote.load(std::memory_order_relaxed);
        if (noteId == 0u)
        {
            return false;
        }
        auto* noteObj =
            RE::TESForm::LookupByID<RE::TESBoundObject>(static_cast<RE::FormID>(noteId));
        if (!noteObj)
        {
            return false;
        }
        const auto inv = player->GetInventory();
        for (const auto& [item, pair] : inv)
        {
            if (item && pair.first > 0 && static_cast<uint32_t>(item->GetFormID()) == noteId)
            {
                return true;
            }
        }
        player->AddObjectToContainer(noteObj, nullptr, 1, nullptr);
        RE::PlaySound("ITMNoteUpSD");
        const char* safeSource = (source && source[0] != '\0') ? source : "Unknown";
        logger::info("[TrueGaze::ExpandedPlay] Granted '{}' ({:08X}) directly into player "
                     "inventory (source={}).",
                     noteObj->GetName() ? noteObj->GetName() : "Innkeeper's Note", noteId,
                     safeSource);
        return true;
#else
        (void)source;
        return false;
#endif
    }

    bool AwakeningRitual::GiveInnNoteToPlayerAndSpeak(uint32_t speakerActorFormId) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
        {
            return false;
        }

        // 1. Add Innkeeper's Note: The Moth Priest's Passage directly to player inventory (not floor)
        bool addedNote = false;
        if (IsEslPluginLoaded())
        {
            if (const uint32_t noteId = g_eslFormCassianNote.load(std::memory_order_relaxed);
                noteId != 0u)
            {
                if (auto* noteObj = RE::TESForm::LookupByID<RE::TESBoundObject>(
                        static_cast<RE::FormID>(noteId)))
                {
                    bool alreadyHas = false;
                    const auto inv = player->GetInventory();
                    for (const auto& [item, pair] : inv)
                    {
                        if (item && pair.first > 0 &&
                            static_cast<uint32_t>(item->GetFormID()) == noteId)
                        {
                            alreadyHas = true;
                            break;
                        }
                    }
                    if (!alreadyHas)
                    {
                        player->AddObjectToContainer(noteObj, nullptr, 1, nullptr);
                        addedNote = true;
                    }
                }
            }
        }

        // 2. Find Orgnar (BaseFormID 0x00013479, MaleBrute — 100% verified voiced fuz for
        //    "It's all in this note.") or fallback to the speaking actor, and play the line.
        RE::Actor* speaker =
            (speakerActorFormId != 0u)
                ? RE::TESForm::LookupByID<RE::Actor>(static_cast<RE::FormID>(speakerActorFormId))
                : nullptr;
        RE::Actor* orgnar = nullptr;
        if (auto* pl = RE::ProcessLists::GetSingleton())
        {
            for (auto& h : pl->highActorHandles)
            {
                auto ptr = h.get();
                auto* a = ptr.get();
                if (!a || a->IsDead())
                {
                    continue;
                }
                const auto* base = a->GetActorBase();
                const uint32_t baseLow =
                    base ? (static_cast<uint32_t>(base->GetFormID()) & 0x00FFFFFFu) : 0u;
                if (baseLow == 0x013479u) // Orgnar
                {
                    orgnar = a;
                    break;
                }
            }
        }
        RE::Actor* voiceActor = orgnar ? orgnar : speaker;

        bool spoke = false;
        if (voiceActor && !voiceActor->IsDead())
        {
            const uint32_t topicId = g_eslFormNoteTopic.load(std::memory_order_relaxed);
            auto* topic = (topicId != 0u)
                              ? RE::TESForm::LookupByID<RE::TESTopic>(
                                    static_cast<RE::FormID>(topicId))
                              : RE::TESForm::LookupByID<RE::TESTopic>(
                                    static_cast<RE::FormID>(0x000142B5u)); // DialogueGenericHello
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
            if (topic && vm && policy)
            {
                const auto handle = policy->GetHandleForObject(RE::Actor::FORMTYPE, voiceActor);
                if (handle != policy->EmptyHandle())
                {
                    RE::TESTopic* topicArg = topic;
                    RE::Actor* speakAsArg = nullptr;
                    bool inHeadArg = false;
                    auto* args = RE::MakeFunctionArguments(
                        std::move(topicArg), std::move(speakAsArg), std::move(inHeadArg));
                    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
                    spoke = vm->DispatchMethodCall(handle, RE::BSFixedString("ObjectReference"),
                                                   RE::BSFixedString("Say"), args, callback);
                }
            }
        }

        if (addedNote)
        {
            RE::PlaySound("ITMNoteUpSD");
        }
        logger::info("[TrueGaze::ExpandedPlay] Sleeping Giant Inn note handoff: addedNote={} "
                     "spoke={} voiceActor='{}'",
                     addedNote ? "yes" : "no", spoke ? "yes" : "no",
                     (voiceActor && voiceActor->GetName()) ? voiceActor->GetName() : "none");
        return addedNote || spoke;
#else
        (void)speakerActorFormId;
        return false;
#endif
    }

    bool AwakeningRitual::BestowSanctuaryRelicIfEslLoaded(uint8_t sanctuaryBit,
                                                          std::string& outRelicName) noexcept
    {
        outRelicName.clear();
#if __has_include(<RE/Skyrim.h>)
        if (!IsEslPluginLoaded())
        {
            return false;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
        {
            return false;
        }
        const auto spec = GetSanctuaryRelicSpec(sanctuaryBit);
        if (spec.slot == SanctuaryRelicSlot::None)
        {
            return false;
        }

        uint32_t formId = 0u;
        switch (spec.slot)
        {
        case SanctuaryRelicSlot::FirstCanticle:
            formId = g_eslFormBook.load(std::memory_order_relaxed);
            break;
        case SanctuaryRelicSlot::AmuletMoth:
            formId = g_eslFormAmulet.load(std::memory_order_relaxed);
            break;
        case SanctuaryRelicSlot::MothSilkBrowband:
            formId = g_eslFormBrowband.load(std::memory_order_relaxed);
            break;
        case SanctuaryRelicSlot::RingQuietSight:
            formId = g_eslFormRing.load(std::memory_order_relaxed);
            break;
        case SanctuaryRelicSlot::SilverMedallion:
            formId = g_eslFormMedallion.load(std::memory_order_relaxed);
            break;
        case SanctuaryRelicSlot::MothSilkRibbon:
            formId = g_eslFormRibbon.load(std::memory_order_relaxed);
            break;
        default:
            break;
        }
        if (formId == 0u)
        {
            return false;
        }

        auto* obj = RE::TESForm::LookupByID<RE::TESBoundObject>(static_cast<RE::FormID>(formId));
        if (!obj)
        {
            return false;
        }

        // Skip duplicating a relic the player already carries in inventory.
        const auto inv = player->GetInventory();
        for (const auto& [item, pair] : inv)
        {
            if (item && pair.first > 0 && static_cast<uint32_t>(item->GetFormID()) == formId)
            {
                return false;
            }
        }

        auto placed = player->PlaceObjectAtMe(obj, false);
        if (!placed)
        {
            return false;
        }
        if (auto* playerBase = player->GetActorBase())
        {
            placed->SetOwner(playerBase);
        }

        const RE::NiPoint3 pPos = player->GetPosition();
        const float yaw = player->GetAngleZ();
        const RE::NiPoint3 dropPos{pPos.x + std::sin(yaw) * 72.0f, pPos.y + std::cos(yaw) * 72.0f,
                                   pPos.z + 36.0f};
        placed->SetPosition(dropPos);

        outRelicName =
            (obj->GetName() && obj->GetName()[0] != '\0') ? obj->GetName() : spec.relicName;

        logger::info("[TrueGaze::ExpandedPlay] Sanctuary Discovery ({}): placed '{}' ({:08X}) "
                     "with Player ownership (zero cell edits).",
                     spec.sanctuaryTitle, outRelicName, formId);
        return true;
#else
        (void)sanctuaryBit;
        return false;
#endif
    }

    DevDropResult AwakeningRitual::DropAllDevItemsInFrontOfPlayer() noexcept
    {
        DevDropResult res{};
#if __has_include(<RE/Skyrim.h>)
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
        {
            return res;
        }

        const bool eslLoaded = IsEslPluginLoaded();
        res.usedEslItems = eslLoaded;

        std::vector<uint32_t> formIds;
        if (eslLoaded)
        {
            const std::array<uint32_t, 7> eslIds{g_eslFormBook.load(std::memory_order_relaxed),
                                                 g_eslFormCassianNote.load(std::memory_order_relaxed),
                                                 g_eslFormAmulet.load(std::memory_order_relaxed),
                                                 g_eslFormBrowband.load(std::memory_order_relaxed),
                                                 g_eslFormRing.load(std::memory_order_relaxed),
                                                 g_eslFormMedallion.load(std::memory_order_relaxed),
                                                 g_eslFormRibbon.load(std::memory_order_relaxed)};
            for (const uint32_t fid : eslIds)
            {
                if (fid != 0u)
                    formIds.push_back(fid);
            }
            if (const uint32_t spellId = g_eslFormPower.load(std::memory_order_relaxed);
                spellId != 0u)
            {
                if (auto* spell =
                        RE::TESForm::LookupByID<RE::SpellItem>(static_cast<RE::FormID>(spellId)))
                {
                    if (!player->HasSpell(spell))
                    {
                        player->AddSpell(spell);
                    }
                    res.spellGranted = true;
                    g_canticleSpellCheckedLearned.store(true, std::memory_order_release);
                    logger::info("[TrueGaze::ExpandedPlay] Dev drop: granted Right-Hand Spell '{}' "
                                 "({:08X}) to player.",
                                 spell->GetName() ? spell->GetName()
                                                  : "Sight of the First Canticle",
                                 spellId);
                }
            }
        }
        else
        {
            // Pure vanilla Skyrim.esm fallback items for DLL-only progression testing
            formIds = {kVanillaBookEffectsOfScrolls, kVanillaAmuletJulianos, kVanillaAmuletKynareth,
                       kVanillaCirclet01, kVanillaRingSilver};
        }

        // Compute a clean semicircular arc ~1.1 m in front of the player at waist height
        const RE::NiPoint3 pPos = player->GetPosition();
        const float yaw = player->GetAngleZ();
        const float fwdX = std::sin(yaw);
        const float fwdY = std::cos(yaw);
        const float rightX = std::cos(yaw);
        const float rightY = -std::sin(yaw);
        constexpr float kForwardDist = 78.0f; // ~1.1 m ahead of player
        constexpr float kSpacing = 16.0f;     // spread laterally so items don't collide
        constexpr float kDropZ = 42.0f;       // gentle drop onto floor/table

        const float centerIdx =
            0.5f * static_cast<float>(formIds.empty() ? 0 : (formIds.size() - 1));
        for (size_t i = 0; i < formIds.size(); ++i)
        {
            auto* obj =
                RE::TESForm::LookupByID<RE::TESBoundObject>(static_cast<RE::FormID>(formIds[i]));
            if (!obj)
            {
                logger::warn("[TrueGaze::ExpandedPlay] Dev drop: FormID {:08X} not found.",
                             formIds[i]);
                continue;
            }
            auto placed = player->PlaceObjectAtMe(obj, false);
            if (!placed)
            {
                logger::warn(
                    "[TrueGaze::ExpandedPlay] Dev drop: PlaceObjectAtMe failed for {:08X}.",
                    formIds[i]);
                continue;
            }

            // Explicitly assign player ownership so items dropped inside owned interiors
            // (inns, shops, temples) show free "Take / Read" instead of "Steal".
            if (auto* playerBase = player->GetActorBase())
            {
                placed->SetOwner(playerBase);
            }

            const float lateral = (static_cast<float>(i) - centerIdx) * kSpacing;
            const RE::NiPoint3 dropPos{pPos.x + fwdX * kForwardDist + rightX * lateral,
                                       pPos.y + fwdY * kForwardDist + rightY * lateral,
                                       pPos.z + kDropZ};
            placed->SetPosition(dropPos);
            ++res.itemsDropped;

            const char* itemName = obj->GetName() ? obj->GetName() : "Item";
            logger::info("[TrueGaze::ExpandedPlay] Dev drop [{}/{}]: '{}' (base={:08X} ref={:08X}) "
                         "at ({:.1f}, {:.1f}, {:.1f}) owner=Player",
                         res.itemsDropped, formIds.size(), itemName, formIds[i],
                         static_cast<uint32_t>(placed->GetFormID()), dropPos.x, dropPos.y,
                         dropPos.z);
        }

        const std::string traceDetail =
            std::format("mode={} dropped={} power={}", eslLoaded ? "ESL" : "vanilla",
                        res.itemsDropped, res.spellGranted ? "granted" : "none");
        Engine::TraceLogger::Get().LogExpandedPlayEvent(
            Engine::TraceLogger::Get().DurationSeconds(), "DEV_ITEMS_DROPPED", 0u, "", 0u, 0.0f,
            traceDetail.c_str());

        if (res.itemsDropped > 0u)
        {
            GazeReticle::Get().NotifyMilestone(
                std::format("Relics of the First Canticle placed before you ({} items{}).",
                            res.itemsDropped,
                            res.spellGranted ? " + Sight of the First Canticle" : ""),
                0xFFD966u, "UIMenuOKSD", true);
        }
#endif
        return res;
    }
} // namespace TrueGaze::ExpandedPlay
