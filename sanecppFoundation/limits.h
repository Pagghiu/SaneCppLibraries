#pragma once
#include "types.h"

namespace sanecpp
{
struct MaxValue
{
    template <typename T>
    constexpr T SignedMaxValue() const
    {
        return (1ull << (sizeof(T) * 8 - 1)) - 1;
    }
    template <typename T>
    constexpr T UnsignedMaxValue() const
    {
        return ~0;
    }

    constexpr operator uint8_t const() const { return UnsignedMaxValue<uint8_t>(); }
    constexpr operator uint16_t const() const { return UnsignedMaxValue<uint16_t>(); }
    constexpr operator uint32_t const() const { return UnsignedMaxValue<uint32_t>(); }
    constexpr operator uint64_t const() const { return UnsignedMaxValue<uint64_t>(); }

    constexpr operator int8_t const() const { return SignedMaxValue<int8_t>(); }
    constexpr operator int16_t const() const { return SignedMaxValue<int16_t>(); }
    constexpr operator int32_t const() const { return SignedMaxValue<int32_t>(); }
    constexpr operator int64_t const() const { return SignedMaxValue<int64_t>(); }

    constexpr operator float const() const { return 3.40282347e+38F; }
    constexpr operator double const() const { return 1.7976931348623157e+308; }
};
} // namespace sanecpp
