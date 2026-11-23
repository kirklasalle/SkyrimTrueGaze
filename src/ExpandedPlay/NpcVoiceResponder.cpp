#include "ExpandedPlay/NpcVoiceResponder.hpp"

#include "Engine/TraceLogger.hpp"
#include "PCH.h"

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#endif

#include <atomic>
#include <format>
#include <mutex>

namespace TrueGaze::ExpandedPlay
{
    namespace
    {
        std::mutex g_voiceMutex;
        VoiceResponseBudget g_budget{};
        std::atomic<uint32_t> g_entropyCounter{1u};

#if __has_include(<RE/Skyrim.h>)
        // Verified base Skyrim.esm DIAL (TESTopic) FormIDs with full vanilla voice-type coverage
        constexpr uint32_t kTopicGenericHello      = 0x000142B5u; // DialogueGenericHello
        constexpr uint32_t kTopicGenericGoodbye    = 0x000140FEu; // DialogueGenericGoodbye
        constexpr uint32_t kTopicGenericFireWeapon = 0x000142B1u; // DialogueGenericFireWeapon
        constexpr uint32_t kTopicTrespass          = 0x0002A457u; // Generic Trespass warning
        constexpr uint32_t kTopicTaunt             = 0x0002E33Du; // Generic Taunt / challenge
        constexpr uint32_t kTopicAcceptYield       = 0x0003F500u; // Generic AcceptYield / stand-down

        [[nodiscard]] uint32_t TopicFormIdForEvent(VoiceEventKind kind) noexcept
        {
            switch (kind)
            {
            case VoiceEventKind::WarmCommit:      return kTopicGenericHello;
            case VoiceEventKind::IntrusionStare:  return kTopicTrespass;
            case VoiceEventKind::ArmedCoerce:     return kTopicGenericFireWeapon;
            case VoiceEventKind::StandDownGrace:  return kTopicAcceptYield;
            case VoiceEventKind::GuardGlance:     return kTopicTrespass;
            case VoiceEventKind::ContestTension:  return kTopicTaunt;
            case VoiceEventKind::FactionScrutiny: return kTopicGenericHello;
            case VoiceEventKind::None:
            default:                              return 0u;
            }
        }

        [[nodiscard]] const char* VoiceEventName(VoiceEventKind kind) noexcept
        {
            switch (kind)
            {
            case VoiceEventKind::WarmCommit:      return "WarmCommit";
            case VoiceEventKind::IntrusionStare:  return "IntrusionStare";
            case VoiceEventKind::ArmedCoerce:     return "ArmedCoerce";
            case VoiceEventKind::StandDownGrace:  return "StandDownGrace";
            case VoiceEventKind::GuardGlance:     return "GuardGlance";
            case VoiceEventKind::ContestTension:  return "ContestTension";
            case VoiceEventKind::FactionScrutiny: return "FactionScrutiny";
            case VoiceEventKind::None:
            default:                              return "None";
            }
        }
#endif
    } // namespace

    NpcVoiceResponder& NpcVoiceResponder::Get() noexcept
    {
        static NpcVoiceResponder instance;
        return instance;
    }

    void NpcVoiceResponder::Reset() noexcept
    {
        std::lock_guard lock(g_voiceMutex);
        g_budget.Reset();
    }

    bool NpcVoiceResponder::TrySpeak(uint32_t actorFormId, VoiceEventKind kind, float nowSessionSec,
                                     bool inDialogueMenu, bool playerSneaking,
                                     bool featureEnabled) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (!featureEnabled || actorFormId == 0u || kind == VoiceEventKind::None)
        {
            return false;
        }
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(static_cast<RE::FormID>(actorFormId));
        if (!actor || actor->IsPlayerRef())
        {
            return false;
        }

        VoiceBudgetInput in{};
        in.actorFormId = actorFormId;
        in.kind = kind;
        in.nowSessionSec = nowSessionSec;
        in.featureEnabled = featureEnabled;
        in.actorSpeaking = !actor->QSpeakingDone() || (actor->GetVoiceRecoveryTime() > 0.05f);
        in.inDialogueMenu = inDialogueMenu;
        in.inScene = (actor->GetCurrentScene() != nullptr);
        in.inCombat = actor->IsInCombat();
        in.actorDeadOrDisabled = actor->IsDead() || actor->IsDisabled();
        in.actorIsChild = actor->IsChild();
        in.playerSneaking = playerSneaking;
        in.entropySeed = g_entropyCounter.fetch_add(1u, std::memory_order_relaxed);

        const uint32_t topicId = TopicFormIdForEvent(kind);
        auto* topic = (topicId != 0u)
                          ? RE::TESForm::LookupByID<RE::TESTopic>(static_cast<RE::FormID>(topicId))
                          : nullptr;
        if (!topic)
        {
            return false;
        }

        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
        if (!vm || !policy)
        {
            return false;
        }
        const auto handle = policy->GetHandleForObject(RE::Actor::FORMTYPE, actor);
        if (handle == policy->EmptyHandle())
        {
            return false;
        }

        // Only consume cooldown budget after verifying topic, VM, and actor handle exist.
        {
            std::lock_guard lock(g_voiceMutex);
            if (!g_budget.TryApprove(in))
            {
                return false;
            }
        }

        RE::TESTopic* topicArg = topic;
        RE::Actor* speakAsArg = nullptr;
        bool inHeadArg = false;
        auto* args = RE::MakeFunctionArguments(std::move(topicArg), std::move(speakAsArg),
                                               std::move(inHeadArg));
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
        const bool dispatched = vm->DispatchMethodCall(
            handle, RE::BSFixedString("ObjectReference"), RE::BSFixedString("Say"), args, callback);
        if (dispatched)
        {
            const char* name = actor->GetName() ? actor->GetName() : "NPC";
            const std::string detail =
                std::format("event={} topic={:08X}", VoiceEventName(kind), topicId);
            Engine::TraceLogger::Get().LogExpandedPlayEvent(nowSessionSec, "NPC_VOICE_BARK",
                                                            actorFormId, name, 0u, 0.0f,
                                                            detail.c_str());
            logger::info("[TrueGaze::ExpandedPlay] Voice response '{}' ({:08X}): {}", name,
                         actorFormId, detail);
        }
        return dispatched;
#else
        (void)actorFormId;
        (void)kind;
        (void)nowSessionSec;
        (void)inDialogueMenu;
        (void)playerSneaking;
        (void)featureEnabled;
        return false;
#endif
    }
} // namespace TrueGaze::ExpandedPlay
