// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_COMPILER_MIN_MAX_DEFINITION_H
#if SC_FOUNDATION_COMPILER_MIN_MAX_DEFINITION_H != 1
#error "CompilerMinMax.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_COMPILER_MIN_MAX_DEFINITION_H 1 // Increment to indicate a new version of the file

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
//! @addtogroup group_algorithms
//! @{

/// Finds the minimum of two values.
// clang-format off
namespace SC
{
template <typename T> constexpr const T& min(const T& t1, const T& t2) { return t1 < t2 ? t1 : t2; }
/// Finds the maximum of two values.
template <typename T>
constexpr const T& max(const T& t1, const T& t2) { return t1 > t2 ? t1 : t2; }
}
// clang-format on

//! @}
#endif // SC_FOUNDATION_COMPILER_MIN_MAX_DEFINITION_H
