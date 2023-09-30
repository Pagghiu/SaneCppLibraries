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
