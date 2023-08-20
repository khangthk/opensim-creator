#pragma once

#include "oscar/Utils/Cpp20Shims.hpp"

#include <cstddef>
#include <iosfwd>
#include <string>

namespace osc
{
    class AntiAliasingLevel final {
    public:
        static constexpr AntiAliasingLevel min() noexcept
        {
            return AntiAliasingLevel{1};
        }

        static constexpr AntiAliasingLevel max() noexcept
        {
            return AntiAliasingLevel{128};
        }

        static constexpr AntiAliasingLevel none() noexcept
        {
            return AntiAliasingLevel{1};
        }

        constexpr AntiAliasingLevel() = default;

        explicit constexpr AntiAliasingLevel(int value) noexcept :
            m_Value{value > 1 ? uint32_t(1) << (bit_width(static_cast<unsigned>(value))-1) : 1}
        {
        }

        constexpr int32_t getI32() const noexcept
        {
            return static_cast<int32_t>(m_Value);
        }

        constexpr uint32_t getU32() const noexcept
        {
            return m_Value;
        }

        constexpr AntiAliasingLevel& operator++() noexcept
        {
            m_Value <<= 1;
            return *this;
        }

    private:
        uint32_t m_Value = 1;
    };

    constexpr bool operator==(AntiAliasingLevel const& a, AntiAliasingLevel const& b) noexcept
    {
        return a.getU32() == b.getU32();
    }

    constexpr bool operator!=(AntiAliasingLevel const& a, AntiAliasingLevel const& b) noexcept
    {
        return a.getU32() != b.getU32();
    }

    constexpr bool operator<(AntiAliasingLevel const& a, AntiAliasingLevel const& b) noexcept
    {
        return a.getU32() < b.getU32();
    }

    constexpr bool operator<=(AntiAliasingLevel const& a, AntiAliasingLevel const& b) noexcept
    {
        return a.getU32() <= b.getU32();
    }

    constexpr bool operator>(AntiAliasingLevel const& a, AntiAliasingLevel const& b) noexcept
    {
        return a.getU32() > b.getU32();
    }

    std::ostream& operator<<(std::ostream&, AntiAliasingLevel);
    std::string to_string(AntiAliasingLevel);
}
