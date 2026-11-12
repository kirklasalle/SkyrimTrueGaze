#pragma once

#include <algorithm>
#include <cmath>
#include <format>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Visuals/IncomingGaze.hpp"

namespace TrueGaze::Visuals::IncomingUi
{
    struct TargetCounts
    {
        size_t measurements{0};
        std::set<uint32_t> observers;
    };

    inline std::map<uint32_t, TargetCounts> CountIncoming(std::span<const Incoming::Hit> hits)
    {
        std::map<uint32_t, TargetCounts> counts;
        for (const auto& hit : hits)
        {
            if (!hit.hit)
                continue;
            auto& target = counts[hit.target];
            ++target.measurements;
            target.observers.insert(hit.observer);
        }
        return counts;
    }

    inline void IdentifyNames(std::map<uint32_t, std::string>& names)
    {
        std::map<std::string, size_t> duplicates;
        for (const auto& [id, name] : names)
            ++duplicates[name];
        for (auto& [id, name] : names)
            if (duplicates.at(name) > 1)
                name = std::format("{} [{:08X}]", name, id);
    }

    inline std::string Summary(size_t observers)
    {
        return observers ? std::format("Gaze matches from {} observer{}.", observers,
                                       observers == 1 ? "" : "s") :
                           "No incoming gaze matches detected.";
    }

    inline const char* RegionDescription(Incoming::Region region)
    {
        switch (region)
        {
        case Incoming::Region::LeftEye: return "left eye area";
        case Incoming::Region::RightEye: return "right eye area";
        case Incoming::Region::Mouth: return "mouth area";
        case Incoming::Region::Forehead: return "forehead area";
        case Incoming::Region::Chin: return "chin area";
        case Incoming::Region::LeftHand: return "left hand area";
        case Incoming::Region::RightHand: return "right hand area";
        case Incoming::Region::Head: return "head area";
        case Incoming::Region::Body: return "body area";
        default: return "unsupported area";
        }
    }

    inline std::string Describe(const Incoming::Hit& hit, std::string_view observer,
                                std::string_view target, bool targetIsPlayer)
    {
        const auto subject = hit.source == Incoming::Source::Camera ?
            std::string("Your view") : std::format("{}'s gaze", observer);
        const auto owner = targetIsPlayer ? std::string("your") : std::format("{}'s", target);
        if (hit.measurement == Incoming::Measurement::Anatomy)
            return std::format("{} points toward {} {} (estimated).",
                               subject, owner, RegionDescription(hit.region));
        return std::format("{} crosses {} {} panel.", subject, owner,
            hit.measurement == Incoming::Measurement::AbovePanel ? "above-head" : "front");
    }

    struct TextRect
    {
        float x, y, width, height;
    };

    inline bool Overlaps(const TextRect& a, const TextRect& b, float gap = 8) noexcept
    {
        return a.x < b.x + b.width + gap && a.x + a.width + gap > b.x &&
               a.y < b.y + b.height + gap && a.y + a.height + gap > b.y;
    }

    struct CardRequest
    {
        uint32_t id;
        float anchorX, anchorY, width, height;
        bool active;
    };
    struct PlacedCard
    {
        uint32_t id;
        TextRect rect;
        float anchorX, anchorY;
    };
    struct CardLayout
    {
        std::vector<PlacedCard> placed;
        std::vector<uint32_t> overflow;
    };

    struct InspectionPosition { size_t entry; int scroll; };
    inline InspectionPosition StepInspection(InspectionPosition position, int maximum,
                                              int lines, size_t entries, bool forward) noexcept
    {
        if (forward && position.scroll < maximum)
            position.scroll += std::min(lines, maximum - position.scroll);
        else if (!forward && position.scroll > 1)
            position.scroll -= std::min(lines, position.scroll - 1);
        else
        {
            position.entry = forward ? (position.entry + 1) % entries :
                                       (position.entry + entries - 1) % entries;
            position.scroll = forward ? 1 : std::numeric_limits<int>::max();
        }
        return position;
    }

    inline CardLayout LayoutCards(std::span<const CardRequest> requests,
                                  std::span<const TextRect> reserved,
                                  const std::map<uint32_t, PlacedCard>& previous)
    {
        std::vector<CardRequest> ordered(requests.begin(), requests.end());
        std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
            return a.active != b.active ? a.active > b.active : a.id < b.id;
        });
        CardLayout result;
        for (const auto& request : ordered)
        {
            const auto free = [&](const TextRect& rect) {
                if (!std::isfinite(rect.x) || !std::isfinite(rect.y) ||
                    !std::isfinite(rect.width) || !std::isfinite(rect.height) ||
                    rect.width <= 0 || rect.height <= 0 ||
                    rect.x < 12 || rect.y < 156 ||
                    rect.x + rect.width > 1268 || rect.y + rect.height > 608)
                    return false;
                for (const auto& area : reserved)
                    if (Overlaps(rect, area))
                        return false;
                for (const auto& card : result.placed)
                    if (Overlaps(rect, card.rect))
                        return false;
                return true;
            };
            TextRect chosen{};
            bool found = false;
            if (const auto old = previous.find(request.id); old != previous.end())
            {
                chosen = {old->second.rect.x + request.anchorX - old->second.anchorX,
                          old->second.rect.y + request.anchorY - old->second.anchorY,
                          request.width, request.height};
                found = free(chosen);
            }
            if (!found)
            {
                float best = std::numeric_limits<float>::max();
                const auto consider = [&](float x, float y) {
                    const TextRect rect{x, y, request.width, request.height};
                    if (!free(rect))
                        return;
                    const float dx = x + request.width * 0.5f - request.anchorX;
                    const float dy = y + request.height * 0.5f - request.anchorY;
                    const float score = dx * dx + dy * dy;
                    if (score < best)
                    {
                        best = score;
                        chosen = rect;
                        found = true;
                    }
                };
                consider(request.anchorX + 48, request.anchorY - request.height * 0.5f);
                consider(request.anchorX - request.width - 48, request.anchorY - request.height * 0.5f);
                consider(request.anchorX - request.width * 0.5f, request.anchorY - request.height - 56);
                consider(request.anchorX - request.width * 0.5f, request.anchorY + 56);
                if (std::isfinite(request.width) && request.width > 0 &&
                    std::isfinite(request.height) && request.height > 0)
                    for (float x = 12; x + request.width <= 1268; x += request.width + 12)
                        for (float y = 156; y + request.height <= 608; y += 26)
                            consider(x, y);
            }
            if (found)
                result.placed.push_back({request.id, chosen, request.anchorX, request.anchorY});
            else
                result.overflow.push_back(request.id);
        }
        return result;
    }

    class TimingWindow
    {
    public:
        struct Stats { size_t samples; uint64_t median, p95, peak; };
        void Add(uint64_t value) noexcept
        {
            _values[_next] = value;
            _next = (_next + 1) % _values.size();
            _count = std::min(_count + 1, _values.size());
        }
        Stats Read() const noexcept
        {
            auto values = _values;
            std::sort(values.begin(), values.begin() + _count);
            return _count ? Stats{_count, values[_count / 2], values[(_count * 95 + 99) / 100 - 1], values[_count - 1]} :
                            Stats{0, 0, 0, 0};
        }
    private:
        std::array<uint64_t, 256> _values{};
        size_t _next{0}, _count{0};
    };

    // Dimensions come from GFx text metrics, not estimated character widths.
    inline TextRect PlaceText(float x, float y, float width, float height) noexcept
    {
        constexpr float margin = 12;
        width = std::clamp(width, 1.0f, 1280 - 2 * margin);
        height = std::clamp(height, 1.0f, 720 - 2 * margin);
        return {std::clamp(x, margin, 1280 - margin - width),
                std::clamp(y, margin, 720 - margin - height), width, height};
    }
}
