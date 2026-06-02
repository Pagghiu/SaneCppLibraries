// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#if defined(SC_FOUNDATION_COMPILER_MACROS_DEBUG_BREAK_DEFINITION_H)
#if SC_FOUNDATION_COMPILER_MACROS_DEBUG_BREAK_DEFINITION_H != 1
#error "CompilerMacrosDebugBreak.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_COMPILER_MACROS_DEBUG_BREAK_DEFINITION_H 1 // Increment to indicate a new version of the file

#if defined(_MSC_VER) && !defined(__clang__)
#define SC_COMPILER_DEBUG_BREAK __debugbreak() ///< Macro for breaking into debugger in MSVC.
#else
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

#endif // SC_FOUNDATION_COMPILER_MACROS_DEBUG_BREAK_DEFINITION_H
