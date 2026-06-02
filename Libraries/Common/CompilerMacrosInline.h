// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#if defined(SC_FOUNDATION_COMPILER_MACROS_INLINE_DEFINITION_H)
#if SC_FOUNDATION_COMPILER_MACROS_INLINE_DEFINITION_H != 1
#error "CompilerMacrosInline.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_COMPILER_MACROS_INLINE_DEFINITION_H 1 // Increment to indicate a new version of the file

#if defined(_MSC_VER) && !defined(__clang__)
#define SC_COMPILER_FORCE_INLINE __forceinline ///< Macro for forcing inline functions.
#else
#define SC_COMPILER_FORCE_INLINE __attribute__((always_inline)) inline ///< Macro for forcing inline functions.
#endif

#endif // SC_FOUNDATION_COMPILER_MACROS_INLINE_DEFINITION_H
