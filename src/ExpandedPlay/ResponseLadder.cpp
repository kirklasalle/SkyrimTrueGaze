#include "ExpandedPlay/ResponseLadder.hpp"

#include "Engine/GazeEngine.hpp"
#include "Engine/TraceLogger.hpp"
#include "ExpandedPlay/GazeReticle.hpp"
#include "Integrations/OarConditions.hpp"
#include "PCH.h"

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <mutex>
#include <string>

namespace TrueGaze::ExpandedPlay
{
    namespace
    {
#if __has_include(<RE/Skyrim.h>)
        struct FaceGenSpinGuard
        {
            RE::BSSpinLock& lock;
            explicit FaceGenSpinGuard(RE::BSSpinLock& l) noexcept : lock(l) { lock.Lock(); }
            ~FaceGenSpinGuard() noexcept { lock.Unlock(); }
            FaceGenSpinGuard(const FaceGenSpinGuard&) = delete;
            FaceGenSpinGuard& operator=(const FaceGenSpinGuard&) = delete;
        };

        struct ActiveExpressionSlot
        {
            uint32_t actorFormId{0};
            std::uint32_t moodIndex{0};
            float currentWeight{0.0f};
            float targetWeight{0.0f};
            float writtenWeight{-1.0f};
            float moodLockedUntilSec{0.0f};
            float expiresAtSec{0.0f};
            bool active{false};
        };

        constexpr float kMoodSwitchHoldSec = 0.65f;
        constexpr float kRampRatePerSec = 1.80f;

        std::mutex g_exprMutex;
        std::array<ActiveExpressionSlot, 12> g_activeExprs{};

        void WriteActorFaceMoodWeight(uint32_t actorFormId, std::uint32_t moodIdx,
                                      float weight) noexcept
        {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(static_cast<RE::FormID>(actorFormId));
            if (!actor || !actor->Get3D() || actor->GetCurrentScene() || actor->IsInCombat())
            {
                return;
            }
            auto* uiMgr = RE::UI::GetSingleton();
            const bool inDialogue = uiMgr && uiMgr->IsMenuOpen(RE::DialogueMenu::MENU_NAME);
            if (inDialogue)
            {
                return;
            }
            if (auto* faceGen = actor->GetFaceGenAnimationData())
            {
                FaceGenSpinGuard guard(faceGen->lock);
                if (!faceGen->exprOverride && faceGen->expressionKeyFrame.values &&
                    moodIdx < faceGen->expressionKeyFrame.count)
                {
                    faceGen->expressionKeyFrame.SetValue(moodIdx, std::clamp(weight, 0.0f, 0.65f));
                    faceGen->expressionKeyFrame.isUpdated = true;
                }
            }
        }
#endif
    } // namespace

    void ResponseLadder::ApplyTransientExpression(uint32_t actorFormId, FaceExpressionMood mood,
                                                  float weight, float durationSec,
                                                  float nowSessionSec) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (actorFormId == 0u || !std::isfinite(weight) || weight <= 0.005f)
        {
            return;
        }
        const auto moodIdx = static_cast<std::uint32_t>(mood);
        const float clampedTarget = std::clamp(weight, 0.0f, 0.62f);
        const float safeDur =
            (std::isfinite(durationSec) && durationSec > 0.4f) ? durationSec : 2.0f;
        const float safeNow = std::isfinite(nowSessionSec) ? nowSessionSec : 0.0f;

        std::uint32_t prevMoodToClear = 0u;
        bool hasPrevMood = false;
        uint32_t evictedActor = 0u;
        std::uint32_t evictedMood = 0u;
        bool moodChangedLog = false;

        {
            std::lock_guard lock(g_exprMutex);
            ActiveExpressionSlot* slot = nullptr;
            ActiveExpressionSlot* oldest = &g_activeExprs[0];
            for (auto& s : g_activeExprs)
            {
                if (s.active && s.actorFormId == actorFormId)
                {
                    slot = &s;
                    break;
                }
                if (!s.active && !slot)
                {
                    slot = &s;
                }
                if (s.active && (!oldest->active || s.expiresAtSec < oldest->expiresAtSec))
                {
                    oldest = &s;
                }
            }
            if (!slot)
            {
                slot = oldest;
                if (slot->active)
                {
                    evictedActor = slot->actorFormId;
                    evictedMood = slot->moodIndex;
                }
            }

            if (slot->active && slot->actorFormId == actorFormId)
            {
                if (slot->moodIndex != moodIdx)
                {
                    // Debounce mood flips so micro-aim jitter can never strobe an NPC's face
                    if (safeNow < slot->moodLockedUntilSec)
                    {
                        slot->expiresAtSec = std::max(slot->expiresAtSec, safeNow + safeDur);
                        return;
                    }
                    prevMoodToClear = slot->moodIndex;
                    hasPrevMood = true;
                    slot->moodIndex = moodIdx;
                    slot->currentWeight = 0.0f;
                    slot->writtenWeight = -1.0f;
                    slot->moodLockedUntilSec = safeNow + kMoodSwitchHoldSec;
                    moodChangedLog = true;
                }
                slot->targetWeight = clampedTarget;
                slot->expiresAtSec = std::max(slot->expiresAtSec, safeNow + safeDur);
            }
            else
            {
                slot->actorFormId = actorFormId;
                slot->moodIndex = moodIdx;
                slot->currentWeight = 0.0f;
                slot->targetWeight = clampedTarget;
                slot->writtenWeight = -1.0f;
                slot->moodLockedUntilSec = safeNow + kMoodSwitchHoldSec;
                slot->expiresAtSec = safeNow + safeDur;
                slot->active = true;
                moodChangedLog = true;
            }
        }

        if (evictedActor != 0u)
        {
            WriteActorFaceMoodWeight(evictedActor, evictedMood, 0.0f);
        }
        if (hasPrevMood)
        {
            WriteActorFaceMoodWeight(actorFormId, prevMoodToClear, 0.0f);
        }
        if (moodChangedLog)
        {
            const std::string detail = std::format("mood={} targetWeight={:.2f} dur={:.1f}s",
                                                   moodIdx, clampedTarget, safeDur);
            Engine::TraceLogger::Get().LogExpandedPlayEvent(safeNow, "EXPRESSION_SET", actorFormId,
                                                            "", 0u, 0.0f, detail.c_str());
            logger::debug("[TrueGaze::ExpandedPlay] Expression {:08X}: {}", actorFormId, detail);
        }
#else
        (void)actorFormId;
        (void)mood;
        (void)weight;
        (void)durationSec;
        (void)nowSessionSec;
#endif
    }

    void ResponseLadder::TickExpressionResets(float nowSessionSec, float deltaSec,
                                              uint32_t activeFocusActorFormId) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        const float safeNow = std::isfinite(nowSessionSec) ? nowSessionSec : 0.0f;
        const float safeDt =
            (std::isfinite(deltaSec) && deltaSec > 0.0f) ? std::min(deltaSec, 0.25f) : 0.10f;
        const float maxStep = kRampRatePerSec * safeDt;

        struct WriteOp
        {
            uint32_t actorFormId{0};
            std::uint32_t moodIndex{0};
            float weight{0.0f};
            bool cleared{false};
        };

        std::array<WriteOp, 12> ops{};
        size_t opCount = 0;

        {
            std::lock_guard lock(g_exprMutex);
            for (auto& s : g_activeExprs)
            {
                if (!s.active)
                {
                    continue;
                }
                const bool expired = (safeNow >= s.expiresAtSec);
                const bool focusLeft =
                    (s.actorFormId != activeFocusActorFormId && safeNow >= (s.expiresAtSec - 0.5f));
                if (expired || focusLeft)
                {
                    s.targetWeight = 0.0f;
                }

                const float diff = s.targetWeight - s.currentWeight;
                if (std::abs(diff) <= maxStep)
                {
                    s.currentWeight = s.targetWeight;
                }
                else
                {
                    s.currentWeight += (diff > 0.0f ? maxStep : -maxStep);
                }

                const bool doneFadingOut = (s.targetWeight <= 0.001f && s.currentWeight <= 0.001f);
                if (doneFadingOut)
                {
                    ops[opCount++] = {s.actorFormId, s.moodIndex, 0.0f, true};
                    s = {};
                }
                else if (std::abs(s.currentWeight - s.writtenWeight) > 0.005f)
                {
                    s.writtenWeight = s.currentWeight;
                    ops[opCount++] = {s.actorFormId, s.moodIndex, s.currentWeight, false};
                }
            }
        }

        for (size_t i = 0; i < opCount; ++i)
        {
            WriteActorFaceMoodWeight(ops[i].actorFormId, ops[i].moodIndex, ops[i].weight);
            if (ops[i].cleared)
            {
                const std::string detail = std::format("mood={} reset=0.00", ops[i].moodIndex);
                Engine::TraceLogger::Get().LogExpandedPlayEvent(
                    safeNow, "EXPRESSION_RESET", ops[i].actorFormId, "", 0u, 0.0f, detail.c_str());
            }
        }
#else
        (void)nowSessionSec;
        (void)deltaSec;
        (void)activeFocusActorFormId;
#endif
    }

    void ResponseLadder::ClearAllTransientExpressions() noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        std::array<ActiveExpressionSlot, 12> toClear{};
        size_t clearCount = 0;
        {
            std::lock_guard lock(g_exprMutex);
            for (auto& s : g_activeExprs)
            {
                if (s.active)
                {
                    toClear[clearCount++] = s;
                    s = {};
                }
            }
        }
        for (size_t i = 0; i < clearCount; ++i)
        {
            WriteActorFaceMoodWeight(toClear[i].actorFormId, toClear[i].moodIndex, 0.0f);
        }
#endif
    }

    void ResponseLadder::ApplyDecision(uint32_t actorFormId, std::string_view actorName,
                                       uint8_t regionId, float dwellSec,
                                       const InterpretationResult& interpResult,
                                       const ResponseDecision& decision,
                                       float nowSessionSec) noexcept
    {
        if (actorFormId == 0u)
        {
            return;
        }

        // Rung 1: Eyes & HCEP runtime mode / reciprocity lock
        if (decision.rung1.applyModeOverride)
        {
            if (auto* runtime = Engine::GazeEngine::Get().FindActor(actorFormId))
            {
                runtime->hcepMode = decision.rung1.hcepMode;
                runtime->modeOverrideTimerSec = decision.rung1.overrideDurationSec;
                runtime->hasModeOverride = true;
                if (decision.rung1.lockMutualReciprocity)
                {
                    runtime->crosshairHoldTimerSec =
                        std::max(runtime->crosshairHoldTimerSec, 0.35f);
                }
                if (decision.rung1.forceAversion)
                {
                    runtime->cgaActive = true;
                }
            }
        }

        // Rung 3: OAR Expanded Play conditions cache
        Integrations::OarConditions::PublishExpandedPlayState(
            actorFormId, decision.rung3.interpretation, decision.rung3.demeanor,
            decision.rung3.presenceTier, decision.rung3.dominantImpression);

#if __has_include(<RE/Skyrim.h>)
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(static_cast<RE::FormID>(actorFormId));

        // Rung 2: Gentle non-dialogue FaceGen expression weight with automatic neutral reset
        if (actor && decision.rung2.applyExpression)
        {
            ApplyTransientExpression(actorFormId, decision.rung2.mood, decision.rung2.weight,
                                     decision.rung2.durationSec, nowSessionSec);
        }
#else
        (void)nowSessionSec;
#endif

        // Rung 4: Whispers, SKSE ModCallbackEvent, and AI-NPC Bridge emission
        if (decision.rung4.emitWhisper)
        {
            const std::string whisper = FormatWhisperText(actorName, interpResult.interpretation);
            if (!whisper.empty())
            {
                GazeReticle::Get().PushWhisper(whisper, decision.rung4.whisperColorRgb);
            }
        }

#if __has_include(<RE/Skyrim.h>)
        if (decision.rung4.emitModEvent || decision.rung4.emitAiNpcBridge)
        {
            const std::string context = FormatAiNpcBridgeContext(
                actorName, regionId, interpResult.interpretation, interpResult.band, dwellSec,
                static_cast<Demeanor>(decision.rung3.demeanor));

            if (decision.rung4.emitModEvent)
            {
                if (auto* source = SKSE::GetModCallbackEventSource())
                {
                    SKSE::ModCallbackEvent ev{};
                    ev.eventName = RE::BSFixedString("TrueGaze_OnGazeInterpreted");
                    ev.strArg = RE::BSFixedString(context.c_str());
                    ev.numArg = interpResult.strength;
                    ev.sender = actor;
                    source->SendEvent(&ev);
                }
            }

            if (decision.rung4.emitAiNpcBridge)
            {
                logger::info("[TrueGaze::AiNpcBridge] {}", context);
            }
        }
#else
        (void)regionId;
        (void)dwellSec;
#endif
    }

    bool ResponseLadder::TryInitiateEyeContactGreeting(uint32_t actorFormId,
                                                       std::string_view actorName,
                                                       bool whispersEnabled) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (actorFormId == 0u)
        {
            return false;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(static_cast<RE::FormID>(actorFormId));
        if (!player || !actor || actor->IsDead() || actor->IsActivationBlocked())
        {
            return false;
        }
        if (actor->GetCurrentScene() || actor->IsInCombat() || !actor->CanTalkToPlayer())
        {
            return false;
        }

        const bool activated = actor->ActivateRef(player, 0, nullptr, 0, false);
        if (activated && whispersEnabled)
        {
            const std::string_view who = actorName.empty() ? "They" : actorName;
            GazeReticle::Get().PushWhisper(std::format("{} meets your eyes in greeting.", who),
                                           0xE6D8B8u);
        }
        return activated;
#else
        (void)actorFormId;
        (void)actorName;
        (void)whispersEnabled;
        return false;
#endif
    }
} // namespace TrueGaze::ExpandedPlay
