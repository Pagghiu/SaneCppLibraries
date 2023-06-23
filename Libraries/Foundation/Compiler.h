// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#ifndef SC_DISABLE_CONFIG
#include "SCConfig.h"
#endif
// Compiler name

#if __clang__
#define SC_CLANG 1
#define SC_GCC   0
#define SC_MSVC  0
#if _MSC_VER
#define SC_CLANG_CL 1
#else
#define SC_CLANG_CL 0
#endif
#elif _MSC_VER
#define SC_CLANG    0
#define SC_GCC      0
#define SC_MSVC     1
#define SC_CLANG_CL 0
#else
#define SC_CLANG    0
#define SC_GCC      1
#define SC_MSVC     0
#define SC_CLANG_CL 0
#endif

// Compiler Attributes
#if SC_MSVC
#define SC_ATTRIBUTE_PRINTF(a, b)
#else
#define SC_ATTRIBUTE_PRINTF(a, b) __attribute__((format(printf, a, b)))
#endif

#if SC_MSVC
#define SC_NO_RETURN(func) __declspec(noreturn) func
#define SC_ALWAYS_INLINE   __forceinline
#define SC_BREAK_DEBUGGER  __debugbreak()
#else
#define SC_NO_RETURN(func) func __attribute__((__noreturn__))
#define SC_ALWAYS_INLINE   __attribute__((always_inline)) inline
#ifdef __has_builtin
#if __has_builtin(__builtin_debugtrap)
#define SC_BREAK_DEBUGGER __builtin_debugtrap()
#elif __has_builtin(__builtin_trap)
#define SC_BREAK_DEBUGGER __builtin_trap()
#else
#error "No builtin_trap"
#endif
#else
#error "No builtin_trap"
#endif
#endif

#define SC_OFFSETOF(Class, Field) __builtin_offsetof(Class, Field)
#if SC_CLANG

#define SC_DISABLE_OFFSETOF_WARNING                                                                                    \
    _Pragma("clang diagnostic push");                                                                                  \
    _Pragma("clang diagnostic ignored \"-Winvalid-offsetof\"");
#define SC_ENABLE_OFFSETOF_WARNING _Pragma("clang diagnostic pop");

#elif SC_GCC
#define SC_DISABLE_OFFSETOF_WARNING                                                                                    \
    _Pragma("GCC diagnostic push");                                                                                    \
    _Pragma("GCC diagnostic ignored \"-Winvalid-offsetof\"");
#define SC_ENABLE_OFFSETOF_WARNING _Pragma("GCC diagnostic pop");

#else

#define SC_DISABLE_OFFSETOF_WARNING
#define SC_ENABLE_OFFSETOF_WARNING

#endif

#ifdef __SANITIZE_ADDRESS__
#define SC_ADDRESS_SANITIZER 1
#else
#define SC_ADDRESS_SANITIZER 0
#endif

#if SC_MSVC
#if defined(SC_PLUGIN_LIBRARY)
#define SC_EXPORT_SYMBOL __declspec(dllimport)
#else
#define SC_EXPORT_SYMBOL __declspec(dllexport)
#endif
#else
#if defined(SC_PLUGIN_LIBRARY)
#define SC_EXPORT_SYMBOL
#else
#define SC_EXPORT_SYMBOL __attribute__((visibility("default")))
#endif
#endif
