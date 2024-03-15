#pragma once

#include <oscar/Utils/Assertions.h>

#include <cstdint>
#include <compare>
#include <stdexcept>

namespace osc
{
    // provides convenient manipulation of the three coordinate axes (X, Y, Z)
    //
    // inspired by simbody's `SimTK::CoordinateAxis` class
    class CoordinateAxis final {
    public:
        // returns a `CoordinateAxis` that represents the X axis
        static constexpr CoordinateAxis x()
        {
            return CoordinateAxis{0};
        }

        // returns a `CoordinateAxis` that represents the Y axis
        static constexpr CoordinateAxis y()
        {
            return CoordinateAxis{1};
        }

        // returns a `CoordinateAxis` that represents the Z axis
        static constexpr CoordinateAxis z()
        {
            return CoordinateAxis{2};
        }

        // default-constructs a `CoordinateAxis` that represents the X axis
        constexpr CoordinateAxis() = default;

        // constructs a `CoordinateAxis` from a runtime integer that must be 0, 1, or 2, representing X, Y, or Z axis
        explicit constexpr CoordinateAxis(int i) :
            m_AxisIndex{static_cast<uint8_t>(i)}
        {
            OSC_ASSERT(0 <= i && i <= 2 && "out-of-range index given to a CoordinateAxis");
        }

        // `CoordinateAxis`es are equality-comparable and totally ordered as X < Y < Z
        friend constexpr auto operator<=>(CoordinateAxis const&, CoordinateAxis const&) = default;

        // returns the index of the axis (i.e. X == 0, Y == 1, Z == 2)
        constexpr size_t index() const
        {
            return static_cast<size_t>(m_AxisIndex);
        }

        // returns the previous axis in the ring sequence X -> Y -> Z -> X...
        constexpr CoordinateAxis previous() const
        {
            return CoordinateAxis{static_cast<uint8_t>((m_AxisIndex + 2) % 3), SkipRangeCheck{}};
        }

        // returns the next axis in the ring sequence X -> Y -> Z -> X...
        constexpr CoordinateAxis next() const
        {
            return CoordinateAxis{static_cast<uint8_t>((m_AxisIndex + 1) % 3), SkipRangeCheck{}};
        }
    private:
        struct SkipRangeCheck {};
        explicit constexpr CoordinateAxis(uint8_t v, SkipRangeCheck) :
            m_AxisIndex{v}
        {}

        uint8_t m_AxisIndex = 0;
    };
}
