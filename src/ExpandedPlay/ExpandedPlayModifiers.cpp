#include "ExpandedPlay/ExpandedPlayModifiers.hpp"

#include "Engine/ConfigManager.hpp"
#include "ExpandedPlay/CoSaveSerializer.hpp"
#include "PCH.h"

#include <array>
#include <cmath>
#include <mutex>

namespace TrueGaze::ExpandedPlay
{
    namespace
    {
        std::mutex g_modifierMutex;
        ModifierLedger g_ledger{};
        float g_transientRapportSpeechBonus{0.0f};
        TacticalSightBuffs g_transientTacticalBonuses{};

#if __has_include(<RE/Skyrim.h>)
        void ApplyActorDelta(uint32_t targetFormId, int32_t actorValueId, float delta) noexcept
        {
            if (targetFormId == 0u || !std::isfinite(delta) || std::abs(delta) <= 1e-5f)
            {
                return;
            }

            RE::Actor* actor = nullptr;
            if (auto* player = RE::PlayerCharacter::GetSingleton();
                player && player->GetFormID() == targetFormId)
            {
                actor = player;
            }
            else
            {
                actor = RE::TESForm::LookupByID<RE::Actor>(static_cast<RE::FormID>(targetFormId));
            }

            if (!actor)
            {
                return;
            }

            if (auto* avOwner = actor->AsActorValueOwner())
            {
                // CRIT-04: use kTemporary (never kPermanent) AND strip strictly before
                // PlayerCharacter::SaveGame serializes into .ess so TrueGaze buffs can
                // never permanently bake into user saves even on abrupt uninstall.
                avOwner->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary,
                                       static_cast<RE::ActorValue>(actorValueId), delta);
            }
        }
#else
        void ApplyActorDelta(uint32_t, int32_t, float) noexcept {}
#endif
    } // namespace

    ExpandedPlayModifiers& ExpandedPlayModifiers::Get() noexcept
    {
        static ExpandedPlayModifiers instance;
        return instance;
    }

    void ExpandedPlayModifiers::SyncFromPersistentState(bool expandedPlayEnabled) noexcept
    {
        const auto& cfg = Engine::ConfigManager::GetSingleton();
        if (!expandedPlayEnabled || (!cfg.moduleBookStudy && !cfg.moduleSocialReactions))
        {
            StripAllActiveModifiers();
            return;
        }

#if __has_include(<RE/Skyrim.h>)
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
        {
            return;
        }

        const uint32_t playerId = static_cast<uint32_t>(player->GetFormID());
        const auto ep = cfg.SnapshotExpandedPlay();
        const auto state = CoSaveSerializer::Get().SnapshotState();
        const auto caps = ResolveStageCapabilities(state.awakening, ep.unlockMode);
        if (!caps.expandedActive)
        {
            StripAllActiveModifiers();
            return;
        }
        const auto rawRank = ClassifyScholarshipRank(state.sign.booksFullyStudiedCount);
        const auto rank = std::min(rawRank, caps.maxScholarshipRank);
        const auto bonus = cfg.moduleBookStudy ? ComputeScholarshipBonus(rank) : ScholarshipBonus{};

        float transientSpeech = 0.0f;
        TacticalSightBuffs tactical{};
        {
            std::lock_guard lock(g_modifierMutex);
            if (!cfg.moduleSocialReactions || !caps.allowTransactions)
            {
                g_transientRapportSpeechBonus = 0.0f;
            }
            transientSpeech = g_transientRapportSpeechBonus;
            tactical = g_transientTacticalBonuses;
        }

        struct TargetSpec
        {
            int32_t av;
            float targetDelta;
        };
                    const std::array<TargetSpec, 12> specs{{
                        {static_cast<int32_t>(RE::ActorValue::kMagickaRateMult),
                         bonus.magickaRateMultBonus + tactical.magickaRegenBonus},
                        {static_cast<int32_t>(RE::ActorValue::kSpeechcraftModifier),
                         bonus.speechIllusionBonus + transientSpeech},
                        {static_cast<int32_t>(RE::ActorValue::kIllusionModifier),
                         bonus.speechIllusionBonus},
                        {static_cast<int32_t>(RE::ActorValue::kBlockModifier), tactical.blockBonus},
                        {static_cast<int32_t>(RE::ActorValue::kResistMagic),
                         tactical.resistMagicBonus},
                        {static_cast<int32_t>(RE::ActorValue::kOneHandedModifier),
                         tactical.offenseBonus},
                        {static_cast<int32_t>(RE::ActorValue::kTwoHandedModifier),
                         tactical.offenseBonus},
                        {static_cast<int32_t>(RE::ActorValue::kMarksmanModifier),
                         tactical.offenseBonus},
                        {static_cast<int32_t>(RE::ActorValue::kDamageResist), tactical.armorBonus},
                        {static_cast<int32_t>(RE::ActorValue::kStaminaRateMult),
                         tactical.staminaRegenBonus},
                        {static_cast<int32_t>(RE::ActorValue::kSneakingModifier),
                         tactical.sneakBonus},
                        {static_cast<int32_t>(RE::ActorValue::kPickpocketModifier),
                         tactical.pickpocketBonus},
                    }};

                std::array<TaggedActorModifier, 12> increments{};
                size_t incrementCount = 0;
                {
                    std::lock_guard lock(g_modifierMutex);
                    for (const auto& spec : specs)
                    {
                        const float inc =
                            g_ledger.SetTargetDelta(playerId, spec.av, spec.targetDelta);
                        if (std::abs(inc) > 1e-5f)
                        {
                            increments[incrementCount++] =
                                TaggedActorModifier{playerId, spec.av, inc};
                        }
                    }
                }

                for (size_t i = 0; i < incrementCount; ++i)
                {
                    ApplyActorDelta(increments[i].targetFormId, increments[i].actorValueId,
                                    increments[i].delta);
                }
#endif
            }

            void ExpandedPlayModifiers::StripAllActiveModifiers() noexcept
            {
                std::vector<TaggedActorModifier> teardowns;
                {
                    std::lock_guard lock(g_modifierMutex);
                    teardowns = g_ledger.CollectTeardownDeltas();
                }

                for (const auto& t : teardowns)
                {
                    ApplyActorDelta(t.targetFormId, t.actorValueId, t.delta);
                }
            }

            void ExpandedPlayModifiers::RevertInMemory() noexcept
            {
                std::lock_guard lock(g_modifierMutex);
                g_transientRapportSpeechBonus = 0.0f;
                g_transientTacticalBonuses = {};
                g_ledger.ClearInMemory();
            }

            void ExpandedPlayModifiers::SetTransientRapportSpeechBonus(float speechDelta) noexcept
            {
                const auto& cfg = Engine::ConfigManager::GetSingleton();
                {
                    std::lock_guard lock(g_modifierMutex);
                    if (!cfg.expandedPlayEnabled || !cfg.moduleSocialReactions ||
                        !std::isfinite(speechDelta))
                    {
                        g_transientRapportSpeechBonus = 0.0f;
                    }
                    else
                    {
                        g_transientRapportSpeechBonus = std::clamp(speechDelta, -10.0f, 15.0f);
                    }
                }
                SyncFromPersistentState(cfg.expandedPlayEnabled);
            }

            float ExpandedPlayModifiers::TransientRapportSpeechBonus() const noexcept
            {
                std::lock_guard lock(g_modifierMutex);
                return g_transientRapportSpeechBonus;
            }

            void ExpandedPlayModifiers::SetTransientTacticalBonuses(
                const TacticalSightBuffs& buffs) noexcept
            {
                const auto& cfg = Engine::ConfigManager::GetSingleton();
                {
                    std::lock_guard lock(g_modifierMutex);
                    if (!cfg.expandedPlayEnabled)
                    {
                        g_transientTacticalBonuses = {};
                    }
                    else
                    {
                        TacticalSightBuffs clean{};
                        clean.blockBonus = std::isfinite(buffs.blockBonus)
                                               ? std::clamp(buffs.blockBonus, 0.0f, 25.0f)
                                               : 0.0f;
                        clean.resistMagicBonus =
                            std::isfinite(buffs.resistMagicBonus)
                                ? std::clamp(buffs.resistMagicBonus, 0.0f, 25.0f)
                                : 0.0f;
                        clean.offenseBonus = std::isfinite(buffs.offenseBonus)
                                                 ? std::clamp(buffs.offenseBonus, 0.0f, 20.0f)
                                                 : 0.0f;
                        clean.armorBonus = std::isfinite(buffs.armorBonus)
                                               ? std::clamp(buffs.armorBonus, 0.0f, 60.0f)
                                               : 0.0f;
                        clean.staminaRegenBonus =
                            std::isfinite(buffs.staminaRegenBonus)
                                ? std::clamp(buffs.staminaRegenBonus, 0.0f, 30.0f)
                                : 0.0f;
                        clean.sneakBonus = std::isfinite(buffs.sneakBonus)
                                               ? std::clamp(buffs.sneakBonus, 0.0f, 20.0f)
                                               : 0.0f;
                        clean.pickpocketBonus = std::isfinite(buffs.pickpocketBonus)
                                                    ? std::clamp(buffs.pickpocketBonus, 0.0f, 25.0f)
                                                    : 0.0f;
                        clean.magickaRegenBonus =
                            std::isfinite(buffs.magickaRegenBonus)
                                ? std::clamp(buffs.magickaRegenBonus, 0.0f, 30.0f)
                                : 0.0f;
                        clean.durationSec = buffs.durationSec;
                        g_transientTacticalBonuses = clean;
                    }
                }
                SyncFromPersistentState(cfg.expandedPlayEnabled);
            }

            TacticalSightBuffs ExpandedPlayModifiers::TransientTacticalBonuses() const noexcept
            {
                std::lock_guard lock(g_modifierMutex);
                return g_transientTacticalBonuses;
            }

            size_t ExpandedPlayModifiers::ActiveModifierCount() const noexcept
            {
                std::lock_guard lock(g_modifierMutex);
                return g_ledger.ActiveCount();
            }
        } // namespace TrueGaze::ExpandedPlay
