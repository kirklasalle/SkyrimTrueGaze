#include "Visuals/IncomingGazeManager.hpp"
#include "Visuals/IncomingUi.hpp"
#include "Visuals/DiagnosticMenuPolicy.hpp"
#include "Visuals/VisualEffectsManager.hpp"
#include "Engine/GazeAnchors.hpp"
#include "Engine/TraceLogger.hpp"

#include <mutex>
#include <format>
#include <unordered_map>
#include <map>
#include <atomic>
#include <chrono>

namespace TrueGaze::Visuals
{
    namespace
    {
        namespace I = Incoming;
        namespace G = Geometry;
        constexpr const char* kMenu = "TrueGazeIncoming";
        constexpr float kAspect = 2760.0f / 1504.0f;
        G::Vec3 Vec(const RE::NiPoint3& p) { return {p.x, p.y, p.z}; }
        RE::NiPoint3 Ni(G::Vec3 p) { return {p.x, p.y, p.z}; }

        struct WorldReceiver
        {
            RE::ActorHandle actor{};
            RE::NiPointer<RE::NiNode> parent{};
            RE::NiPointer<RE::NiNode> above{};
            RE::NiPointer<RE::NiNode> front{};
            uint64_t frame{0};
            uint64_t retryFrame{0};
        };
        struct Label
        {
            float x{0}, y{0};
            uint32_t color{0xFFFFFF};
            std::string text;
            bool pointMarker{false};
            unsigned style{0};
        };
        struct Card
        {
            uint32_t id{0};
            float x{0}, y{0};
            bool onScreen{false}, active{false}, hud{false};
            std::vector<Label> rows;
        };
        struct UiSnapshot
        {
            std::vector<Label> labels;
            std::vector<Card> cards;
            std::vector<IncomingUi::TextRect> faces;
            struct InspectionEntry { uint32_t target, observer; std::string text; };
            std::vector<InspectionEntry> inspection;
            bool inspect{false};
            uint64_t frame{0};
            uint64_t menuState{0};
            bool visible{false};
        };

        I::Frame g_frame;
        uint64_t g_frameId{0};
        uint64_t g_frameMenuState{0};
        DiagnosticMenuPolicy g_menuPolicy;
        VisualTuning g_tuning{};
        std::unordered_map<uint32_t, WorldReceiver> g_receivers;
        uint32_t g_receiverOverflow{0};
        std::atomic<uint64_t> g_failures{0};
        std::mutex g_uiMutex;
        UiSnapshot g_ui;
        IncomingGazeManager::Cost g_cost{};
        std::atomic<uint64_t> g_uiUs{0}, g_peakUiUs{0};
        std::atomic<size_t> g_placedCards{0}, g_overflowCards{0}, g_labelCount{0};
        std::atomic<int> g_inspectionStep{0};
        std::mutex g_timingMutex;
        IncomingUi::TimingWindow g_finalizeTiming, g_uiTiming;

        uint64_t ElapsedUs(std::chrono::steady_clock::time_point start)
        {
            return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start).count());
        }

        void Report(const char* operation)
        {
            const auto count = g_failures.fetch_add(1, std::memory_order_relaxed) + 1;
            if (count <= 5)
                logger::error("[TrueGaze::Incoming] {} failed; failures={}", operation, count);
        }

        class DiagnosticMenuEvents final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                if (!event || !g_menuPolicy.SetMenu(event->menuName.c_str(), event->opening))
                    return RE::BSEventNotifyControl::kContinue;
                if (event->opening)
                {
                    auto* tasks = SKSE::GetTaskInterface();
                    if (!tasks)
                        Report("diagnostic menu cleanup task interface");
                    else
                        tasks->AddTask([queuedAt = std::chrono::steady_clock::now()] {
                            if (!IncomingGazeManager::DiagnosticsSuppressed())
                                return;
                            // Scene ownership stays on the game thread; never
                            // touch skeleton nodes from a menu/UI callback.
                            auto& visuals = VisualEffectsManager::Get();
                            if (visuals.ActiveActorCount() || IncomingGazeManager::Get().GetCost().panels)
                            {
                                const auto queueDelay = ElapsedUs(queuedAt);
                                const auto start = std::chrono::steady_clock::now();
                                visuals.Reset();
                                logger::info("[TrueGaze::Incoming] Menu diagnostics cleanup: queued {}us, cleared {}us.",
                                             queueDelay, ElapsedUs(start));
                            }
                        });
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        std::string ActorLabel(RE::Actor* actor, uint32_t id)
        {
            if (!actor)
            {
                Report("actor display-name lookup");
                return std::format("Unavailable actor [{:08X}]", id);
            }
            if (actor->IsPlayerRef())
                return "You";
            const char* name = actor->GetDisplayFullName();
            return name && *name ? std::string(name) : std::format("Unnamed actor [{:08X}]", id);
        }

        void Detach(RE::NiPointer<RE::NiNode>& panel, RE::NiNode* parent)
        {
            if (panel && parent)
                parent->DetachChild(panel.get());
            panel = nullptr;
        }

        RE::NiAVObject* Find(RE::NiAVObject* root, std::initializer_list<const char*> names)
        {
            for (const auto* name : names)
                if (auto* found = root->GetObjectByName(RE::BSFixedString(name)))
                    return found;
            return nullptr;
        }

        RE::NiMatrix3 Basis(const I::Panel& panel)
        {
            RE::NiMatrix3 basis{};
            const auto right = panel.right * kAspect;
            for (int row = 0; row < 3; ++row)
            {
                basis.entry[row][0] = row == 0 ? right.x : row == 1 ? right.y : right.z;
                basis.entry[row][1] = row == 0 ? panel.normal.x : row == 1 ? panel.normal.y : panel.normal.z;
                basis.entry[row][2] = row == 0 ? panel.up.x : row == 1 ? panel.up.y : panel.up.z;
            }
            return basis;
        }

        void PlacePanel(RE::NiPointer<RE::NiNode>& node, WorldReceiver& owner, const I::Panel& panel)
        {
            if (!panel.enabled)
            {
                Detach(node, owner.parent.get());
                return;
            }
            if (!node && g_frameId >= owner.retryFrame)
            {
                node = VisualEffectsManager::Get().CreateReceivingPanel();
                if (node)
                    owner.parent->AttachChild(node.get(), false);
                else
                    owner.retryFrame = g_frameId + 300;
            }
            if (!node)
                return;
            RE::NiTransform world{};
            world.translate = Ni(panel.center);
            world.rotate = Basis(panel);
            world.scale = (panel.halfHeight * 2.0f) / 512.0f;
            node->local = owner.parent->world.Invert() * world;
            node->world = owner.parent->world * node->local;
            RE::NiUpdateData update;
            update.time = 0;
            update.flags = RE::NiUpdateData::Flag::kDirty;
            node->UpdateDownwardPass(update, 0);
        }

        bool Project(RE::NiCamera* camera, G::Vec3 position, float& x, float& y)
        {
            float z = 0;
            if (!camera || !I::Finite(position) ||
                G::Dot(position - Vec(camera->world.translate),
                       Vec(camera->world.rotate.GetVectorY()) * -1) <= 0 ||
                !camera->WorldPtToScreenPt3(Ni(position), x, y, z, 0.0001f) ||
                !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
                x < 0 || x > 1 || y < 0 || y > 1)
                return false;
            x *= 1280;
            y = (1 - y) * 720;
            return true;
        }

        uint32_t ObserverColor(uint32_t id)
        {
            constexpr uint32_t colors[] = {0xFFD966, 0x66CCFF, 0xFF99CC, 0x99FF99,
                                           0xDDAAFF, 0xFFAA66, 0xFFFFFF};
            return colors[id % std::size(colors)];
        }

        class IncomingMenu final : public RE::IMenu
        {
        public:
            static RE::IMenu* Create() { return new IncomingMenu(); }

            IncomingMenu()
            {
                menuFlags.set(RE::UI_MENU_FLAGS::kRequiresUpdate);
                depthPriority = 5;
                auto* manager = RE::BSScaleformManager::GetSingleton();
                if (!manager || !manager->LoadMovie(this, uiMovie, kMenu))
                {
                    Report("native UI movie load");
                    return;
                }
                for (unsigned i = 0; i < _formats.size(); ++i)
                {
                    uiMovie->CreateObject(&_formats[i], "TextFormat");
                    if (!_formats[i].IsObject() ||
                        !_formats[i].SetMember("font", RE::GFxValue(
                            i == 1 ? "$EverywhereMediumFont" : "$EverywhereFont")) ||
                        !_formats[i].SetMember("size", RE::GFxValue(i == 1 ? 20.0 : i == 2 ? 14.0 : 17.0)) ||
                        !_formats[i].SetMember("align", RE::GFxValue("left")) ||
                        !_formats[i].SetMember("bold", RE::GFxValue(false)))
                    {
                        Report("native Skyrim card text format");
                        return;
                    }
                }
                _ready = true;
                logger::info("[TrueGaze::Incoming] UI card fonts configured: game library, measured wrapped text.");
            }

            void AdvanceMovie(float interval, uint32_t time) override
            {
                const auto start = std::chrono::steady_clock::now();
                RE::IMenu::AdvanceMovie(interval, time);
                if (!uiMovie || !_ready)
                    return;
                UiSnapshot snapshot;
                {
                    std::lock_guard lock(g_uiMutex);
                    snapshot = g_ui;
                }
                if (!g_menuPolicy.Allows(snapshot.menuState))
                    snapshot.visible = false;
                RE::GFxValue root;
                if (!uiMovie->GetVariable(&root, "_root"))
                {
                    Report("UI root lookup");
                    return;
                }
                RE::GFxValue drawing;
                if (!root.GetMember("leaders", &drawing))
                {
                    std::array<RE::GFxValue, 2> args{RE::GFxValue("leaders"), RE::GFxValue(0.0)};
                    if (!root.Invoke("createEmptyMovieClip", nullptr, args.data(), args.size()) ||
                        !root.GetMember("leaders", &drawing))
                    {
                        Report("ownership leader creation");
                        return;
                    }
                }
                if (!drawing.Invoke("clear", nullptr, nullptr, 0))
                    Report("ownership leader clearing");
                size_t next = 0;
                std::vector<bool> visible;
                const auto show = [&](size_t index, float x, float y, float width, float height, uint32_t color) {
                    if (visible.size() <= index)
                        visible.resize(index + 1);
                    visible[index] = true;
                    RE::GFxValue::DisplayInfo info;
                    info.SetVisible(true);
                    info.SetPosition(x, y);
                    auto& field = _fields[index];
                    const bool resized = !field.geometryValid || field.rect.width != width || field.rect.height != height;
                    const bool recolored = !field.geometryValid || field.color != color;
                    const bool moved = !field.shown || !field.geometryValid || field.rect.x != x || field.rect.y != y;
                    const bool ok =
                        (!resized || (field.value.SetMember("_width", RE::GFxValue(static_cast<double>(width))) &&
                                      field.value.SetMember("_height", RE::GFxValue(static_cast<double>(height))))) &&
                        (!recolored || field.value.SetMember("textColor", RE::GFxValue(static_cast<double>(color)))) &&
                        (!moved || field.value.SetDisplayInfo(info));
                    if (!ok)
                    {
                        Report("native card placement");
                        visible[index] = false;
                        RE::GFxValue::DisplayInfo hidden;
                        hidden.SetVisible(false);
                        if (field.value.SetDisplayInfo(hidden))
                            field.shown = false;
                        else
                            Report("failed card placement hiding");
                        field.geometryValid = false;
                        return;
                    }
                    field.shown = field.geometryValid = true;
                    field.rect = {x, y, width, height};
                    field.color = color;
                };
                const auto line = [&](float x1, float y1, float x2, float y2) {
                    std::array<RE::GFxValue, 2> from{RE::GFxValue(static_cast<double>(x1)), RE::GFxValue(static_cast<double>(y1))};
                    std::array<RE::GFxValue, 2> to{RE::GFxValue(static_cast<double>(x2)), RE::GFxValue(static_cast<double>(y2))};
                    if (!drawing.Invoke("moveTo", nullptr, from.data(), from.size()) ||
                        !drawing.Invoke("lineTo", nullptr, to.data(), to.size()))
                        Report("card path drawing");
                };
                const auto backing = [&](const IncomingUi::TextRect& rect) {
                    std::array<RE::GFxValue, 2> fill{RE::GFxValue(1053720.0), RE::GFxValue(78.0)};
                    if (!drawing.Invoke("beginFill", nullptr, fill.data(), fill.size()))
                        Report("card backing fill");
                    const std::array<std::array<float, 2>, 5> corners{{
                        {rect.x, rect.y}, {rect.x + rect.width, rect.y},
                        {rect.x + rect.width, rect.y + rect.height},
                        {rect.x, rect.y + rect.height}, {rect.x, rect.y}}};
                    for (size_t i = 0; i < corners.size(); ++i)
                    {
                        std::array<RE::GFxValue, 2> point{
                            RE::GFxValue(static_cast<double>(corners[i][0])),
                            RE::GFxValue(static_cast<double>(corners[i][1]))};
                        if (!drawing.Invoke(i ? "lineTo" : "moveTo", nullptr, point.data(), point.size()))
                            Report("card backing path");
                    }
                    if (!drawing.Invoke("endFill", nullptr, nullptr, 0))
                        Report("card backing completion");
                };
                if (snapshot.visible)
                {
                    std::array<RE::GFxValue, 3> style{
                        RE::GFxValue(1.0), RE::GFxValue(16777215.0), RE::GFxValue(80.0)};
                    if (!drawing.Invoke("lineStyle", nullptr, style.data(), style.size()))
                        Report("ownership leader style");
                    std::vector<IncomingUi::TextRect> reserved = snapshot.faces;
                    reserved.push_back({500, 290, 280, 280});
                    if (snapshot.inspect)
                        reserved.push_back({828, 156, 440, 452});
                    std::vector<IncomingUi::CardRequest> requests;
                    std::map<uint32_t, std::vector<size_t>> indices;
                    for (const auto& card : snapshot.cards)
                    {
                        if (!card.onScreen)
                            continue;
                        float height = 16;
                        for (const auto& row : card.rows)
                        {
                            const auto index = next++;
                            if (!Prepare(root, index, row, 300))
                            {
                                height = 1000;
                                break;
                            }
                            indices[card.id].push_back(index);
                            height += _fields[index].height + 4;
                            if (height > 452)
                                break;
                        }
                        requests.push_back({card.id, card.x, card.y, 320, height, card.active});
                    }
                    const auto layout = IncomingUi::LayoutCards(requests, reserved, _previous);
                    _previous.clear();
                    for (const auto& placed : layout.placed)
                    {
                        _previous.emplace(placed.id, placed);
                        const auto card = std::find_if(snapshot.cards.begin(), snapshot.cards.end(),
                            [&](const auto& value) { return value.id == placed.id; });
                        if (!card->hud)
                            line(std::clamp(placed.anchorX, placed.rect.x, placed.rect.x + placed.rect.width),
                                 std::clamp(placed.anchorY, placed.rect.y, placed.rect.y + placed.rect.height),
                                 placed.anchorX, placed.anchorY);
                        backing(placed.rect);
                        float y = placed.rect.y + 8;
                        for (size_t row = 0; row < indices.at(placed.id).size(); ++row)
                        {
                            const auto index = indices.at(placed.id)[row];
                            show(index, placed.rect.x + 10, y, 300, _fields[index].height, card->rows[row].color);
                            y += _fields[index].height + 4;
                        }
                    }
                    g_placedCards.store(layout.placed.size(), std::memory_order_relaxed);
                    g_overflowCards.store(layout.overflow.size(), std::memory_order_relaxed);
                    if (!layout.overflow.empty() || snapshot.inspect)
                    {
                        Label notice{20, 92, 0xFFD966, std::format(
                            "Incoming: {}/{} on-screen cards; {} need inspection | stgintext, stginnext / stginprev",
                            layout.placed.size(), requests.size(), layout.overflow.size()), false, 2};
                        const auto index = next++;
                        if (Prepare(root, index, notice, 780))
                            show(index, notice.x, notice.y, 780, _fields[index].height, notice.color);
                    }
                    for (const auto& label : snapshot.labels)
                    {
                        const auto index = next++;
                        if (Prepare(root, index, label, label.pointMarker ? 32.0f : 780.0f))
                        {
                            const auto& field = _fields[index];
                            const auto rect = IncomingUi::PlaceText(label.x, label.y, field.width, field.height);
                            show(index, label.pointMarker ? label.x - field.width * 0.5f : rect.x,
                                 label.pointMarker ? label.y - field.height * 0.5f : rect.y,
                                 field.width, field.height, label.color);
                        }
                    }
                    if (snapshot.inspect && !snapshot.inspection.empty())
                    {
                        backing({828, 156, 440, 452});
                        const auto old = std::find_if(snapshot.inspection.begin(), snapshot.inspection.end(),
                            [&](const auto& entry) { return entry.target == _target && entry.observer == _observer; });
                        if (old != snapshot.inspection.end())
                            _entry = static_cast<size_t>(old - snapshot.inspection.begin());
                        else
                        {
                            _entry = std::min(_entry, snapshot.inspection.size() - 1);
                            _scroll = 1;
                        }
                        const auto bodyIndex = next++;
                        const auto prepareBody = [&]() {
                            const auto& entry = snapshot.inspection[_entry];
                            Label body{838, 200, 0xFFFFFF, entry.text};
                            if (!Prepare(root, bodyIndex, body, 420) ||
                                !_fields[bodyIndex].value.SetMember("_height", RE::GFxValue(354.0)))
                                return false;
                            RE::GFxValue maximum;
                            if (!_fields[bodyIndex].value.GetMember("maxscroll", &maximum) ||
                                !maximum.IsNumber() || !std::isfinite(maximum.GetNumber()) ||
                                maximum.GetNumber() < 1 || maximum.GetNumber() > INT_MAX)
                                return false;
                            _maxScroll = static_cast<int>(maximum.GetNumber());
                            _scroll = std::clamp(_scroll, 1, _maxScroll);
                            return _fields[bodyIndex].value.SetMember("scroll", RE::GFxValue(static_cast<double>(_scroll)));
                        };
                        bool bodyReady = prepareBody();
                        int64_t steps = g_inspectionStep.exchange(0, std::memory_order_relaxed);
                        while (bodyReady && steps)
                        {
                            RE::GFxValue bottom;
                            if (!_fields[bodyIndex].value.GetMember("bottomScroll", &bottom) ||
                                !bottom.IsNumber() || !std::isfinite(bottom.GetNumber()) ||
                                bottom.GetNumber() < _scroll || bottom.GetNumber() > INT_MAX)
                            {
                                bodyReady = false;
                                break;
                            }
                            const auto lines = std::max(1, static_cast<int>(bottom.GetNumber()) - _scroll + 1);
                            const auto position = IncomingUi::StepInspection(
                                {_entry, _scroll}, _maxScroll, lines, snapshot.inspection.size(), steps > 0);
                            _entry = position.entry;
                            _scroll = position.scroll;
                            steps += steps > 0 ? -1 : 1;
                            bodyReady = prepareBody();
                        }
                        if (bodyReady)
                        {
                            show(bodyIndex, 838, 200, 420, 354, 0xFFFFFF);
                            _target = snapshot.inspection[_entry].target;
                            _observer = snapshot.inspection[_entry].observer;
                        }
                        else
                            Report("native inspection scrolling");
                        Label title{838, 166, 0xFFFFFF, std::format("Incoming inspection | {}/{}",
                            _entry + 1, snapshot.inspection.size()), false, 1};
                        const auto titleIndex = next++;
                        if (Prepare(root, titleIndex, title, 420))
                            show(titleIndex, title.x, title.y, 420, _fields[titleIndex].height, title.color);
                        Label footer{838, 562, 0xCCCCCC, std::format(
                            "Live frame {} | text {}/{}\nstginnext / stginprev: text pages, then entries",
                            snapshot.frame, _scroll, _maxScroll), false, 2};
                        const auto footerIndex = next++;
                        if (Prepare(root, footerIndex, footer, 420))
                            show(footerIndex, footer.x, footer.y, 420, _fields[footerIndex].height, footer.color);
                    }
                    else if (snapshot.inspect)
                    {
                        backing({828, 156, 440, 452});
                        const Label empty{838, 166, 0xFFFFFF, "Incoming inspection\nNo receiving actors in this frame."};
                        const auto index = next++;
                        if (Prepare(root, index, empty, 420))
                            show(index, empty.x, empty.y, 420, _fields[index].height, empty.color);
                        g_inspectionStep.store(0, std::memory_order_relaxed);
                    }
                }
                else
                {
                    _previous.clear();
                    g_placedCards.store(0, std::memory_order_relaxed);
                    g_overflowCards.store(0, std::memory_order_relaxed);
                }
                for (size_t i = 0; i < _fields.size(); ++i)
                    if ((i >= visible.size() || !visible[i]) && _fields[i].shown)
                    {
                        RE::GFxValue::DisplayInfo info;
                        info.SetVisible(false);
                        if (!_fields[i].value.SetDisplayInfo(info))
                            Report("native card hiding");
                        else
                            _fields[i].shown = false;
                    }
                g_labelCount.store(static_cast<size_t>(std::count(visible.begin(), visible.end(), true)),
                                   std::memory_order_relaxed);
                const auto elapsed = ElapsedUs(start);
                if (snapshot.visible && snapshot.frame != _timedFrame)
                {
                    _timedFrame = snapshot.frame;
                    g_uiUs.store(elapsed, std::memory_order_relaxed);
                    g_peakUiUs.store(std::max(elapsed, g_peakUiUs.load(std::memory_order_relaxed)),
                                    std::memory_order_relaxed);
                    std::lock_guard lock(g_timingMutex);
                    g_uiTiming.Add(elapsed);
                }
            }

        private:
            struct Field
            {
                RE::GFxValue value;
                std::string text;
                unsigned style{99};
                float width{0}, height{0}, inputWidth{0};
                bool marker{false}, valid{false};
                bool shown{false}, geometryValid{false};
                IncomingUi::TextRect rect{};
                uint32_t color{0};
            };
            bool Prepare(RE::GFxValue& root, size_t index, const Label& label, float width)
            {
                if (label.style >= _formats.size())
                {
                    Report("invalid card text style");
                    return false;
                }
                while (_fields.size() <= index)
                {
                    const auto name = std::format("incoming{}", _fields.size());
                    std::array<RE::GFxValue, 6> args{RE::GFxValue(name.c_str()),
                        RE::GFxValue(static_cast<double>(_fields.size() + 1)),
                        RE::GFxValue(0.0), RE::GFxValue(0.0), RE::GFxValue(320.0), RE::GFxValue(22.0)};
                    Field field;
                    if (!root.Invoke("createTextField", nullptr, args.data(), args.size()) ||
                        !root.GetMember(name.c_str(), &field.value) ||
                        !field.value.SetMember("selectable", RE::GFxValue(false)) ||
                        !field.value.SetMember("background", RE::GFxValue(false)) ||
                        !field.value.SetMember("embedFonts", RE::GFxValue(true)))
                    {
                        Report("native card field creation");
                        return false;
                    }
                    RE::GFxValue::DisplayInfo hidden;
                    hidden.SetVisible(false);
                    if (!field.value.SetDisplayInfo(hidden))
                    {
                        Report("native card field initialization");
                        return false;
                    }
                    _fields.push_back(std::move(field));
                }
                auto& field = _fields[index];
                if (field.valid && field.text == label.text && field.style == label.style &&
                    field.inputWidth == width && field.marker == label.pointMarker)
                    return true;
                field.valid = false;
                field.geometryValid = false;
                if (!field.value.SetMember("multiline", RE::GFxValue(!label.pointMarker)) ||
                    !field.value.SetMember("wordWrap", RE::GFxValue(!label.pointMarker)) ||
                    !field.value.SetMember("scroll", RE::GFxValue(1.0)) ||
                    !field.value.SetMember("_width", RE::GFxValue(static_cast<double>(width))) ||
                    !field.value.Invoke("setNewTextFormat", nullptr, &_formats[label.style], 1) ||
                    !field.value.SetText(label.text.c_str()) ||
                    !field.value.Invoke("setTextFormat", nullptr, &_formats[label.style], 1))
                {
                    Report("native card text update");
                    return false;
                }
                RE::GFxValue textWidth, textHeight;
                if (!field.value.GetMember("textWidth", &textWidth) || !textWidth.IsNumber() ||
                    !field.value.GetMember("textHeight", &textHeight) || !textHeight.IsNumber() ||
                    !std::isfinite(textWidth.GetNumber()) || !std::isfinite(textHeight.GetNumber()) ||
                    textWidth.GetNumber() < 0 || textHeight.GetNumber() <= 0 ||
                    textHeight.GetNumber() > std::numeric_limits<float>::max())
                {
                    Report("native wrapped text metrics");
                    return false;
                }
                field.text = label.text;
                field.style = label.style;
                field.marker = label.pointMarker;
                field.inputWidth = width;
                field.width = label.pointMarker ? static_cast<float>(textWidth.GetNumber()) + 6 : width;
                field.height = static_cast<float>(textHeight.GetNumber()) + 4;
                field.valid = true;
                return true;
            }

            bool _ready{false};
            std::array<RE::GFxValue, 3> _formats;
            std::vector<Field> _fields;
            std::map<uint32_t, IncomingUi::PlacedCard> _previous;
            size_t _entry{0};
            uint32_t _target{0}, _observer{0};
            int _scroll{1}, _maxScroll{1};
            uint64_t _timedFrame{std::numeric_limits<uint64_t>::max()};
        };
    }

    IncomingGazeManager& IncomingGazeManager::Get()
    {
        static IncomingGazeManager instance;
        return instance;
    }

    void IncomingGazeManager::InstallUi()
    {
        if (auto* ui = RE::UI::GetSingleton())
        {
            ui->Register(kMenu, IncomingMenu::Create);
            static DiagnosticMenuEvents events;
            static bool subscribed = false;
            if (!subscribed)
            {
                ui->AddEventSink<RE::MenuOpenCloseEvent>(&events);
                subscribed = true;
                for (const auto name : DiagnosticMenuPolicy::menus)
                    if (ui->IsMenuOpen(name))
                        g_menuPolicy.SetMenu(name, true);
                logger::info("[TrueGaze::Incoming] Development diagnostics menu suppression registered; console settings unchanged.");
            }
        }
        else
            Report("UI registration");
    }

    void IncomingGazeManager::SetTuning(const VisualTuning& tuning)
    {
        g_tuning = tuning;
        if (!tuning.showIncomingText)
            g_inspectionStep.store(0, std::memory_order_relaxed);
        if (!tuning.IncomingActive())
            Reset();
    }

    const Incoming::Frame& IncomingGazeManager::Results() const noexcept { return g_frame; }

    IncomingGazeManager::Cost IncomingGazeManager::GetCost() const noexcept
    {
        auto cost = g_cost;
        cost.uiUs = g_uiUs.load(std::memory_order_relaxed);
        cost.peakUiUs = g_peakUiUs.load(std::memory_order_relaxed);
        cost.cards = g_placedCards.load(std::memory_order_relaxed);
        cost.overflowCards = g_overflowCards.load(std::memory_order_relaxed);
        cost.labels = g_labelCount.load(std::memory_order_relaxed);
        std::lock_guard lock(g_timingMutex);
        const auto finalize = g_finalizeTiming.Read();
        const auto ui = g_uiTiming.Read();
        cost.finalizationSamples = finalize.samples;
        cost.finalizationMedianUs = finalize.median;
        cost.finalizationP95Us = finalize.p95;
        cost.finalizationWindowPeakUs = finalize.peak;
        cost.uiSamples = ui.samples;
        cost.uiMedianUs = ui.median;
        cost.uiP95Us = ui.p95;
        cost.uiWindowPeakUs = ui.peak;
        return cost;
    }

    void IncomingGazeManager::StepInspection(int direction) noexcept
    {
        g_inspectionStep.fetch_add(direction, std::memory_order_relaxed);
    }

    bool IncomingGazeManager::DiagnosticsSuppressed() noexcept
    {
        return DiagnosticMenuPolicy::Blocked(g_menuPolicy.Snapshot());
    }

    void IncomingGazeManager::AbandonFrame()
    {
        // Fault handling must not dereference the scene graph that may have faulted.
        g_frame.Begin(g_frameId);
        std::lock_guard lock(g_uiMutex);
        g_ui = {};
    }

    void IncomingGazeManager::Reset()
    {
        for (auto& [id, receiver] : g_receivers)
        {
            (void)id;
            Detach(receiver.above, receiver.parent.get());
            Detach(receiver.front, receiver.parent.get());
        }
        g_receivers.clear();
        g_cost.panels = g_cost.labels = 0;
        g_frame.Begin(g_frameId);
        std::lock_guard lock(g_uiMutex);
        g_ui = {};
    }

    void IncomingGazeManager::BeginFrame(uint64_t frame)
    {
        g_frameId = frame;
        g_frameMenuState = g_menuPolicy.Snapshot();
        g_frame.Begin(frame);
        g_receiverOverflow = 0;
        // Faulted or aborted frames must not leave last frame's UI observations.
        {
            std::lock_guard lock(g_uiMutex);
            g_ui = {};
        }
        if (!g_tuning.IncomingActive() || DiagnosticMenuPolicy::Blocked(g_frameMenuState))
            return;
        auto* player = RE::PlayerCharacter::GetSingleton();
        RegisterReceiver(player);
        auto* camera = RE::Main::WorldRootCamera();
        if (camera && player)
        {
            const auto forward = camera->world.rotate.GetVectorY() * -1.0f;
            g_frame.AddRay({player->GetFormID(), I::Source::Camera,
                            Vec(RE::PlayerCamera::GetActiveCameraPosition()), Vec(forward),
                            g_tuning.LengthUnits(), frame});
        }
    }

    void IncomingGazeManager::RegisterReceiver(RE::Actor* actor)
    {
        if (!g_tuning.IncomingActive() || DiagnosticsSuppressed() ||
            !actor || actor->IsDead() || !actor->Get3D(false))
            return;
        const auto id = actor->GetFormID();
        if (g_receivers.find(id) == g_receivers.end() && g_receivers.size() >= I::kActorCapacity)
        {
            // Drop old-frame registrations before enforcing the current-frame cap.
            for (auto it = g_receivers.begin(); it != g_receivers.end();)
            {
                if (it->second.frame != g_frameId)
                {
                    Detach(it->second.above, it->second.parent.get());
                    Detach(it->second.front, it->second.parent.get());
                    it = g_receivers.erase(it);
                }
                else
                    ++it;
            }
            if (g_receivers.size() >= I::kActorCapacity)
            {
                ++g_receiverOverflow;
                auto worst = std::max_element(g_receivers.begin(), g_receivers.end(),
                    [](const auto& a, const auto& b) { return a.first < b.first; });
                if (id >= worst->first)
                    return;
                Detach(worst->second.above, worst->second.parent.get());
                Detach(worst->second.front, worst->second.parent.get());
                g_receivers.erase(worst);
            }
        }
        auto& receiver = g_receivers[id];
        receiver.actor = actor->GetHandle();
        receiver.frame = g_frameId;
    }

    void IncomingGazeManager::SubmitEyes(RE::Actor* actor, const RE::NiPoint3& origin,
                                          const RE::NiPoint3& direction, float range)
    {
        if (g_tuning.IncomingActive() && !DiagnosticsSuppressed() && actor && !actor->IsPlayerRef())
            g_frame.AddRay({actor->GetFormID(), I::Source::PosedEyes, Vec(origin), Vec(direction),
                            range, g_frameId});
    }

    void IncomingGazeManager::EndFrame()
    {
        if (!g_tuning.IncomingActive())
            return;
        if (!g_menuPolicy.Allows(g_frameMenuState))
        {
            AbandonFrame();
            return;
        }
        const auto start = std::chrono::steady_clock::now();
        auto* camera = RE::Main::WorldRootCamera();
        UiSnapshot snapshot;
        snapshot.visible = true;
        snapshot.frame = g_frameId;
        snapshot.menuState = g_frameMenuState;
        snapshot.inspect = g_tuning.showIncomingText;
        std::map<uint32_t, I::Receiver> poses;
        std::map<uint32_t, G::Vec3> headPositions;
        std::map<uint32_t, std::string> names;
        for (auto it = g_receivers.begin(); it != g_receivers.end();)
        {
            auto& owner = it->second;
            auto actorPtr = owner.actor.get();
            auto* actor = actorPtr.get();
            auto* root = actor ? actor->Get3D(false) : nullptr;
            if (owner.frame != g_frameId || !root || actor->IsDead())
            {
                Detach(owner.above, owner.parent.get());
                Detach(owner.front, owner.parent.get());
                it = g_receivers.erase(it);
                continue;
            }
            auto* parent = root->AsNode();
            auto* head = Find(root, {"NPC Head [Head]", "Head", "Head1", "HEAD",
                                    "Canine_Head", "RabbitHead", "NPC Head",
                                    "HorseScull", "ElkScull", "Bip01 Head"});
            if (!parent || !head || !std::isfinite(head->world.scale) || head->world.scale <= 0)
            {
                Detach(owner.above, owner.parent.get());
                Detach(owner.front, owner.parent.get());
                ++it;
                continue;
            }
            if (owner.parent.get() != parent)
            {
                Detach(owner.above, owner.parent.get());
                Detach(owner.front, owner.parent.get());
                owner.parent.reset(parent);
            }
            I::Receiver receiver{};
            receiver.target = it->first;
            names.emplace(receiver.target, ActorLabel(actor, receiver.target));
            const float scale = head->world.scale;
            const auto eyes = Anchors::ResolveEyeCenters(actor, head, nullptr, nullptr,
                                                         g_tuning.pupilForwardOffsetCm,
                                                         g_tuning.pupilUpOffsetCm);
            // Do not apply humanoid eye-height offsets to a creature cranial anchor.
            const auto mid = actor->IsHumanoid() ? (eyes.left + eyes.right) * 0.5f :
                                                  head->world.translate;
            const auto right = head->world.rotate.GetVectorX();
            const auto forward = head->world.rotate.GetVectorY();
            const auto up = head->world.rotate.GetVectorZ();
            const auto add = [&](const RE::NiPoint3& point, float radius, I::Region region,
                                 I::Quality quality = I::Quality::Approximate) {
                if (receiver.surfaceCount < receiver.surfaces.size())
                    receiver.surfaces[receiver.surfaceCount++] = {Vec(point), radius * scale, region, quality};
            };
            Anchors::EyePair exact{};
            if (actor->IsHumanoid() && Anchors::FaceGenEyeCenters(actor, head, exact))
            {
                const auto fwd = forward * (Anchors::kEyeGlobeRadiusUnits * scale);
                add(exact.left + fwd, 1.4f, I::Region::LeftEye, I::Quality::Anchored);
                add(exact.right + fwd, 1.4f, I::Region::RightEye, I::Quality::Anchored);
                // These remaining landmarks are approximations, not mesh intersections.
                add(mid - up * (4.0f * scale), 2.0f, I::Region::Mouth);
                add(mid + up * (4.0f * scale), 2.5f, I::Region::Forehead);
                add(mid - up * (7.0f * scale), 2.0f, I::Region::Chin);
            }
            else
                add(head->world.translate, 7.0f, I::Region::Head);
            if (auto* spine = Find(root, {"NPC Spine2 [Spn2]", "Spine2", "Spine", "NPC Spine [Spn0]",
                                         "Canine_Spine2", "RabbitSpine2", "NPC Spine2",
                                         "HorseSpine3", "ElkSpine3", "Bip01 Spine2"}))
                add(spine->world.translate, 10.0f, I::Region::Body);
            if (actor->IsHumanoid())
            {
                if (auto* hand = Find(root, {"NPC L Hand [LHnd]"}))
                    add(hand->world.translate, 3.0f, I::Region::LeftHand, I::Quality::Anchored);
                if (auto* hand = Find(root, {"NPC R Hand [RHnd]"}))
                    add(hand->world.translate, 3.0f, I::Region::RightHand, I::Quality::Anchored);
            }
            const bool allow = g_tuning.hcepPanelAllActors || actor->IsPlayerRef();
            const float height = g_tuning.hcepPanelScale * scale;
            const auto frontPos = mid + forward * (g_tuning.HcepPanelForwardOffsetUnits() * scale);
            receiver.front = {Vec(frontPos), Vec(right * -1.0f), Vec(up), Vec(forward * -1.0f),
                               height * kAspect * 0.5f, height * 0.5f, allow && g_tuning.showHcepPanel};
            if (camera)
            {
                const auto cameraRight = camera->world.rotate.GetVectorX();
                const auto cameraUp = camera->world.rotate.GetVectorZ();
                const auto cameraForward = camera->world.rotate.GetVectorY();
                receiver.above = {Vec(mid + RE::NiPoint3{0, 0, height * 0.5f + 12.0f * scale}),
                                   Vec(cameraRight), Vec(cameraUp), Vec(cameraForward),
                                   height * kAspect * 0.5f, height * 0.5f,
                                   allow && g_tuning.showIncomingAbove};
            }
            for (auto* panel : {&receiver.above, &receiver.front})
            {
                if (panel->enabled && !I::ValidPanel(*panel))
                {
                    Report("invalid receiving panel transform");
                    panel->enabled = false;
                }
            }
            PlacePanel(owner.above, owner, receiver.above);
            PlacePanel(owner.front, owner, receiver.front);
            // A failed asset load must not produce a hit on an unrendered quad.
            receiver.above.enabled = receiver.above.enabled && static_cast<bool>(owner.above);
            receiver.front.enabled = receiver.front.enabled && static_cast<bool>(owner.front);
            g_frame.AddReceiver(receiver);
            poses.emplace(receiver.target, receiver);
            headPositions.emplace(receiver.target, Vec(mid));
            ++it;
        }
        g_frame.Resolve(g_tuning.measureIncomingAnatomy, g_tuning.measureIncomingPanels);
        auto* player = RE::PlayerCharacter::GetSingleton();
        const uint32_t playerId = player ? player->GetFormID() : 0;
        auto* playerCamera = RE::PlayerCamera::GetSingleton();
        const bool hud = g_tuning.showIncomingHud && playerCamera && playerCamera->IsInFirstPerson();
        auto incomingCounts = IncomingUi::CountIncoming({g_frame.Hits().data(), g_frame.HitCount()});
        std::map<uint32_t, std::map<uint32_t, std::vector<const I::Hit*>>> observations;
        for (size_t i = 0; i < g_frame.HitCount(); ++i)
        {
            const auto& hit = g_frame.Hits()[i];
            const auto observer = hit.observer;
            if (!names.contains(observer))
                names.emplace(observer, ActorLabel(RE::TESForm::LookupByID<RE::Actor>(observer), observer));
            observations[hit.target][observer].push_back(&hit);
        }
        IncomingUi::IdentifyNames(names);
        for (const auto& [id, pose] : poses)
        {
            const auto& count = incomingCounts[id];
            Card card;
            card.id = id;
            card.active = !count.observers.empty();
            card.hud = hud && id == playerId;
            const bool headOnScreen = Project(camera, headPositions.at(id), card.x, card.y);
            if (headOnScreen)
                snapshot.faces.push_back({card.x - 40, card.y - 48, 80, 96});
            card.onScreen = headOnScreen && (pose.above.enabled || pose.front.enabled || snapshot.inspect);
            if (card.hud)
            {
                card.onScreen = true;
                card.x = 40;
                card.y = 220;
            }
            card.rows.push_back({0, 0, 0xFFFFFF, names.at(id), false, 1});
            card.rows.push_back({0, 0, card.active ? 0xFFFFFFu : 0xBBBBBBu,
                card.active ? std::format("{} incoming observer{}", count.observers.size(),
                    count.observers.size() == 1 ? "" : "s") : "No incoming matches detected."});
            const auto detailHeader = std::format(
                "{} [target {:08X}]\n{}\n{} measurements\nEstimated; visibility not checked.\n",
                names.at(id), id, IncomingUi::Summary(count.observers.size()), count.measurements);
            if (snapshot.inspect && !card.active)
                snapshot.inspection.push_back({id, 0, detailHeader});
            for (const auto& [observer, hits] : observations[id])
            {
                std::string row = hits.front()->source == I::Source::Camera ?
                    "Your view | " : names.at(observer) + " | ";
                std::string detail = detailHeader;
                for (size_t i = 0; i < hits.size(); ++i)
                {
                    const auto& hit = *hits[i];
                    if (i)
                        row += "; ";
                    row += hit.measurement == I::Measurement::Anatomy ?
                        std::string(IncomingUi::RegionDescription(hit.region)) + " ~" :
                        hit.measurement == I::Measurement::AbovePanel ? "above-head panel" : "front panel";
                    if (snapshot.inspect)
                    {
                        detail += "\n" + IncomingUi::Describe(hit, names.at(observer), names.at(id), id == playerId);
                        detail += std::format("\n{} {:08X}>{:08X} | {} {} | [{:.1f},{:.1f},{:.1f}] | {} LOS?\n",
                            hit.source == I::Source::Camera ? "Camera" : "Eyes", observer, id,
                            hit.measurement == I::Measurement::Anatomy ? "Anatomy" :
                                hit.measurement == I::Measurement::AbovePanel ? "Panel A" : "Panel B",
                            I::Name(hit.region), hit.point.x, hit.point.y, hit.point.z,
                            hit.quality == I::Quality::Anchored ? "~anchor" : "~");
                    }
                }
                card.rows.push_back({0, 0, ObserverColor(observer), std::move(row)});
                if (snapshot.inspect)
                    snapshot.inspection.push_back({id, observer, std::move(detail)});
            }
            card.rows.push_back({0, 0, 0xBBBBBB, "Estimated; visibility not checked.", false, 2});
            snapshot.cards.push_back(std::move(card));
        }
        for (size_t i = 0; i < g_frame.HitCount(); ++i)
        {
            const auto& hit = g_frame.Hits()[i];
            float x = 0, y = 0;
            // Mark actual quad hits even when verbose text is disabled.
            if (hit.measurement != I::Measurement::Anatomy && Project(camera, hit.point, x, y))
                snapshot.labels.push_back({x, y, ObserverColor(hit.observer), "+", true});
            if (hit.measurement == I::Measurement::Anatomy)
            {
                // Display-only anatomical map; never feed these coordinates into ray tests.
                float u = 0, v = 0;
                switch (hit.region)
                {
                case I::Region::LeftEye: u = 0.15f; v = 0.28f; break;
                case I::Region::RightEye: u = -0.15f; v = 0.28f; break;
                case I::Region::Mouth: v = 0.05f; break;
                case I::Region::Forehead: v = 0.4f; break;
                case I::Region::Chin: v = -0.1f; break;
                case I::Region::LeftHand: u = 0.4f; v = -0.32f; break;
                case I::Region::RightHand: u = -0.4f; v = -0.32f; break;
                case I::Region::Body: v = -0.3f; break;
                case I::Region::Head: v = 0.25f; break;
                default: break;
                }
                const auto& pose = poses.at(hit.target);
                for (const auto* panel : {&pose.above, &pose.front})
                {
                    const auto marker = panel->center + panel->right * (u * panel->halfWidth * 2) +
                                        panel->up * (v * panel->halfHeight * 2);
                    if (panel->enabled && Project(camera, marker, x, y))
                    {
                        snapshot.labels.push_back({x, y, ObserverColor(hit.observer), "o", true});
                    }
                }
            }
            Engine::TraceLogger::Get().LogIncomingHit(hit);
        }
        if (g_tuning.showIncomingText || hud)
            snapshot.labels.push_back({20, 114, 0xFFFFFF,
                std::format("Incoming ~ | rays:{} targets:{} hits:{} | visibility unverified",
                    g_frame.RayCount(), g_frame.ReceiverCount(), g_frame.HitCount()), false, 2});
        if (g_frame.Overflow() || g_receiverOverflow || g_frame.Invalid() ||
            g_receivers.size() != poses.size() || g_failures.load(std::memory_order_relaxed))
            snapshot.labels.push_back({20, 136, 0xFFD966,
                std::format("Incoming omissions:{}+{} invalid:{} unsupported:{} errors:{}",
                    g_frame.Overflow(), g_receiverOverflow, g_frame.Invalid(),
                    g_receivers.size() - poses.size(), g_failures.load(std::memory_order_relaxed)), false, 2});
        if (auto* queue = RE::UIMessageQueue::GetSingleton())
        {
            auto* ui = RE::UI::GetSingleton();
            if (ui && !ui->IsMenuOpen(kMenu))
                queue->AddMessage(RE::BSFixedString(kMenu), RE::UI_MESSAGE_TYPE::kShow, nullptr);
        }
        {
            std::lock_guard lock(g_uiMutex);
            g_cost.labels = snapshot.labels.size();
            g_ui = std::move(snapshot);
        }
        g_cost.panels = 0;
        for (const auto& [id, owner] : g_receivers)
        {
            (void)id;
            g_cost.panels += static_cast<size_t>(static_cast<bool>(owner.above)) +
                             static_cast<size_t>(static_cast<bool>(owner.front));
        }
        g_cost.finalizationUs = ElapsedUs(start);
        g_cost.peakFinalizationUs = std::max(g_cost.peakFinalizationUs, g_cost.finalizationUs);
        {
            std::lock_guard lock(g_timingMutex);
            g_finalizeTiming.Add(g_cost.finalizationUs);
        }
    }
}
