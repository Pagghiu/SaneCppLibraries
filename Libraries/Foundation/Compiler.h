// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#if defined(SC_COMPILER_ENABLE_CONFIG)
#include "SCConfig.h"
#endif
//! @defgroup group_foundation_compiler_macros Compiler Macros
//! @ingroup group_foundation
//! Compiler Macros
/// Preprocessor macros to detect compiler and platform features.

//! @addtogroup group_foundation_compiler_macros
//! @{

#if __clang__
#define SC_COMPILER_CLANG 1 ///< Flag indicating Clang compiler.
#define SC_COMPILER_GCC   0 ///< Flag indicating GCC compiler.
#define SC_COMPILER_MSVC  0 ///< Flag indicating MSVC compiler.

#if _MSC_VER
#define SC_COMPILER_CLANG_CL 1 ///< Flag indicating Clang-CL (MSVC) compiler.
#else
#define SC_COMPILER_CLANG_CL 0 ///< Flag indicating non-Clang-CL (MSVC) compiler.
#endif

#elif _MSC_VER
#define SC_COMPILER_CLANG    0 ///< Flag indicating Clang compiler.
#define SC_COMPILER_GCC      0 ///< Flag indicating GCC compiler.
#define SC_COMPILER_MSVC     1 ///< Flag indicating MSVC compiler.
#define SC_COMPILER_CLANG_CL 0 ///< Flag indicating non-Clang-CL (MSVC) compiler.

#else
#define SC_COMPILER_CLANG    0 ///< Flag indicating Clang compiler.
#define SC_COMPILER_GCC      1 ///< Flag indicating GCC compiler.
#define SC_COMPILER_MSVC     0 ///< Flag indicating MSVC compiler.
#define SC_COMPILER_CLANG_CL 0 ///< Flag indicating non-Clang-CL (MSVC) compiler.
#endif

#if SC_COMPILER_MSVC

#define SC_COMPILER_FORCE_INLINE __forceinline  ///< Macro for forcing inline functions.
#define SC_COMPILER_DEBUG_BREAK  __debugbreak() ///< Macro for breaking into debugger in MSVC.

#else

#define SC_COMPILER_FORCE_INLINE __attribute__((always_inline)) inline ///< Macro for forcing inline functions.
#if defined(__has_builtin)
#if __has_builtin(__builtin_debugtrap)
#define SC_COMPILER_DEBUG_BREAK __builtin_debugtrap() ///< Macro for breaking into debugger in non-MSVC compilers.
#elif __has_builtin(__builtin_trap)
#define SC_COMPILER_DEBUG_BREAK __builtin_trap() ///< Macro for breaking into debugger in non-MSVC compilers.
#else
#error "No __builtin_trap or __builtin_debugtrap"
#endif
#else
#error "No __has_builtin"
#endif

#endif

/// Define compiler-specific export macros for DLL visibility.
#if SC_COMPILER_MSVC

#if defined(SC_PLUGIN_LIBRARY)
#define SC_COMPILER_EXPORT __declspec(dllimport) ///< Macro for importing symbols from DLL in MSVC.
#else
#define SC_COMPILER_EXPORT __declspec(dllexport) ///< Macro for exporting symbols from DLL in MSVC.
#endif

#else

#if defined(SC_PLUGIN_LIBRARY)
#define SC_COMPILER_EXPORT ///< Macro for handling symbol visibility for plugin library.
#else
#define SC_COMPILER_EXPORT                                                                                             \
    __attribute__((visibility("default"))) ///< Macro for symbol visibility in non-MSVC compilers.
#endif

#endif

#if !DOXYGEN && defined(SC_LIBRARY_PATH)
#define SC_COMPILER_MACRO_ESCAPE(input)          __SC_COMPILER_MACRO_ESCAPE_HELPER(input) ///< Macro to escape input.
#define __SC_COMPILER_MACRO_ESCAPE_HELPER(input) #input  ///< Macro helper for escaping input.
#define __SC_COMPILER_MACRO_TO_LITERAL(string)   #string ///< Macro for string to literal conversion.
#define SC_COMPILER_MACRO_TO_LITERAL(string)                                                                           \
    __SC_COMPILER_MACRO_TO_LITERAL(string) ///< Macro for string to literal conversion.
#define SC_COMPILER_LIBRARY_PATH                                                                                       \
    SC_COMPILER_MACRO_TO_LITERAL(SC_COMPILER_MACRO_ESCAPE(SC_LIBRARY_PATH)) ///< Macro to get literal library path.
#endif

/// Evaluates to true (1) if ASAN is active
#if defined(__SANITIZE_ADDRESS__)
#define SC_COMPILER_ASAN 1 ///< Flag indicating the availability of Address Sanitizer (ASAN).
#else
#define SC_COMPILER_ASAN 0 ///< Flag indicating the absence of Address Sanitizer (ASAN).
#endif

/// Pops warning from inside a macro
#if defined(__clang__)
#define SC_COMPILER_WARNING_POP _Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
#define SC_COMPILER_WARNING_POP _Pragma("GCC diagnostic pop")
#else
#define SC_COMPILER_WARNING_POP _Pragma("warning(pop)")
#endif

/// Returns offset of Class::Field in bytes
#define SC_COMPILER_OFFSETOF(Class, Field) __builtin_offsetof(Class, Field)

namespace SC
{
template <int offset, typename T, typename R>
T& fieldOffset(R& object)
{
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(&object) - offset);
}
} // namespace SC

#define SC_COMPILER_FIELD_OFFSET(Class, Field, Value)                                                                  \
    SC::fieldOffset<SC_COMPILER_OFFSETOF(Class, Field), Class, decltype(Class::Field)>(Value);

/// Disables invalid-offsetof gcc and clang warning
#if SC_COMPILER_CLANG
#define SC_COMPILER_WARNING_PUSH_OFFSETOF                                                                              \
    _Pragma("clang diagnostic push");                                                                                  \
    _Pragma("clang diagnostic ignored \"-Winvalid-offsetof\"");
#elif SC_COMPILER_GCC
#define SC_COMPILER_WARNING_PUSH_OFFSETOF                                                                              \
    _Pragma("GCC diagnostic push");                                                                                    \
    _Pragma("GCC diagnostic ignored \"-Winvalid-offsetof\"");
#else
#define SC_COMPILER_WARNING_PUSH_OFFSETOF _Pragma("warning(push)")
#endif

/// Silence an `unused variable` or `unused parameter` warning
#define SC_COMPILER_UNUSED(param) ((void)param)

/// Disables `unused-result` warning (due to ignoring a return value marked as `[[nodiscard]]`)
#if SC_COMPILER_CLANG
#define SC_COMPILER_WARNING_PUSH_UNUSED_RESULT                                                                         \
    _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Wunused-result\"")
#elif SC_COMPILER_GCC
#define SC_COMPILER_WARNING_PUSH_UNUSED_RESULT                                                                         \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wunused-result\"")
#else
#define SC_COMPILER_WARNING_PUSH_UNUSED_RESULT _Pragma("warning(push)") _Pragma("warning(disable : 4834 6031)")
#endif

/// `SC_LANGUAGE_FORCE_STANDARD_CPP` == `14`, `17` or `20` to force a specific C++ standard version (`202002L`,
/// `201703L` or `201402L`)
#if defined(SC_LANGUAGE_FORCE_STANDARD_CPP)
#if SC_LANGUAGE_FORCE_STANDARD_CPP == 14
#define SC_LANGUAGE_CPP_VERSION 201402L
#elif SC_LANGUAGE_FORCE_STANDARD_CPP == 17
#define SC_LANGUAGE_CPP_VERSION 201703L
#elif SC_LANGUAGE_FORCE_STANDARD_CPP == 20
#define SC_LANGUAGE_CPP_VERSION 202002L
#else
#error "SC_LANGUAGE_FORCE_STANDARD_CPP has invalid value"
#endif
#else

/// Macro to check `__cplusplus` properly on MSVC
#if SC_COMPILER_MSVC
#define SC_LANGUAGE_CPP_VERSION _MSVC_LANG
#else
#define SC_LANGUAGE_CPP_VERSION __cplusplus
#endif

#endif

#if SC_LANGUAGE_CPP_VERSION >= 202002L
#define SC_LANGUAGE_CPP_LESS_THAN_20 0
#define SC_LANGUAGE_CPP_AT_LEAST_20  1
#define SC_LANGUAGE_CPP_AT_LEAST_17  1
#define SC_LANGUAGE_CPP_AT_LEAST_14  1

#elif SC_LANGUAGE_CPP_VERSION >= 201703L

#define SC_LANGUAGE_CPP_LESS_THAN_20 1
#define SC_LANGUAGE_CPP_AT_LEAST_20  0
#define SC_LANGUAGE_CPP_AT_LEAST_17  1
#define SC_LANGUAGE_CPP_AT_LEAST_14  1

#elif SC_LANGUAGE_CPP_VERSION >= 201402L

#define SC_LANGUAGE_CPP_LESS_THAN_20 1
#define SC_LANGUAGE_CPP_AT_LEAST_20  0
#define SC_LANGUAGE_CPP_AT_LEAST_17  0
#define SC_LANGUAGE_CPP_AT_LEAST_14  1

#else

#define SC_LANGUAGE_CPP_LESS_THAN_20 1 ///< True (1) if C++ standard is  < C++ 20
#define SC_LANGUAGE_CPP_AT_LEAST_20  0 ///< True (1) if C++ standard is <= C++ 20
#define SC_LANGUAGE_CPP_AT_LEAST_17  0 ///< True (1) if C++ standard is <= C++ 17
#define SC_LANGUAGE_CPP_AT_LEAST_14  0 ///< True (1) if C++ standard is <= C++ 14

#endif

#undef SC_LANGUAGE_CPP_VERSION

#if SC_LANGUAGE_CPP_AT_LEAST_20
#define SC_LANGUAGE_LIKELY   [[likely]]   ///<  Use `[[likely]]` if available
#define SC_LANGUAGE_UNLIKELY [[unlikely]] ///<  Use `[[unlikely]]` if available
#else
#define SC_LANGUAGE_LIKELY
#define SC_LANGUAGE_UNLIKELY
#endif

#if SC_LANGUAGE_CPP_AT_LEAST_17
#define SC_LANGUAGE_IF_CONSTEXPR constexpr
#else
#define SC_LANGUAGE_IF_CONSTEXPR
#endif

#if __cpp_exceptions == 199711 || _EXCEPTIONS
#define SC_LANGUAGE_EXCEPTIONS 1
#else
#define SC_LANGUAGE_EXCEPTIONS 0
#endif
//! @}
// clang-format off
namespace SC
{
namespace TypeTraits
{
//! @addtogroup group_foundation_type_traits
//! @{

/// Removes reference from a type T.
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

}
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
template <typename T> constexpr inline void swap(T& t1, T& t2)
{
    T temp = move(t1);
    t1     = move(t2);
    t2     = move(temp);
}
//! @}

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
//! @addtogroup group_algorithms
//! @{

/// Finds the minimum of two values.
template <typename T> constexpr const T& min(const T& t1, const T& t2) { return t1 < t2 ? t1 : t2; }
/// Finds the maximum of two values.
template <typename T> constexpr const T& max(const T& t1, const T& t2) { return t1 > t2 ? t1 : t2; }

//! @}

}
// clang-format on
