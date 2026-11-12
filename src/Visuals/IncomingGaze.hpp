#pragma once

#include "Visuals/GazeGeometry.hpp"

#include <array>
#include <cstdint>
#include <limits>

namespace TrueGaze::Visuals::Incoming
{
    namespace G = Geometry;
    inline constexpr size_t kActorCapacity = 64;
    inline constexpr size_t kObserverCapacity = kActorCapacity + 1;

    enum class Source : uint8_t { PosedEyes, Camera };
    enum class Measurement : uint8_t { Anatomy, AbovePanel, FrontPanel };
    enum class Region : uint8_t
    {
        Head, Body, LeftEye, RightEye, Mouth, Forehead, Chin, LeftHand, RightHand, Unknown
    };
    enum class Quality : uint8_t { Approximate, Anchored };

    inline const char* Name(Region region) noexcept
    {
        switch (region)
        {
        case Region::Head: return "Head";
        case Region::Body: return "Body";
        case Region::LeftEye: return "LeftEye";
        case Region::RightEye: return "RightEye";
        case Region::Mouth: return "Mouth";
        case Region::Forehead: return "Forehead";
        case Region::Chin: return "Chin";
        case Region::LeftHand: return "LeftHand";
        case Region::RightHand: return "RightHand";
        default: return "Unsupported";
        }
    }

    inline bool Finite(G::Vec3 v) noexcept
    {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }

    struct Ray
    {
        uint32_t observer{0};
        Source source{Source::PosedEyes};
        G::Vec3 origin{};
        G::Vec3 direction{};
        float range{0};
        uint64_t frame{0};
    };

    struct Surface
    {
        G::Vec3 center{};
        float radius{0};
        Region region{Region::Unknown};
        Quality quality{Quality::Approximate};
    };

    struct Panel
    {
        G::Vec3 center{};
        G::Vec3 right{1, 0, 0};
        G::Vec3 up{0, 0, 1};
        G::Vec3 normal{0, 1, 0};
        float halfWidth{0};
        float halfHeight{0};
        bool enabled{false};
    };

    struct Receiver
    {
        uint32_t target{0};
        std::array<Surface, 9> surfaces{};
        size_t surfaceCount{0};
        Panel above{};
        Panel front{};
    };

    inline bool ValidPanel(const Panel& panel) noexcept
    {
        return Finite(panel.center) && Finite(panel.right) && Finite(panel.up) &&
            Finite(panel.normal) && std::isfinite(panel.halfWidth) &&
            std::isfinite(panel.halfHeight) && panel.halfWidth > 0 && panel.halfHeight > 0 &&
            std::abs(G::Length(panel.right) - 1) <= 0.001f &&
            std::abs(G::Length(panel.up) - 1) <= 0.001f &&
            std::abs(G::Length(panel.normal) - 1) <= 0.001f &&
            std::abs(G::Dot(panel.right, panel.up)) <= 0.001f &&
            std::abs(G::Dot(panel.right, panel.normal)) <= 0.001f &&
            std::abs(G::Dot(panel.up, panel.normal)) <= 0.001f;
    }

    struct Hit
    {
        bool hit{false};
        uint32_t observer{0};
        uint32_t target{0};
        Source source{Source::PosedEyes};
        Measurement measurement{Measurement::Anatomy};
        Region region{Region::Unknown};
        Quality quality{Quality::Approximate};
        G::Vec3 point{};
        float distance{0};
        float x{0};
        float z{0};
        uint64_t frame{0};
        bool visibilityVerified{false};
    };

    inline float SphereDistance(const Ray& ray, const Surface& surface) noexcept
    {
        if (!Finite(surface.center) || !std::isfinite(surface.radius) || surface.radius <= 0)
            return -1;
        const auto offset = ray.origin - surface.center;
        const float b = G::Dot(offset, ray.direction);
        const float c = G::Dot(offset, offset) - surface.radius * surface.radius;
        // An origin inside a volume is not an incoming anatomical observation.
        if (c <= 0)
            return -1;
        const float discriminant = b * b - c;
        if (!std::isfinite(discriminant) || discriminant < 0)
            return -1;
        const float t = -b - std::sqrt(discriminant);
        return t >= 0 && t <= ray.range ? t : -1;
    }

    inline Hit Intersect(const Ray& ray, const Receiver& receiver,
                         Measurement measurement) noexcept
    {
        Hit result{};
        result.observer = ray.observer;
        result.target = receiver.target;
        result.source = ray.source;
        result.measurement = measurement;
        result.frame = ray.frame;
        if (!ray.observer || !receiver.target || ray.observer == receiver.target ||
            !Finite(ray.origin) || !Finite(ray.direction) || !std::isfinite(ray.range) ||
            ray.range <= 0 || std::abs(G::Length(ray.direction) - 1.0f) > 0.001f)
            return result;

        if (measurement == Measurement::Anatomy)
        {
            float nearest = std::numeric_limits<float>::max();
            for (size_t i = 0; i < std::min(receiver.surfaceCount, receiver.surfaces.size()); ++i)
            {
                const auto& surface = receiver.surfaces[i];
                const float t = SphereDistance(ray, surface);
                if (t >= 0 && t < nearest)
                {
                    nearest = t;
                    result.hit = true;
                    result.distance = t;
                    result.region = surface.region;
                    result.quality = surface.quality;
                }
            }
        }
        else
        {
            const auto& panel = measurement == Measurement::AbovePanel ? receiver.above : receiver.front;
            if (!panel.enabled || !ValidPanel(panel))
                return result;
            const auto hit = G::IntersectRayPlane(ray.origin, ray.direction, panel.center, panel.normal);
            if (!hit.hit || hit.t > ray.range)
                return result;
            const auto offset = hit.point - panel.center;
            result.x = G::Dot(offset, panel.right);
            result.z = G::Dot(offset, panel.up);
            if (std::abs(result.x) > panel.halfWidth || std::abs(result.z) > panel.halfHeight)
                return result;
            result.hit = true;
            result.distance = hit.t;
        }
        if (result.hit)
            result.point = ray.origin + ray.direction * result.distance;
        return result;
    }

    class Frame
    {
    public:
        void Begin(uint64_t frame) noexcept
        {
            _frame = frame;
            _rayCount = _receiverCount = _hitCount = 0;
            _overflow = _invalid = 0;
        }

        bool AddRay(Ray ray) noexcept
        {
            if (ray.frame != _frame || !ray.observer || !Finite(ray.origin) ||
                !Finite(ray.direction) || G::Length(ray.direction) < 0.001f ||
                !std::isfinite(G::Length(ray.direction)) ||
                (ray.source != Source::Camera && ray.source != Source::PosedEyes) ||
                !std::isfinite(ray.range) || ray.range <= 0)
            {
                ++_invalid;
                return false;
            }
            ray.direction = G::Normalized(ray.direction);
            for (size_t i = 0; i < _rayCount; ++i)
            {
                if (_rays[i].observer == ray.observer && _rays[i].source == ray.source)
                {
                    _rays[i] = ray;
                    return true;
                }
            }
            size_t sourceCount = 0;
            size_t worst = 0;
            for (size_t i = 0; i < _rayCount; ++i)
            {
                if (_rays[i].source == ray.source)
                {
                    if (sourceCount == 0 || _rays[i].observer > _rays[worst].observer)
                        worst = i;
                    ++sourceCount;
                }
            }
            if (sourceCount >= (ray.source == Source::Camera ? 1 : kActorCapacity))
            {
                ++_overflow;
                if (ray.observer >= _rays[worst].observer)
                    return false;
                _rays[worst] = ray;
                return true;
            }
            _rays[_rayCount++] = ray;
            return true;
        }

        bool AddReceiver(const Receiver& receiver) noexcept
        {
            if (!receiver.target)
            {
                ++_invalid;
                return false;
            }
            for (size_t i = 0; i < _receiverCount; ++i)
            {
                if (_receivers[i].target == receiver.target)
                {
                    _receivers[i] = receiver;
                    return true;
                }
            }
            if (_receiverCount == _receivers.size())
            {
                ++_overflow;
                auto worst = std::max_element(_receivers.begin(), _receivers.begin() + _receiverCount,
                    [](const Receiver& a, const Receiver& b) { return a.target < b.target; });
                if (receiver.target >= worst->target)
                    return false;
                *worst = receiver;
                return true;
            }
            _receivers[_receiverCount++] = receiver;
            return true;
        }

        void Resolve(bool anatomy, bool physicalPanels) noexcept
        {
            _hitCount = 0;
            std::sort(_rays.begin(), _rays.begin() + _rayCount,
                [](const Ray& a, const Ray& b) {
                    return a.observer != b.observer ? a.observer < b.observer : a.source < b.source;
                });
            std::sort(_receivers.begin(), _receivers.begin() + _receiverCount,
                [](const Receiver& a, const Receiver& b) { return a.target < b.target; });
            for (size_t i = 0; i < _rayCount; ++i)
            {
                Hit nearest{};
                for (size_t j = 0; j < _receiverCount; ++j)
                {
                    if (anatomy)
                    {
                        const auto hit = Intersect(_rays[i], _receivers[j], Measurement::Anatomy);
                        if (hit.hit && (!nearest.hit || hit.distance < nearest.distance ||
                                        (hit.distance == nearest.distance && hit.target < nearest.target)))
                            nearest = hit;
                    }
                    if (physicalPanels)
                    {
                        for (auto measurement : {Measurement::AbovePanel, Measurement::FrontPanel})
                        {
                            const auto hit = Intersect(_rays[i], _receivers[j], measurement);
                            if (hit.hit)
                                _hits[_hitCount++] = hit;
                        }
                    }
                }
                if (nearest.hit)
                    _hits[_hitCount++] = nearest;
            }
        }

        const auto& Hits() const noexcept { return _hits; }
        size_t HitCount() const noexcept { return _hitCount; }
        size_t RayCount() const noexcept { return _rayCount; }
        size_t ReceiverCount() const noexcept { return _receiverCount; }
        uint32_t Overflow() const noexcept { return _overflow; }
        uint32_t Invalid() const noexcept { return _invalid; }

    private:
        uint64_t _frame{0};
        std::array<Ray, kObserverCapacity> _rays{};
        std::array<Receiver, kActorCapacity> _receivers{};
        std::array<Hit, kObserverCapacity * (1 + kActorCapacity * 2)> _hits{};
        size_t _rayCount{0}, _receiverCount{0}, _hitCount{0};
        uint32_t _overflow{0}, _invalid{0};
    };
}
