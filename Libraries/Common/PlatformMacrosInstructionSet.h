// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#if defined(SC_FOUNDATION_PLATFORM_MACROS_INSTRUCTION_SET_DEFINITION_H)
#if SC_FOUNDATION_PLATFORM_MACROS_INSTRUCTION_SET_DEFINITION_H != 1
#error "PlatformMacrosInstructionSet.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_PLATFORM_MACROS_INSTRUCTION_SET_DEFINITION_H 1 // Increment to indicate a new version of the file

#if defined(__SIZEOF_POINTER__)
#if __SIZEOF_POINTER__ == 8
#define SC_PLATFORM_64_BIT 1 ///< True (1) when compiling to a 64 bit platform
#else
#define SC_PLATFORM_64_BIT 0 ///< True (1) when compiling to a 64 bit platform
#endif
#elif defined(_WIN64)
#define SC_PLATFORM_64_BIT 1 ///< True (1) when compiling to a 64 bit platform
#elif defined(_WIN32)
#define SC_PLATFORM_64_BIT 0 ///< True (1) when compiling to a 64 bit platform
#else
#define SC_PLATFORM_64_BIT 1 ///< True (1) when compiling to a 64 bit platform
#endif

#if defined(_M_ARM64) || defined(__aarch64__)
#define SC_PLATFORM_ARM64 1 ///< True (1) when compiling on ARM64, including Apple Silicon
#define SC_PLATFORM_INTEL 0 ///< True (1) when compiling on Intel platforms
#else
#define SC_PLATFORM_ARM64 0 ///< True (1) when compiling on ARM64, including Apple Silicon
#define SC_PLATFORM_INTEL 1 ///< True (1) when compiling on Intel platforms
#endif

#endif // SC_FOUNDATION_PLATFORM_MACROS_INSTRUCTION_SET_DEFINITION_H
