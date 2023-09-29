// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#ifdef SC_ENABLE_CONFIG_FILE
#include "SCConfig.h"
#endif

// Mandatory defines
#ifndef SC_ENABLE_STD_CPP_LIBRARY
#define SC_ENABLE_STD_CPP_LIBRARY 0
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

// Define SC_NO_RETURN, SC_ALWAYS_INLINE and SC_BREAK_DEBUGGER
#if SC_COMPILER_MSVC

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
#error "No __builtin_trap or __builtin_debugtrap"
#endif
#else
#error "No __has_builtin"
#endif

#endif

// Define SC_OFFSETOF, SC_DISABLE_OFFSETOF_WARNING and SC_ENABLE_OFFSETOF_WARNING
#define SC_OFFSETOF(Class, Field) __builtin_offsetof(Class, Field)
#if SC_COMPILER_CLANG

#define SC_DISABLE_OFFSETOF_WARNING                                                                                    \
    _Pragma("clang diagnostic push");                                                                                  \
    _Pragma("clang diagnostic ignored \"-Winvalid-offsetof\"");
#define SC_ENABLE_OFFSETOF_WARNING _Pragma("clang diagnostic pop");

#elif SC_COMPILER_GCC
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

// Define SC_EXPORT_SYMBOL
#if SC_COMPILER_MSVC

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
