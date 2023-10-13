// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#if defined(SC_COMPILER_ENABLE_CONFIG)
#include "SCConfig.h"
#endif

// Define SC_COMPILER_CLANG, SC_COMPILER_GCC, SC_COMPILER_MSVC, SC_COMPILER_CLANG_CL
#if __clang__
#define SC_COMPILER_CLANG 1
#define SC_COMPILER_GCC   0
#define SC_COMPILER_MSVC  0
#if _MSC_VER
#define SC_COMPILER_CLANG_CL 1
#else
#define SC_COMPILER_CLANG_CL 0
#endif
#elif _MSC_VER
#define SC_COMPILER_CLANG    0
#define SC_COMPILER_GCC      0
#define SC_COMPILER_MSVC     1
#define SC_COMPILER_CLANG_CL 0
#else
#define SC_COMPILER_CLANG    0
#define SC_COMPILER_GCC      1
#define SC_COMPILER_MSVC     0
#define SC_COMPILER_CLANG_CL 0
#endif

// Define SC_COMPILER_FORCE_INLINE and SC_COMPILER_DEBUG_BREAK
#if SC_COMPILER_MSVC

#define SC_COMPILER_FORCE_INLINE __forceinline
#define SC_COMPILER_DEBUG_BREAK  __debugbreak()

#else

#define SC_COMPILER_FORCE_INLINE __attribute__((always_inline)) inline
#if defined(__has_builtin)
#if __has_builtin(__builtin_debugtrap)
#define SC_COMPILER_DEBUG_BREAK __builtin_debugtrap()
#elif __has_builtin(__builtin_trap)
#define SC_COMPILER_DEBUG_BREAK __builtin_trap()
#else
#error "No __builtin_trap or __builtin_debugtrap"
#endif
#else
#error "No __has_builtin"
#endif

#endif

// Define SC_COMPILER_EXPORT
#if SC_COMPILER_MSVC

#if defined(SC_PLUGIN_LIBRARY)
#define SC_COMPILER_EXPORT __declspec(dllimport)
#else
#define SC_COMPILER_EXPORT __declspec(dllexport)
#endif

#else

#if defined(SC_PLUGIN_LIBRARY)
#define SC_COMPILER_EXPORT
#else
#define SC_COMPILER_EXPORT __attribute__((visibility("default")))
#endif

#endif

// Define SC_COMPILER_LIBRARY_PATH (from SC_LIBRARY_PATH)
#if defined(SC_LIBRARY_PATH)
#define SC_COMPILER_MACRO_ESCAPE(input)          __SC_COMPILER_MACRO_ESCAPE_HELPER(input)
#define __SC_COMPILER_MACRO_ESCAPE_HELPER(input) #input
#define __SC_COMPILER_MACRO_TO_LITERAL(string)   #string
#define SC_COMPILER_MACRO_TO_LITERAL(string)     __SC_COMPILER_MACRO_TO_LITERAL(string)

#define SC_COMPILER_LIBRARY_PATH SC_COMPILER_MACRO_TO_LITERAL(SC_COMPILER_MACRO_ESCAPE(SC_LIBRARY_PATH))
#endif

// Define SC_COMPILER_ASAN
#if defined(__SANITIZE_ADDRESS__)
#define SC_COMPILER_ASAN 1
#else
#define SC_COMPILER_ASAN 0
#endif

// Define SC_COMPILER_WARNING_POP
#if defined(__clang__)
#define SC_COMPILER_WARNING_POP _Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
#define SC_COMPILER_WARNING_POP _Pragma("GCC diagnostic pop")
#else
#define SC_COMPILER_WARNING_POP _Pragma("warning(pop)")
#endif

// Define SC_COMPILER_OFFSETOF, SC_COMPILER_WARNING_PUSH_OFFSETOF and
#define SC_COMPILER_OFFSETOF(Class, Field) __builtin_offsetof(Class, Field)
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

// Define SC_COMPILER_UNUSED, SC_COMPILER_WARNING_PUSH_UNUSED_RESULT
#define SC_COMPILER_UNUSED(param) ((void)param)

#if SC_COMPILER_CLANG
#define SC_COMPILER_WARNING_PUSH_UNUSED_RESULT                                                                         \
    _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Wunused-result\"")
#elif SC_COMPILER_GCC
#define SC_COMPILER_WARNING_PUSH_UNUSED_RESULT                                                                         \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wunused-result\"")
#else
#define SC_COMPILER_WARNING_PUSH_UNUSED_RESULT _Pragma("warning(push)") _Pragma("warning(disable : 4834)")
#endif

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

#define SC_LANGUAGE_CPP_LESS_THAN_20 1
#define SC_LANGUAGE_CPP_AT_LEAST_20  0
#define SC_LANGUAGE_CPP_AT_LEAST_17  0
#define SC_LANGUAGE_CPP_AT_LEAST_14  0

#endif

#undef SC_LANGUAGE_CPP_VERSION

#if (not SC_COMPILER_MSVC) or SC_LANGUAGE_CPP_AT_LEAST_20
#define SC_LANGUAGE_LIKELY   [[likely]]
#define SC_LANGUAGE_UNLIKELY [[unlikely]]
#else
#define SC_LANGUAGE_LIKELY
#define SC_LANGUAGE_UNLIKELY
#endif

// clang-format off
namespace SC
{
template <class T> struct RemoveReference       { using type = T; };
template <class T> struct RemoveReference<T&>   { using type = T; };
template <class T> struct RemoveReference<T&&>  { using type = T; };
template <class T> struct IsLValueReference     { static constexpr bool value = false; };
template <class T> struct IsLValueReference<T&> { static constexpr bool value = true; };
template <class T> struct IsRValueReference     { static constexpr bool value = false; };
template <class T> struct IsRValueReference<T&&>{ static constexpr bool value = true; };

template <typename T> constexpr T&& move(T& value) { return static_cast<T&&>(value); }
template <typename T> constexpr T&& forward(typename RemoveReference<T>::type& value) { return static_cast<T&&>(value); }
template <typename T> constexpr T&& forward(typename RemoveReference<T>::type&& value)
{
    static_assert(!IsLValueReference<T>::value, "Forward an rvalue as an lvalue is not allowed");
    return static_cast<T&&>(value);
}

template <typename T> constexpr inline void swap(T& t1, T& t2)
{
    T temp = move(t1);
    t1     = move(t2);
    t2     = move(temp);
}
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
template <typename T> constexpr T min(T t1, T t2) { return t1 < t2 ? t1 : t2; }
template <typename T> constexpr T max(T t1, T t2) { return t1 > t2 ? t1 : t2; }
template <typename F> struct Deferred
{
    Deferred(F&& f) : f(forward<F>(f)) {}
    ~Deferred() { if (armed) f(); }
    void disarm() { armed = false; }

  private:
    F    f;
    bool armed = true;
};
template <typename F> Deferred<F> MakeDeferred(F&& f) { return Deferred<F>(forward<F>(f)); }

}
// clang-format on
