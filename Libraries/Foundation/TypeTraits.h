// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/PrimitiveTypes.h"
//!
//! @defgroup group_foundation Foundation
//! @copybrief library_foundation
//!
//! See @ref library_foundation library page for more details.<br>

//! @defgroup group_foundation_type_traits Type Traits
//! @ingroup group_foundation
//! Metaprogramming constructs (SC::EnableIf, SC::AddPointer, SC::RemovePointer, etc.)

namespace SC
{
// clang-format off
//! @addtogroup group_foundation_type_traits
//! @{

/// EnableIf is a metafunction that conditionally defines a type if a boolean template parameter is true.
template <bool B, class T = void> struct EnableIf {};
template <class T> struct EnableIf<true, T> { using type = T; };

/// IsSame is a trait that evaluates to `true` if the provided types `T` and `U` are the same, `false` otherwise.
template <typename T, typename U>   struct IsSame       { static constexpr bool value = false; };
template <typename T>               struct IsSame<T, T> { static constexpr bool value = true;  };

/// RemovePointer is a metafunction that removes the pointer qualification from a type `T`.
template <class T> struct RemovePointer       { using type = T; };
template <class T> struct RemovePointer<T*>   { using type = T; };
template <class T> struct RemovePointer<T**>  { using type = T; };

/// AddReference is a metafunction that adds a reference qualifier to a type `T` if it is not already a reference.
template <class T> struct AddReference      { using type = T&; };
template <class T> struct AddReference<T&>  { using type = T ; };
template <class T> struct AddReference<T&&> { using type = T ; };

/// AddPointer is a metafunction that adds a pointer qualification to a type `T` if it is not already a pointer.
template <class T> struct AddPointer      { using type = typename RemoveReference<T>::type*; };

/// RemoveConst is a metafunction that removes the const qualification from a type `T`.
template <class T> struct RemoveConst           { using type = T;};
template <class T> struct RemoveConst<const T>  { using type = T; };

/// ReturnType is a trait that extracts the return type from different forms of function types.
template <typename R>                               struct ReturnType;
template <typename R, typename... Args>             struct ReturnType<R(Args...)>       { using type = R; };
template <typename R, typename... Args>             struct ReturnType<R(*)(Args...)>    { using type = R; };
template <typename R, typename C, typename... Args> struct ReturnType<R(C::*)(Args...)> { using type = R; };

/// IsConst is a trait that evaluates to `true` if the provided type `T` is `const`, `false` otherwise.
template <typename T> struct IsConst            { using type = T; static constexpr bool value = false; };
template <typename T> struct IsConst<const T>   { using type = T; static constexpr bool value = true;  };

/// IsTriviallyCopyable is a trait that evaluates to `true` if the type `T` can be trivially copied, `false` otherwise.
template <typename T> struct IsTriviallyCopyable { static constexpr bool value = __is_trivially_copyable(T); };

/// IsReference is a trait that evaluates to `true` if the type `T` is a reference, `false` otherwise.
template <class T> struct IsReference { static constexpr bool value = IsLValueReference<T>::value || IsRValueReference<T>::value; };

/// Conditional is a metafunction that defines a type to be `T` if a boolean value is `true`, `F` otherwise.
template <bool B, class T, class F> struct Conditional { using type = T; };
template <class T, class F>         struct Conditional<false, T, F> { using type = F; };

/// ConditionalT is an alias template that resolves to type `T` if a boolean value is `true`, otherwise to type `F`.
template <bool B, class T, class F> using ConditionalT = typename Conditional<B,T,F>::type;

/// SameConstnessAs is a metafunction that modifies type `T` to have the const-qualification of `U`.
template <typename U, typename T> struct SameConstnessAs { using type = typename Conditional<IsConst<U>::value, const T, T>::type; };

/// SizeOfArray is a constexpr function that returns the compile-time size `N` of a plain C array.
template <typename T, size_t N> constexpr auto SizeOfArray(const T (&)[N]) { return N; }

//! @}

// clang-format on
} // namespace SC

// Defining a constexpr destructor is C++ 20+
#if SC_LANGUAGE_CPP_AT_LEAST_20
#define SC_LANGUAGE_CONSTEXPR_DESTRUCTOR constexpr
#else
#define SC_LANGUAGE_CONSTEXPR_DESTRUCTOR
#endif
