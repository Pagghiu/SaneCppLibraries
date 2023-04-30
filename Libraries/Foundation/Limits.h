// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Types.h"

namespace SC
{
struct MaxValue
{
    template <typename T>
    constexpr T SignedMaxValue() const
    {
        // (1ull << (sizeof(T) * 8 - 1)) - 1; produces warning on MSVC
        return (~0) & ~static_cast<T>((1ull << (sizeof(T) * 8 - 1)));
    }
    template <typename T>
    constexpr T UnsignedMaxValue() const
    {
        return static_cast<T>(~0ull);
    }

    constexpr operator uint8_t() const { return UnsignedMaxValue<uint8_t>(); }
    constexpr operator uint16_t() const { return UnsignedMaxValue<uint16_t>(); }
    constexpr operator uint32_t() const { return UnsignedMaxValue<uint32_t>(); }
    constexpr operator uint64_t() const { return UnsignedMaxValue<uint64_t>(); }
#if SC_MSVC == 0 && SC_CLANG_CL == 0
    constexpr operator size_t() const { return UnsignedMaxValue<size_t>(); }
#endif

    constexpr operator int8_t() const { return SignedMaxValue<int8_t>(); }
    constexpr operator int16_t() const { return SignedMaxValue<int16_t>(); }
    constexpr operator int32_t() const { return SignedMaxValue<int32_t>(); }
    constexpr operator int64_t() const { return SignedMaxValue<int64_t>(); }
#if SC_MSVC == 0 && SC_CLANG_CL == 0
    constexpr operator ssize_t() const { return SignedMaxValue<ssize_t>(); }
#endif

    constexpr operator float() const { return 3.40282347e+38F; }
    constexpr operator double() const { return 1.7976931348623157e+308; }
};
} // namespace SC
