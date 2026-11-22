#include "ExpandedPlay/GazeReticle.hpp"

#include "Engine/ConfigManager.hpp"
#include "Engine/TraceLogger.hpp"
#include "ExpandedPlay/BookStudyTracker.hpp"
#include "PCH.h"
#include "Visuals/IncomingGazeManager.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <mutex>
#include <numbers>
#include <string>

namespace TrueGaze::ExpandedPlay
{
    namespace
    {
#if __has_include(<RE/Skyrim.h>)
        constexpr const char* kReticleMenu = "TrueGazeReticle";
        constexpr float kWhisperLifetimeSec = 4.2f;
        constexpr float kAwakeningPulseLifetimeSec = 1.5f;

        std::mutex g_reticleMutex;
        ReticleHudSnapshot g_snapshot{};
        ReciprocityPulseState g_pulseState{};
        std::string g_whisperText{};
        uint32_t g_whisperColor{0xE6D8B8u};
        float g_whisperRemainingSec{0.0f};
        bool g_whisperDirty{false};
        float g_awakeningPulseRemainingSec{0.0f};
        uint32_t g_awakeningPulseColor{0xFFD966u};
        bool g_errorReported{false};
        bool g_lastMenuShown{false};
        bool g_menuSinkInstalled{false};

        [[nodiscard]] const char* ReticleSuppressionReason() noexcept
        {
            if (Visuals::IncomingGazeManager::DiagnosticsSuppressed())
            {
                return "DiagnosticMenuSuppressed";
            }
            auto* ui = RE::UI::GetSingleton();
            if (!ui)
            {
                return "NoUI";
            }
            if (!ui->IsMenuOpen(RE::HUDMenu::MENU_NAME))
            {
                return "HUDClosed";
            }
            if (ui->GameIsPaused())
            {
                return "GamePaused";
            }
            if (ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME))
            {
                return "DialogueMenu";
            }
            if (ui->IsMenuOpen(RE::Console::MENU_NAME))
            {
                return "Console";
            }
            if (ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME) ||
                ui->IsMenuOpen(RE::GiftMenu::MENU_NAME) ||
                ui->IsMenuOpen(RE::TrainingMenu::MENU_NAME) ||
                ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME) ||
                ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME) ||
                ui->IsMenuOpen(RE::SleepWaitMenu::MENU_NAME) ||
                ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME))
            {
                return "InteractiveMenu";
            }
            return nullptr;
        }

        class ReticleMenuVisibilitySink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(
                const RE::MenuOpenCloseEvent* a_event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                if (!a_event || a_event->menuName == kReticleMenu)
                {
                    return RE::BSEventNotifyControl::kContinue;
                }
                if (const char* reason = ReticleSuppressionReason(); reason != nullptr)
                {
                    if (BookStudyTracker::Get().IsBookOpen())
                    {
                        // Keep GazeReticleMenu alive (with cleared canvas & hidden text) while
                        // BookMenu is open so AdvanceMovie safely drives BookStudyTracker::TickLiveStudy().
                        return RE::BSEventNotifyControl::kContinue;
                    }
                    if (auto* ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(kReticleMenu))
                    {
                        if (auto* queue = RE::UIMessageQueue::GetSingleton())
                        {
                            queue->AddMessage(RE::BSFixedString(kReticleMenu),
                                              RE::UI_MESSAGE_TYPE::kHide, nullptr);
                        }
                        if (g_lastMenuShown)
                        {
                            g_lastMenuShown = false;
                            Engine::TraceLogger::Get().LogExpandedPlayEvent(
                                Engine::TraceLogger::Get().DurationSeconds(), "RETICLE_MENU", 0u,
                                "", 0u, 0.0f, reason);
                            logger::info("[TrueGaze::ExpandedPlay] Reticle menu: {}", reason);
                        }
                    }
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        ReticleMenuVisibilitySink g_reticleMenuSink;

        void ReportOnce(const char* op) noexcept
        {
            if (!g_errorReported)
            {
                g_errorReported = true;
                logger::error("[TrueGaze::ExpandedPlay] GazeReticle {} failed.", op);
            }
        }

        void SetLineStyle(RE::GFxValue& canvas, float thickness, uint32_t colorRgb,
                          float alphaPct) noexcept
        {
            std::array<RE::GFxValue, 3> args{
                RE::GFxValue(static_cast<double>(thickness)),
                RE::GFxValue(static_cast<double>(colorRgb & 0x00FFFFFFu)),
                RE::GFxValue(static_cast<double>(std::clamp(alphaPct, 0.0f, 100.0f)))};
            canvas.Invoke("lineStyle", nullptr, args.data(), args.size());
        }

        void MoveTo(RE::GFxValue& canvas, float x, float y) noexcept
        {
            std::array<RE::GFxValue, 2> pts{RE::GFxValue(static_cast<double>(x)),
                                            RE::GFxValue(static_cast<double>(y))};
            canvas.Invoke("moveTo", nullptr, pts.data(), pts.size());
        }

        void LineTo(RE::GFxValue& canvas, float x, float y) noexcept
        {
            std::array<RE::GFxValue, 2> pts{RE::GFxValue(static_cast<double>(x)),
                                            RE::GFxValue(static_cast<double>(y))};
            canvas.Invoke("lineTo", nullptr, pts.data(), pts.size());
        }

        void DrawArc(RE::GFxValue& canvas, float cx, float cy, float radius, float startRad,
                     float sweepRad, int segments) noexcept
        {
            if (segments < 1 || sweepRad <= 1e-4f)
            {
                return;
            }
            MoveTo(canvas, cx + radius * std::cos(startRad), cy + radius * std::sin(startRad));
            for (int i = 1; i <= segments; ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(segments);
                const float angle = startRad + sweepRad * t;
                LineTo(canvas, cx + radius * std::cos(angle), cy + radius * std::sin(angle));
            }
        }

        void DrawAccessibilityGlyph(RE::GFxValue& canvas, float gx, float gy, float r,
                                    ReticleGlyphShape shape) noexcept
        {
            switch (shape)
            {
            case ReticleGlyphShape::Diamond:
                MoveTo(canvas, gx, gy - r);
                LineTo(canvas, gx + r, gy);
                LineTo(canvas, gx, gy + r);
                LineTo(canvas, gx - r, gy);
                LineTo(canvas, gx, gy - r);
                break;
            case ReticleGlyphShape::Triangle:
                MoveTo(canvas, gx, gy - r);
                LineTo(canvas, gx + r, gy + r);
                LineTo(canvas, gx - r, gy + r);
                LineTo(canvas, gx, gy - r);
                break;
            case ReticleGlyphShape::Square:
                MoveTo(canvas, gx - r, gy - r);
                LineTo(canvas, gx + r, gy - r);
                LineTo(canvas, gx + r, gy + r);
                LineTo(canvas, gx - r, gy + r);
                LineTo(canvas, gx - r, gy - r);
                break;
            case ReticleGlyphShape::Circle:
            default:
                DrawArc(canvas, gx, gy, r, 0.0f, 2.0f * std::numbers::pi_v<float>, 10);
                break;
            }
        }

        class GazeReticleMenu final : public RE::IMenu
        {
        public:
            static RE::IMenu* Create() { return new GazeReticleMenu(); }

            GazeReticleMenu()
            {
                // Passive HUD overlay strictly beneath HUDMenu (depth 2), DialogueMenu (3),
                // and CursorMenu (13). NEVER set kDontHideCursorWhenTopmost (which increments
                // UI::numDontHideCursorWhenTopmost like DialogueMenu/CraftingMenu and suppresses
                // Skyrim's world crosshair activation prompts on doors/NPCs/containers).
                menuFlags.set(RE::UI_MENU_FLAGS::kAlwaysOpen,
                              RE::UI_MENU_FLAGS::kRequiresUpdate,
                              RE::UI_MENU_FLAGS::kAllowSaving);
                depthPriority = 0;
                inputContext = Context::kNone;

                auto* manager = RE::BSScaleformManager::GetSingleton();
                if (!manager || !manager->LoadMovie(this, uiMovie, kReticleMenu))
                {
                    ReportOnce("movie load");
                    return;
                }

                const auto makeFormat = [&](RE::GFxValue& fmt, const char* font, double size,
                                            const char* align)
                {
                    uiMovie->CreateObject(&fmt, "TextFormat");
                    return fmt.IsObject() && fmt.SetMember("font", RE::GFxValue(font)) &&
                           fmt.SetMember("size", RE::GFxValue(size)) &&
                           fmt.SetMember("align", RE::GFxValue(align)) &&
                           fmt.SetMember("bold", RE::GFxValue(false));
                };

                if (!makeFormat(_demeanorFmt, "$EverywhereFont", 14.0, "right") ||
                    !makeFormat(_whisperFmt, "$EverywhereMediumFont", 16.0, "center"))
                {
                    ReportOnce("TextFormat creation");
                    return;
                }

                _ready = true;
            }

            void AdvanceMovie(float a_interval, std::uint32_t a_currentTime) override
            {
                RE::IMenu::AdvanceMovie(a_interval, a_currentTime);
                BookStudyTracker::Get().TickLiveStudy();
                if (!uiMovie || !_ready)
                {
                    return;
                }

                const float dt =
                    (std::isfinite(a_interval) && a_interval > 0.0f)
                        ? std::min(a_interval, 0.1f)
                        : 0.016f;

                ReticleHudSnapshot snap{};
                float pulse01 = 0.0f;
                float awakeningPulse01 = 0.0f;
                uint32_t awakeningPulseColor = 0xFFD966u;
                std::string whisperCopy;
                uint32_t whisperColor = 0xE6D8B8u;
                float whisperRem = 0.0f;
                bool whisperDirty = false;
                {
                    std::lock_guard lock(g_reticleMutex);
                    snap = g_snapshot;
                    pulse01 = g_pulseState.Step(snap.enabled && snap.targetValid && snap.isMutualGaze,
                                                dt);
                    if (g_whisperRemainingSec > 0.0f)
                    {
                        g_whisperRemainingSec = std::max(0.0f, g_whisperRemainingSec - dt);
                    }
                    if (g_awakeningPulseRemainingSec > 0.0f)
                    {
                        g_awakeningPulseRemainingSec =
                            std::max(0.0f, g_awakeningPulseRemainingSec - dt);
                    }
                    awakeningPulse01 =
                        std::clamp(g_awakeningPulseRemainingSec / kAwakeningPulseLifetimeSec,
                                   0.0f, 1.0f);
                    awakeningPulseColor = g_awakeningPulseColor;
                    whisperCopy = g_whisperText;
                    whisperColor = g_whisperColor;
                    whisperRem = g_whisperRemainingSec;
                    whisperDirty = g_whisperDirty;
                    g_whisperDirty = false;
                }

                RE::GFxValue root;
                if (!uiMovie->GetVariable(&root, "_root"))
                {
                    return;
                }

                if (!EnsureCanvas(root) || !EnsureTextFields(root))
                {
                    return;
                }

                _canvas.Invoke("clear", nullptr, nullptr, 0);

                const auto& cfg = Engine::ConfigManager::GetSingleton();
                const bool hasFeedback =
                    (awakeningPulse01 > 0.01f) ||
                    (whisperRem > 0.0f && cfg.enableImpressionWhispers && !whisperCopy.empty());
                const bool visible = (snap.enabled || hasFeedback) && cfg.expandedPlayEnabled &&
                                     (ReticleSuppressionReason() == nullptr);

                if (!visible)
                {
                    HideText(_demeanorField);
                    HideText(_whisperField);
                    return;
                }

                // 0. Awakening / Milestone halo pulse (dual expanding rings + soft golden rim)
                if (awakeningPulse01 > 0.01f)
                {
                    const float progress = 1.0f - awakeningPulse01;
                    const float easeOut = 1.0f - (1.0f - progress) * (1.0f - progress);
                    constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;

                    const float outerR = 26.0f + easeOut * 110.0f;
                    const float innerR = 16.0f + easeOut * 66.0f;
                    SetLineStyle(_canvas, 1.5f + 3.0f * awakeningPulse01, awakeningPulseColor,
                                 awakeningPulse01 * 80.0f);
                    DrawArc(_canvas, 640.0f, 360.0f, outerR, 0.0f, kTwoPi, 24);

                    SetLineStyle(_canvas, 1.0f + 1.8f * awakeningPulse01, awakeningPulseColor,
                                 awakeningPulse01 * 55.0f);
                    DrawArc(_canvas, 640.0f, 360.0f, innerR, 0.0f, kTwoPi, 20);

                    // Subtle screen-edge awakening shimmer
                    SetLineStyle(_canvas, 10.0f, awakeningPulseColor, awakeningPulse01 * 28.0f);
                    MoveTo(_canvas, 6.0f, 6.0f);
                    LineTo(_canvas, 1274.0f, 6.0f);
                    LineTo(_canvas, 1274.0f, 714.0f);
                    LineTo(_canvas, 6.0f, 714.0f);
                    LineTo(_canvas, 6.0f, 6.0f);
                }

                // 1. Optional staring-contest frost perimeter vignette
                if (snap.enabled && snap.frostVignetteAlpha > 0.01f)
                {
                    const float borderAlpha = std::clamp(snap.frostVignetteAlpha, 0.0f, 1.0f) * 35.0f;
                    SetLineStyle(_canvas, 14.0f, 0x99D9FFu, borderAlpha);
                    MoveTo(_canvas, 8.0f, 8.0f);
                    LineTo(_canvas, 1272.0f, 8.0f);
                    LineTo(_canvas, 1272.0f, 712.0f);
                    LineTo(_canvas, 8.0f, 712.0f);
                    LineTo(_canvas, 8.0f, 8.0f);
                }

                // 2. First-person HCEP-02 Gaze Reticle, Dwell Ring, Pulse & Glyph
                if (snap.enabled && !snap.isThirdPerson && snap.showReticle &&
                    !snap.immersiveHideReticle && snap.targetValid)
                {
                    const float cx = 640.0f + snap.magnetismX;
                    const float cy = 360.0f + snap.magnetismY;
                    const uint32_t color = RegionColorRgb(snap.gazeRegionId, snap.colorBlindMode);
                    const float baseAlpha = std::clamp(snap.opacity, 0.0f, 1.0f) * 100.0f;
                    constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
                    constexpr float kTopRad = -0.5f * std::numbers::pi_v<float>;

                    // Outer soft bloom + inner region ring
                    SetLineStyle(_canvas, 5.0f, color, baseAlpha * 0.45f);
                    DrawArc(_canvas, cx, cy, 18.0f, 0.0f, kTwoPi, 16);

                    SetLineStyle(_canvas, 1.8f, color, baseAlpha);
                    DrawArc(_canvas, cx, cy, 14.0f, 0.0f, kTwoPi, 16);

                    // 4 dwell threshold ticks (Glance->Look, Look->Gaze, Gaze->Stare, Stare->Fixation)
                    SetLineStyle(_canvas, 1.0f, color, std::min(70.0f, baseAlpha * 1.8f));
                    for (int t = 0; t < 4; ++t)
                    {
                        const float ang = kTopRad + kTwoPi * (static_cast<float>(t) * 0.25f);
                        MoveTo(_canvas, cx + 20.0f * std::cos(ang), cy + 20.0f * std::sin(ang));
                        LineTo(_canvas, cx + 24.5f * std::cos(ang), cy + 24.5f * std::sin(ang));
                    }

                    // Dwell progress arc
                    const auto arc = ComputeDwellArcState(snap.dwellSec, snap.rapidEscalation);
                    if (arc.progress01 > 0.005f)
                    {
                        const int segs =
                            std::max(2, static_cast<int>(std::ceil(arc.progress01 * 16.0f)));
                        SetLineStyle(_canvas, 2.2f, color, std::min(90.0f, baseAlpha * 3.0f));
                        DrawArc(_canvas, cx, cy, 22.0f, kTopRad, arc.progress01 * kTwoPi, segs);
                    }

                    // Accessibility shape glyph
                    SetLineStyle(_canvas, 1.4f, color, std::min(85.0f, baseAlpha * 2.8f));
                    DrawAccessibilityGlyph(_canvas, cx + 28.0f, cy - 28.0f, 4.0f,
                                           RegionGlyphShape(snap.gazeRegionId));

                    // Reciprocity pulse ring on mutual gaze onset
                    if (pulse01 > 0.01f)
                    {
                        const float pulseRadius = 16.0f + (1.0f - pulse01) * 24.0f;
                        SetLineStyle(_canvas, 0.5f + 2.0f * pulse01, color, pulse01 * 75.0f);
                        DrawArc(_canvas, cx, cy, pulseRadius, 0.0f, kTwoPi, 16);
                    }
                }

                // 3. Third-person Demeanor indicator (also shown in 1st-person while pinned)
                if (snap.enabled && (snap.isThirdPerson || snap.demeanorPinned))
                {
                    std::string demeanorLine;
                    if (snap.demeanorPinned)
                    {
                        const int secs = std::max(1, static_cast<int>(std::ceil(snap.demeanorPinRemainingSec)));
                        demeanorLine = std::format("Demeanor: {} [Pinned {}s]",
                                                   DemeanorName(snap.activeDemeanor), secs);
                    }
                    else
                    {
                        demeanorLine = std::format("Demeanor: {} [{}]",
                                                   DemeanorName(snap.activeDemeanor),
                                                   DemeanorModeName(snap.demeanorMode));
                    }
                    const uint32_t dColor = snap.demeanorPinned ? 0xFFD966u : 0xE6D8B8u;
                    ShowText(_demeanorField, _demeanorFmt, demeanorLine,
                             demeanorLine != _lastDemeanorText, 950.0, 26.0, 300.0, dColor, 85.0);
                    _lastDemeanorText = std::move(demeanorLine);
                }
                else
                {
                    HideText(_demeanorField);
                }

                // 4. Telltale-style impression whisper
                if (whisperRem > 0.0f && cfg.enableImpressionWhispers && !whisperCopy.empty())
                {
                    const float elapsed = kWhisperLifetimeSec - whisperRem;
                    float alpha01 = 1.0f;
                    if (elapsed < 0.25f)
                    {
                        alpha01 = elapsed / 0.25f;
                    }
                    else if (whisperRem < 0.60f)
                    {
                        alpha01 = whisperRem / 0.60f;
                    }
                    ShowText(_whisperField, _whisperFmt, whisperCopy,
                             whisperDirty || whisperCopy != _lastWhisperText, 340.0, 580.0, 600.0,
                             whisperColor, std::clamp(alpha01, 0.0f, 1.0f) * 90.0f);
                    _lastWhisperText = std::move(whisperCopy);
                }
                else
                {
                    HideText(_whisperField);
                }
            }

        private:
            bool EnsureCanvas(RE::GFxValue& root) noexcept
            {
                if (_canvas.IsDisplayObject())
                {
                    return true;
                }
                std::array<RE::GFxValue, 2> args{RE::GFxValue("reticleCanvas"), RE::GFxValue(0.0)};
                return root.Invoke("createEmptyMovieClip", nullptr, args.data(), args.size()) &&
                       root.GetMember("reticleCanvas", &_canvas);
            }

            bool EnsureTextField(RE::GFxValue& root, RE::GFxValue& field, const char* name,
                                 double depth, double x, double y, double w, double h) noexcept
            {
                if (field.IsDisplayObject())
                {
                    return true;
                }
                std::array<RE::GFxValue, 6> args{RE::GFxValue(name), RE::GFxValue(depth),
                                                 RE::GFxValue(x), RE::GFxValue(y), RE::GFxValue(w),
                                                 RE::GFxValue(h)};
                if (!root.Invoke("createTextField", nullptr, args.data(), args.size()) ||
                    !root.GetMember(name, &field) ||
                    !field.SetMember("selectable", RE::GFxValue(false)) ||
                    !field.SetMember("background", RE::GFxValue(false)) ||
                    !field.SetMember("embedFonts", RE::GFxValue(true)))
                {
                    return false;
                }
                HideText(field);
                return true;
            }

            bool EnsureTextFields(RE::GFxValue& root) noexcept
            {
                return EnsureTextField(root, _demeanorField, "demeanorTxt", 1.0, 950.0, 26.0,
                                       300.0, 26.0) &&
                       EnsureTextField(root, _whisperField, "whisperTxt", 2.0, 340.0, 580.0, 600.0,
                                       30.0);
            }

            static void HideText(RE::GFxValue& field) noexcept
            {
                if (!field.IsDisplayObject())
                {
                    return;
                }
                RE::GFxValue::DisplayInfo info;
                info.SetVisible(false);
                field.SetDisplayInfo(info);
            }

            static void ShowText(RE::GFxValue& field, RE::GFxValue& fmt, const std::string& text,
                                 bool textChanged, double x, double y, double w, uint32_t colorRgb,
                                 double alphaPct) noexcept
            {
                if (textChanged)
                {
                    field.SetMember("_width", RE::GFxValue(w));
                    field.Invoke("setNewTextFormat", nullptr, &fmt, 1);
                    field.SetText(text.c_str());
                    field.Invoke("setTextFormat", nullptr, &fmt, 1);
                }
                field.SetMember("textColor", RE::GFxValue(static_cast<double>(colorRgb & 0x00FFFFFFu)));
                RE::GFxValue::DisplayInfo info;
                info.SetVisible(true);
                info.SetPosition(x, y);
                info.SetAlpha(alphaPct);
                field.SetDisplayInfo(info);
            }

            bool _ready{false};
            RE::GFxValue _canvas{};
            RE::GFxValue _demeanorFmt{};
            RE::GFxValue _whisperFmt{};
            RE::GFxValue _demeanorField{};
            RE::GFxValue _whisperField{};
            std::string _lastDemeanorText{};
            std::string _lastWhisperText{};
        };
#endif
    } // namespace

    GazeReticle& GazeReticle::Get() noexcept
    {
        static GazeReticle instance;
        return instance;
    }

    void GazeReticle::InstallUi() noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (auto* ui = RE::UI::GetSingleton())
        {
            ui->Register(kReticleMenu, GazeReticleMenu::Create);
            if (!g_menuSinkInstalled)
            {
                ui->AddEventSink<RE::MenuOpenCloseEvent>(&g_reticleMenuSink);
                g_menuSinkInstalled = true;
            }
            logger::info("[TrueGaze::ExpandedPlay] Registered GazeReticle HUD menu.");
        }
#endif
    }

    void GazeReticle::Reset() noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        {
            std::lock_guard lock(g_reticleMutex);
            g_snapshot = {};
            g_pulseState = {};
            g_whisperText.clear();
            g_whisperRemainingSec = 0.0f;
            g_whisperDirty = false;
            g_awakeningPulseRemainingSec = 0.0f;
        }
        g_lastMenuShown = false;
        if (auto* queue = RE::UIMessageQueue::GetSingleton())
        {
            if (auto* ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(kReticleMenu))
            {
                queue->AddMessage(RE::BSFixedString(kReticleMenu), RE::UI_MESSAGE_TYPE::kHide,
                                  nullptr);
            }
        }
#endif
    }

    void GazeReticle::UpdateFrame(const ReticleHudSnapshot& snapshot) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        bool hasActiveFeedback = false;
        {
            std::lock_guard lock(g_reticleMutex);
            g_snapshot = snapshot;
            hasActiveFeedback =
                (g_awakeningPulseRemainingSec > 0.0f) ||
                (g_whisperRemainingSec > 0.0f && !g_whisperText.empty());
        }

        const bool bookOpen = BookStudyTracker::Get().IsBookOpen();
        const bool wantMenu =
            snapshot.enabled || bookOpen ||
            (hasActiveFeedback && Engine::ConfigManager::GetSingleton().expandedPlayEnabled);
        const char* suppressReason = ReticleSuppressionReason();
        const bool suppressed = (suppressReason != nullptr) && !bookOpen;
        if (auto* queue = RE::UIMessageQueue::GetSingleton())
        {
            if (auto* ui = RE::UI::GetSingleton())
            {
                const bool isOpen = ui->IsMenuOpen(kReticleMenu);
                if (wantMenu && !suppressed && !isOpen)
                {
                    queue->AddMessage(RE::BSFixedString(kReticleMenu), RE::UI_MESSAGE_TYPE::kShow,
                                      nullptr);
                    if (!g_lastMenuShown)
                    {
                        g_lastMenuShown = true;
                        const std::string detail = std::format(
                            "show dontHideCursor={} hudOpen={}",
                            ui->numDontHideCursorWhenTopmost,
                            ui->IsMenuOpen(RE::HUDMenu::MENU_NAME) ? "yes" : "no");
                        Engine::TraceLogger::Get().LogExpandedPlayEvent(
                            Engine::TraceLogger::Get().DurationSeconds(), "RETICLE_MENU", 0u, "",
                            snapshot.gazeRegionId, snapshot.dwellSec, detail.c_str());
                        logger::info("[TrueGaze::ExpandedPlay] Reticle menu: {}", detail);
                    }
                }
                else if ((!wantMenu || suppressed) && isOpen)
                {
                    queue->AddMessage(RE::BSFixedString(kReticleMenu), RE::UI_MESSAGE_TYPE::kHide,
                                      nullptr);
                    if (g_lastMenuShown)
                    {
                        g_lastMenuShown = false;
                        const char* reason = !wantMenu ? "hide:disabled" : suppressReason;
                        Engine::TraceLogger::Get().LogExpandedPlayEvent(
                            Engine::TraceLogger::Get().DurationSeconds(), "RETICLE_MENU", 0u, "",
                            snapshot.gazeRegionId, snapshot.dwellSec, reason);
                        logger::info("[TrueGaze::ExpandedPlay] Reticle menu: {}", reason);
                    }
                }
            }
        }
#else
        (void)snapshot;
#endif
    }

    void GazeReticle::PushWhisper(std::string_view text, uint32_t colorRgb) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (text.empty())
        {
            return;
        }
        std::lock_guard lock(g_reticleMutex);
        g_whisperText.assign(text);
        g_whisperColor = colorRgb & 0x00FFFFFFu;
        g_whisperRemainingSec = kWhisperLifetimeSec;
        g_whisperDirty = true;
#else
        (void)text;
        (void)colorRgb;
#endif
    }

    void GazeReticle::TriggerAwakeningPulse(uint32_t colorRgb) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        std::lock_guard lock(g_reticleMutex);
        g_awakeningPulseColor = colorRgb & 0x00FFFFFFu;
        g_awakeningPulseRemainingSec = kAwakeningPulseLifetimeSec;
#else
        (void)colorRgb;
#endif
    }

    void GazeReticle::NotifyMilestone(std::string_view text, uint32_t colorRgb,
                                      const char* soundEditorId, bool triggerPulse) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (text.empty())
        {
            return;
        }
        const std::string msg(text);
        // Native Skyrim top-left HUD banner + verified vanilla Skyrim.esm sound descriptor.
        // Works reliably even at Stage 0 (Dormant) or immediately upon closing BookMenu.
        RE::SendHUDMessage::ShowHUDMessage(msg.c_str(), soundEditorId, true);

        if (Engine::ConfigManager::GetSingleton().enableImpressionWhispers)
        {
            PushWhisper(msg, colorRgb);
        }
        if (triggerPulse)
        {
            TriggerAwakeningPulse(colorRgb);
        }
#else
        (void)text;
        (void)colorRgb;
        (void)soundEditorId;
        (void)triggerPulse;
#endif
    }
} // namespace TrueGaze::ExpandedPlay
