// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_COMPILER_MOVE_DEFINITION_H
#if SC_FOUNDATION_COMPILER_MOVE_DEFINITION_H != 1
#error "CompilerMove.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_COMPILER_MOVE_DEFINITION_H 1 // Increment to indicate a new version of the file
namespace SC
{

// clang-format off
namespace TypeTraits
{
//! @addtogroup group_foundation_type_traits
//! @{
template <class T> struct RemoveReference       { using type = T; };
template <class T> struct RemoveReference<T&>   { using type = T; };
template <class T> struct RemoveReference<T&&>  { using type = T; };
/// Determines if a type is an lvalue reference.
template <class T> struct IsLValueReference     { static constexpr bool value = false; };
template <class T> struct IsLValueReference<T&> { static constexpr bool value = true; };
/// Determines if a type is an rvalue reference.
template <class T> struct IsRValueReference     { static constexpr bool value = false; };
template <class T> struct IsRValueReference<T&&>{ static constexpr bool value = true; };
//! @}

} // namespace TypeTraits
//! @defgroup group_foundation_utility Language Utilities
//! @ingroup group_foundation
//! Language Utilities
/// Utility classes allowing common C++ constructs.

//! @addtogroup group_foundation_utility
//! @{

/// Converts an lvalue to an rvalue reference.
template <typename T> constexpr T&& move(T& value) { return static_cast<T&&>(value); }

/// Forwards an lvalue or an rvalue as an rvalue reference.
template <typename T> constexpr T&& forward(typename TypeTraits::RemoveReference<T>::type& value) { return static_cast<T&&>(value); }

/// Forwards an rvalue as an rvalue reference, with a check that it's not an lvalue reference.
template <typename T> constexpr T&& forward(typename TypeTraits::RemoveReference<T>::type&& value)
{
    static_assert(!TypeTraits::IsLValueReference<T>::value, "Forward an rvalue as an lvalue is not allowed");
    return static_cast<T&&>(value);
}

/// Swaps the values of two objects.
template <typename T> constexpr inline void swap(T& t1, T& t2) { T temp = move(t1); t1 = move(t2); t2 = move(temp); }
//! @}
// clang-format on
} // namespace SC

#endif // SC_FOUNDATION_COMPILER_MOVE_DEFINITION_H
