// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/PrimitiveTypes.h"
//!
//! @defgroup group_foundation Foundation
//! @copybrief library_foundation
//!
//! See @ref library_foundation library page for more details.<br>

//! @defgroup group_foundation_type_traits Type Traits
//! @ingroup group_foundation
//! Type Traits (EnableIf, AddPointer, etc.)

namespace SC
{
/// @brief Template meta-programming helpers
namespace TypeTraits
{
// clang-format off
//! @addtogroup group_foundation_type_traits
//! @{

/// EnableIf conditionally defines a type if a boolean template parameter is true.
template <bool B, class T = void> struct EnableIf {};
template <class T> struct EnableIf<true, T> { using type = T; };

/// IsSame evaluates to `true` if the provided types `T` and `U` are the same, `false` otherwise.
template <typename T, typename U>   struct IsSame       { static constexpr bool value = false; };
template <typename T>               struct IsSame<T, T> { static constexpr bool value = true;  };

/// AddPointer adds a pointer qualification to a type `T` if it is not already a pointer.
template <class T> struct AddPointer      { using type = typename RemoveReference<T>::type*; };

/// RemoveConst removes the const qualification from a type `T`.
template <class T> struct RemoveConst           { using type = T;};
template <class T> struct RemoveConst<const T>  { using type = T; };

/// IsConst evaluates to `true` if the provided type `T` is `const`, `false` otherwise.
template <typename T> struct IsConst            { using type = T; static constexpr bool value = false; };
template <typename T> struct IsConst<const T>   { using type = T; static constexpr bool value = true;  };

/// IsTriviallyCopyable evaluates to `true` if the type `T` can be trivially copied, `false` otherwise.
template <typename T> struct IsTriviallyCopyable { static constexpr bool value = __is_trivially_copyable(T); };

/// Conditional defines a type to be `T` if a boolean value is `true`, `F` otherwise.
template <bool B, class T, class F> struct Conditional { using type = T; };
template <class T, class F>         struct Conditional<false, T, F> { using type = F; };

/// SameConstnessAs modifies type `T` to have the const-qualification of `U`.
template <typename U, typename T> struct SameConstnessAs { using type = typename Conditional<IsConst<U>::value, const T, T>::type; };

//! @}
// clang-format on
} // namespace TypeTraits
} // namespace SC
