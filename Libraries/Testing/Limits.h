// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Common/CompilerMacrosStdVersion.h"
#include "../Common/CompilerMacrosType.h"
#include "../Common/PrimitiveDefinitions.h"
#include "../Common/TypeTraits.h"

namespace SC
{
struct MaxValue;
} // namespace SC

//! @addtogroup group_foundation_utility
//! @{

/// @brief An object that can be converted to any primitive type providing its max value.
///
/// For example:
/// @code
/// uint64_t value = MaxValue(); // contains now maximum value for uint64_t
/// @endcode
struct SC::MaxValue
{
    template <typename T>
    constexpr T SignedMaxValue() const
    {
        return static_cast<T>((1ull << (sizeof(T) * 8 - 1)) - 1);
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

    constexpr operator int8_t() const { return SignedMaxValue<int8_t>(); }
    constexpr operator int16_t() const { return SignedMaxValue<int16_t>(); }
    constexpr operator int32_t() const { return SignedMaxValue<int32_t>(); }
    constexpr operator int64_t() const { return SignedMaxValue<int64_t>(); }

    // Size types may alias one of the fixed-width types. Exact non-template conversions take precedence.
    template <typename T,
              typename TypeTraits::EnableIf<
                  TypeTraits::IsSame<T, size_t>::value or TypeTraits::IsSame<T, ssize_t>::value, int>::type = 0>
    constexpr operator T() const
    {
        if SC_LANGUAGE_IF_CONSTEXPR (TypeTraits::IsSame<T, size_t>::value)
            return UnsignedMaxValue<T>();
        else
            return SignedMaxValue<T>();
    }

#if SC_COMPILER_MSVC
    constexpr operator float() const { return 3.402823466e+38F; }
    constexpr operator double() const { return 1.7976931348623158e+308; }
#else
    constexpr operator float() const { return 3.40282347e+38F; }
    constexpr operator double() const { return 1.7976931348623157e+308; }
#endif
};

//! @}
