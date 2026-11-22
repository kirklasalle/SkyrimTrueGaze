#include "OarCustomConditions.hpp"
#include "OarConditions.hpp"

#include "API/OpenAnimationReplacerAPI-Conditions.h"

#include <cmath>
#include <format>

// ---------------------------------------------------------------------------
// TrueGaze custom conditions for Open Animation Replacer (issue #6).
//
// Contract source: OAR's published Conditions API (interface V3), vendored
// unmodified in extern/OpenAnimationReplacer-API. The pattern follows
// ersh1/OpenAnimationReplacer-ExamplePlugin: subclass Conditions::CustomCondition,
// add base components in the constructor, implement EvaluateImpl, and register
// with OAR_API::Conditions::AddCustomCondition<T>() during kPostLoad.
//
// CustomCondition wraps an OAR-owned condition that handles serialisation,
// negation, the disabled state and the editor UI. TrueGaze provides only the
// evaluation logic and the names and descriptions shown to users.
//
// Evaluation reads OarConditions' state cache, which GazeEngine writes every tick.
// OAR may evaluate conditions off the main thread. The cache is protected by a
// shared_mutex, and the evaluators below touch nothing else.
// ---------------------------------------------------------------------------

namespace TrueGaze::Integrations::OarCustomConditions
{
    namespace
    {
        /// First plugin version whose conditions OAR actually registers. Rules that
        /// reference these conditions should set "requiredVersion" to this or later.
        constexpr REL::Version kFirstRealVersion{1, 0, 8};

        constexpr std::string_view ModeName(uint8_t mode) noexcept
        {
            switch (mode)
            {
            case 0:
                return "LOGIC"sv;
            case 1:
                return "AFFECT"sv;
            case 2:
                return "SPIRIT"sv;
            case 3:
                return "HEART"sv;
            case 4:
                return "THINK"sv;
            default:
                return "?"sv;
            }
        }

        /// Converts a numeric component value to an integer index in [0, maxValue].
        /// Non-finite or out-of-range values are rejected rather than clamped:
        /// a rule asking for mode 7 should never match.
        [[nodiscard]] bool ToIndex(float value, long maxValue, uint8_t &out) noexcept
        {
            if (!std::isfinite(value))
            {
                return false;
            }
            const long rounded = std::lround(value);
            if (rounded < 0 || rounded > maxValue)
            {
                return false;
            }
            out = static_cast<uint8_t>(rounded);
            return true;
        }

        [[nodiscard]] bool TryGetState(RE::TESObjectREFR *refr, OarConditions::ActorState &out) noexcept
        {
            return refr && OarConditions::TryGetActorState(refr->GetFormID(), out);
        }

        constexpr auto kNoState = "No TrueGaze state for this reference"sv;

        // -------------------------------------------------------------------
        // TrueGaze_IsMode: true while the actor is in the given HCEP mode.
        // -------------------------------------------------------------------
        class IsModeCondition : public ::Conditions::CustomCondition
        {
        public:
            static constexpr std::string_view CONDITION_NAME = "TrueGaze_IsMode"sv;

            IsModeCondition()
            {
                _mode = static_cast<::Conditions::INumericConditionComponent *>(AddBaseComponent(
                    ::Conditions::ConditionComponentType::kNumeric, "Mode",
                    "HCEP mode to match: 0 = LOGIC, 1 = AFFECT, 2 = SPIRIT, 3 = HEART, 4 = THINK."));
            }

            RE::BSString GetName() const override { return CONDITION_NAME.data(); }

            RE::BSString GetDescription() const override
            {
                return "True while TrueGaze reports the actor in the given HCEP cognitive mode."sv.data();
            }

            REL::Version GetRequiredVersion() const override { return kFirstRealVersion; }

            RE::BSString GetArgument() const override
            {
                return std::format("Mode == {}", _mode->GetArgument().data()).data();
            }

            RE::BSString GetCurrent(RE::TESObjectREFR *a_refr) const override
            {
                OarConditions::ActorState state;
                if (!TryGetState(a_refr, state))
                {
                    return kNoState.data();
                }
                return std::format("{} ({})", state.hcepMode, ModeName(state.hcepMode)).data();
            }

        protected:
            bool EvaluateImpl(RE::TESObjectREFR *a_refr, [[maybe_unused]] RE::hkbClipGenerator *a_clipGenerator,
                              [[maybe_unused]] void *a_subMod) const override
            {
                uint8_t target = 0;
                if (!a_refr || !ToIndex(_mode->GetNumericValue(a_refr), 4, target))
                {
                    return false;
                }
                return OarConditions::EvaluateIsMode(a_refr->GetFormID(), target);
            }

        private:
            ::Conditions::INumericConditionComponent *_mode{nullptr};
        };

        // -------------------------------------------------------------------
        // TrueGaze_IsMutualGaze: true once mutual gaze with the player has been
        // held for at least the given number of seconds. Negate it for "less than".
        // -------------------------------------------------------------------
        class IsMutualGazeCondition : public ::Conditions::CustomCondition
        {
        public:
            static constexpr std::string_view CONDITION_NAME = "TrueGaze_IsMutualGaze"sv;

            IsMutualGazeCondition()
            {
                _seconds = static_cast<::Conditions::INumericConditionComponent *>(AddBaseComponent(
                    ::Conditions::ConditionComponentType::kNumeric, "Minimum seconds",
                    "How long mutual gaze with the player must have been held, in seconds."));
            }

            RE::BSString GetName() const override { return CONDITION_NAME.data(); }

            RE::BSString GetDescription() const override
            {
                return "True once the actor has held mutual gaze with the player for at least the given time."sv
                    .data();
            }

            REL::Version GetRequiredVersion() const override { return kFirstRealVersion; }

            RE::BSString GetArgument() const override
            {
                return std::format("MutualGazeHoldSec >= {}", _seconds->GetArgument().data()).data();
            }

            RE::BSString GetCurrent(RE::TESObjectREFR *a_refr) const override
            {
                OarConditions::ActorState state;
                if (!TryGetState(a_refr, state))
                {
                    return kNoState.data();
                }
                return std::format("{:.2f} s", state.mutualGazeHoldSec).data();
            }

        protected:
            bool EvaluateImpl(RE::TESObjectREFR *a_refr, [[maybe_unused]] RE::hkbClipGenerator *a_clipGenerator,
                              [[maybe_unused]] void *a_subMod) const override
            {
                if (!a_refr)
                {
                    return false;
                }
                const float threshold = _seconds->GetNumericValue(a_refr);
                if (!std::isfinite(threshold))
                {
                    return false;
                }
                return OarConditions::EvaluateIsMutualGaze(a_refr->GetFormID(), threshold);
            }

        private:
            ::Conditions::INumericConditionComponent *_seconds{nullptr};
        };

        // -------------------------------------------------------------------
        // TrueGaze_GetGazeRegion: true while the actor's gaze rests on the given
        // region ID (0-12, see src/Engine/GazeRegion.hpp).
        // -------------------------------------------------------------------
        class GetGazeRegionCondition : public ::Conditions::CustomCondition
        {
        public:
            static constexpr std::string_view CONDITION_NAME = "TrueGaze_GetGazeRegion"sv;

            GetGazeRegionCondition()
            {
                _region = static_cast<::Conditions::INumericConditionComponent *>(AddBaseComponent(
                    ::Conditions::ConditionComponentType::kNumeric, "Region",
                    "Gaze region ID to match (0-12). See TrueGaze documentation for the region table."));
            }

            RE::BSString GetName() const override { return CONDITION_NAME.data(); }

            RE::BSString GetDescription() const override
            {
                return "True while TrueGaze reports the actor's gaze resting on the given region."sv.data();
            }

            REL::Version GetRequiredVersion() const override { return kFirstRealVersion; }

            RE::BSString GetArgument() const override
            {
                return std::format("Region == {}", _region->GetArgument().data()).data();
            }

            RE::BSString GetCurrent(RE::TESObjectREFR *a_refr) const override
            {
                OarConditions::ActorState state;
                if (!TryGetState(a_refr, state))
                {
                    return kNoState.data();
                }
                return std::format("{}", state.gazeRegion).data();
            }

        protected:
            bool EvaluateImpl(RE::TESObjectREFR *a_refr, [[maybe_unused]] RE::hkbClipGenerator *a_clipGenerator,
                              [[maybe_unused]] void *a_subMod) const override
            {
                uint8_t target = 0;
                if (!a_refr || !ToIndex(_region->GetNumericValue(a_refr), 12, target))
                {
                    return false;
                }
                return OarConditions::EvaluateGazeRegion(a_refr->GetFormID(), target);
            }

        private:
            ::Conditions::INumericConditionComponent *_region{nullptr};
        };

        // -------------------------------------------------------------------
        // TrueGaze_GetInterpretation: true while actor interprets the player's
        // look as the given Interpretation enum (0-11).
        // -------------------------------------------------------------------
        class GetInterpretationCondition : public ::Conditions::CustomCondition
        {
        public:
            static constexpr std::string_view CONDITION_NAME = "TrueGaze_GetInterpretation"sv;

            GetInterpretationCondition()
            {
                _interpretation =
                    static_cast<::Conditions::INumericConditionComponent *>(AddBaseComponent(
                        ::Conditions::ConditionComponentType::kNumeric, "Interpretation",
                        "Expanded Play interpretation ID to match (0=None, 1=Respect, 2=Warmth, "
                        "3=Attraction, 4=Challenge, 5=IntrusionLeer, 6=Suspicion, 7=Deference, "
                        "8=Contempt, 9=Compassion, 10=Deceit, 11=Recognition)."));
            }

            RE::BSString GetName() const override { return CONDITION_NAME.data(); }

            RE::BSString GetDescription() const override
            {
                return "True while TrueGaze Expanded Play reports this actor holding the given interpretation."sv
                    .data();
            }

            REL::Version GetRequiredVersion() const override { return kFirstRealVersion; }

            RE::BSString GetArgument() const override
            {
                return std::format("Interpretation == {}", _interpretation->GetArgument().data())
                    .data();
            }

            RE::BSString GetCurrent(RE::TESObjectREFR *a_refr) const override
            {
                OarConditions::ActorState state;
                if (!TryGetState(a_refr, state))
                {
                    return kNoState.data();
                }
                return std::format("{}", state.interpretation).data();
            }

        protected:
            bool EvaluateImpl(RE::TESObjectREFR *a_refr,
                              [[maybe_unused]] RE::hkbClipGenerator *a_clipGenerator,
                              [[maybe_unused]] void *a_subMod) const override
            {
                uint8_t target = 0;
                if (!a_refr || !ToIndex(_interpretation->GetNumericValue(a_refr), 11, target))
                {
                    return false;
                }
                return OarConditions::EvaluateInterpretation(a_refr->GetFormID(), target);
            }

        private:
            ::Conditions::INumericConditionComponent *_interpretation{nullptr};
        };

        // -------------------------------------------------------------------
        // TrueGaze_GetDemeanor: true while the player's active Demeanor (0-5)
        // observed by this actor matches the target value.
        // -------------------------------------------------------------------
        class GetDemeanorCondition : public ::Conditions::CustomCondition
        {
        public:
            static constexpr std::string_view CONDITION_NAME = "TrueGaze_GetDemeanor"sv;

            GetDemeanorCondition()
            {
                _demeanor =
                    static_cast<::Conditions::INumericConditionComponent *>(AddBaseComponent(
                        ::Conditions::ConditionComponentType::kNumeric, "Demeanor",
                        "Player Demeanor ID to match (0=Courteous, 1=Warm, 2=Stern, 3=Evasive, "
                        "4=Deferential, 5=Predatory)."));
            }

            RE::BSString GetName() const override { return CONDITION_NAME.data(); }

            RE::BSString GetDescription() const override
            {
                return "True while TrueGaze Expanded Play reports the player bearing the given Demeanor."sv
                    .data();
            }

            REL::Version GetRequiredVersion() const override { return kFirstRealVersion; }

            RE::BSString GetArgument() const override
            {
                return std::format("Demeanor == {}", _demeanor->GetArgument().data()).data();
            }

            RE::BSString GetCurrent(RE::TESObjectREFR *a_refr) const override
            {
                OarConditions::ActorState state;
                if (!TryGetState(a_refr, state))
                {
                    return kNoState.data();
                }
                return std::format("{}", state.demeanor).data();
            }

        protected:
            bool EvaluateImpl(RE::TESObjectREFR *a_refr,
                              [[maybe_unused]] RE::hkbClipGenerator *a_clipGenerator,
                              [[maybe_unused]] void *a_subMod) const override
            {
                uint8_t target = 0;
                if (!a_refr || !ToIndex(_demeanor->GetNumericValue(a_refr), 5, target))
                {
                    return false;
                }
                return OarConditions::EvaluateDemeanor(a_refr->GetFormID(), target);
            }

        private:
            ::Conditions::INumericConditionComponent *_demeanor{nullptr};
        };

        // -------------------------------------------------------------------
        // TrueGaze_GetPresenceTier: true once the player's PresenceTier >= min
        // -------------------------------------------------------------------
        class GetPresenceTierCondition : public ::Conditions::CustomCondition
        {
        public:
            static constexpr std::string_view CONDITION_NAME = "TrueGaze_GetPresenceTier"sv;

            GetPresenceTierCondition()
            {
                _tier = static_cast<::Conditions::INumericConditionComponent *>(AddBaseComponent(
                    ::Conditions::ConditionComponentType::kNumeric, "Minimum tier",
                    "Minimum PresenceTier required (0=Unnoticed, 1=Noticed, 2=Steady, "
                    "3=Commanding, 4=Legendary)."));
            }

            RE::BSString GetName() const override { return CONDITION_NAME.data(); }

            RE::BSString GetDescription() const override
            {
                return "True when the player's TrueGaze PresenceTier meets or exceeds the threshold."sv
                    .data();
            }

            REL::Version GetRequiredVersion() const override { return kFirstRealVersion; }

            RE::BSString GetArgument() const override
            {
                return std::format("PresenceTier >= {}", _tier->GetArgument().data()).data();
            }

            RE::BSString GetCurrent(RE::TESObjectREFR *a_refr) const override
            {
                OarConditions::ActorState state;
                if (!TryGetState(a_refr, state))
                {
                    return kNoState.data();
                }
                return std::format("{}", state.presenceTier).data();
            }

        protected:
            bool EvaluateImpl(RE::TESObjectREFR *a_refr,
                              [[maybe_unused]] RE::hkbClipGenerator *a_clipGenerator,
                              [[maybe_unused]] void *a_subMod) const override
            {
                uint8_t minTier = 0;
                if (!a_refr || !ToIndex(_tier->GetNumericValue(a_refr), 4, minTier))
                {
                    return false;
                }
                return OarConditions::EvaluatePresenceTier(a_refr->GetFormID(), minTier);
            }

        private:
            ::Conditions::INumericConditionComponent *_tier{nullptr};
        };

        // -------------------------------------------------------------------
        // TrueGaze_GetImpression: true when dominant GazeLedger impression >= min
        // -------------------------------------------------------------------
        class GetImpressionCondition : public ::Conditions::CustomCondition
        {
        public:
            static constexpr std::string_view CONDITION_NAME = "TrueGaze_GetImpression"sv;

            GetImpressionCondition()
            {
                _threshold =
                    static_cast<::Conditions::INumericConditionComponent *>(AddBaseComponent(
                        ::Conditions::ConditionComponentType::kNumeric, "Minimum impression",
                        "Signed dominant GazeLedger impression threshold (-100 to +100)."));
            }

            RE::BSString GetName() const override { return CONDITION_NAME.data(); }

            RE::BSString GetDescription() const override
            {
                return "True when this actor's dominant TrueGaze GazeLedger impression meets or exceeds threshold."sv
                    .data();
            }

            REL::Version GetRequiredVersion() const override { return kFirstRealVersion; }

            RE::BSString GetArgument() const override
            {
                return std::format("Impression >= {}", _threshold->GetArgument().data()).data();
            }

            RE::BSString GetCurrent(RE::TESObjectREFR *a_refr) const override
            {
                OarConditions::ActorState state;
                if (!TryGetState(a_refr, state))
                {
                    return kNoState.data();
                }
                return std::format("{:.1f}", state.dominantImpression).data();
            }

        protected:
            bool EvaluateImpl(RE::TESObjectREFR *a_refr,
                              [[maybe_unused]] RE::hkbClipGenerator *a_clipGenerator,
                              [[maybe_unused]] void *a_subMod) const override
            {
                if (!a_refr)
                {
                    return false;
                }
                const float threshold = _threshold->GetNumericValue(a_refr);
                if (!std::isfinite(threshold))
                {
                    return false;
                }
                return OarConditions::EvaluateImpression(a_refr->GetFormID(), threshold);
            }

        private:
            ::Conditions::INumericConditionComponent *_threshold{nullptr};
        };

        /// Registers one condition and logs OAR's answer.
        template <class T>
        bool Register()
        {
            using OAR_API::Conditions::APIResult;
            switch (OAR_API::Conditions::AddCustomCondition<T>())
            {
            case APIResult::OK:
                logger::info("[TrueGaze] OAR accepted condition {}.", T::CONDITION_NAME);
                return true;
            case APIResult::AlreadyRegistered:
                logger::warn("[TrueGaze] OAR rejected condition {}: a condition with that name is "
                             "already registered.",
                             T::CONDITION_NAME);
                return false;
            case APIResult::Invalid:
                logger::error("[TrueGaze] OAR rejected condition {}: invalid arguments.", T::CONDITION_NAME);
                return false;
            case APIResult::Failed:
            default:
                logger::error("[TrueGaze] OAR failed to register condition {}.", T::CONDITION_NAME);
                return false;
            }
        }

    } // namespace

    bool RegisterAll()
    {
        if (!OAR_API::Conditions::GetAPI(OAR_API::Conditions::InterfaceVersion::V3))
        {
            logger::warn("[TrueGaze] OpenAnimationReplacer.dll is loaded but did not provide the "
                         "Conditions API (interface V3); it may be too old. TrueGaze OAR conditions "
                         "were NOT registered, so OAR rules that use them will not fire.");
            return false;
        }

        int accepted = 0;
        accepted += Register<IsModeCondition>() ? 1 : 0;
        accepted += Register<IsMutualGazeCondition>() ? 1 : 0;
        accepted += Register<GetGazeRegionCondition>() ? 1 : 0;
        accepted += Register<GetInterpretationCondition>() ? 1 : 0;
        accepted += Register<GetDemeanorCondition>() ? 1 : 0;
        accepted += Register<GetPresenceTierCondition>() ? 1 : 0;
        accepted += Register<GetImpressionCondition>() ? 1 : 0;

        if (accepted == 7)
        {
            logger::info("[TrueGaze] OAR integration active: 7/7 conditions registered "
                         "(TrueGaze_IsMode, TrueGaze_IsMutualGaze, TrueGaze_GetGazeRegion, "
                         "TrueGaze_GetInterpretation, TrueGaze_GetDemeanor, "
                         "TrueGaze_GetPresenceTier, TrueGaze_GetImpression).");
            return true;
        }

        logger::warn("[TrueGaze] OAR integration incomplete: {}/7 conditions registered. OAR rules "
                     "that use the rejected conditions will not fire.",
                     accepted);
        return false;
    }

} // namespace TrueGaze::Integrations::OarCustomConditions
