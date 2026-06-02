// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#if defined(SC_FOUNDATION_COMPILER_MACROS_TYPE_DEFINITION_H)
#if SC_FOUNDATION_COMPILER_MACROS_TYPE_DEFINITION_H != 1
#error "CompilerMacrosType.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_COMPILER_MACROS_TYPE_DEFINITION_H 1 // Increment to indicate a new version of the file

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

#if defined(__FILC__)
#define SC_COMPILER_FILC 1
#else
#define SC_COMPILER_FILC 0
#endif

#endif // SC_FOUNDATION_COMPILER_MACROS_TYPE_DEFINITION_H
