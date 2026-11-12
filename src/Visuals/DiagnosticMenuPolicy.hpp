#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string_view>

namespace TrueGaze::Visuals
{
    class DiagnosticMenuPolicy
    {
    public:
        inline static constexpr std::array<std::string_view, 12> menus{
            "TweenMenu", "InventoryMenu", "MagicMenu", "MapMenu", "StatsMenu",
            "Journal Menu", "ContainerMenu", "BarterMenu", "Crafting Menu",
            "Book Menu", "Loading Menu", "Main Menu"};
        static_assert(menus.size() <= 32);

        bool SetMenu(std::string_view name, bool opening) noexcept
        {
            size_t index = 0;
            while (index < menus.size() && menus[index] != name)
                ++index;
            if (index == menus.size())
                return false;
            const uint32_t bit = uint32_t{1} << index;
            auto old = _state.load(std::memory_order_acquire);
            for (;;)
            {
                const auto mask = static_cast<uint32_t>(old);
                const auto nextMask = opening ? mask | bit : mask & ~bit;
                if (nextMask == mask)
                    return false;
                // Publish menu mask and generation together: closing a menu must
                // never briefly permit a snapshot captured before it opened.
                const uint64_t next = (((old >> 32) + 1) << 32) | nextMask;
                if (_state.compare_exchange_weak(old, next, std::memory_order_acq_rel))
                    return true;
            }
        }

        uint64_t Snapshot() const noexcept { return _state.load(std::memory_order_acquire); }
        static bool Blocked(uint64_t state) noexcept { return static_cast<uint32_t>(state) != 0; }
        bool Allows(uint64_t snapshot) const noexcept
        {
            const auto current = Snapshot();
            return !Blocked(current) && current == snapshot;
        }

    private:
        std::atomic<uint64_t> _state{0};
    };
}
