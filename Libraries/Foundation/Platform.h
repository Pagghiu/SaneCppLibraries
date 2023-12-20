// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

//! @addtogroup group_foundation_compiler_macros
//! @{

#if defined(DEBUG) || defined(_DEBUG)
#define SC_CONFIGURATION_DEBUG   1 ///< True (1) if release configuration is active (_DEBUG==1 or DEBUG==1)
#define SC_CONFIGURATION_RELEASE 0 ///< True (1) if release configuration is active (!(_DEBUG==1 or DEBUG==1))
#else
#define SC_CONFIGURATION_DEBUG   0 ///< True (1) if debug configuration is active (_DEBUG==1 or DEBUG==1)
#define SC_CONFIGURATION_RELEASE 1 ///< True (1) if release configuration is active (!(_DEBUG==1 or DEBUG==1))
#endif

#if defined(__APPLE__)
#define SC_PLATFORM_APPLE      1 ///< True (1) when code is compiled on macOS and iOS
#define SC_PLATFORM_LINUX      0 ///< True (1) when code is compiled on Linux
#define SC_PLATFORM_WINDOWS    0 ///< True (1) when code is compiled on Windows
#define SC_PLATFORM_EMSCRIPTEN 0 ///< True (1) when code is compiled on Emscripten
#elif defined(_WIN32) || defined(_WIN64)
#define SC_PLATFORM_APPLE      0 ///< True (1) when code is compiled on macOS and iOS
#define SC_PLATFORM_LINUX      0 ///< True (1) when code is compiled on Linux
#define SC_PLATFORM_WINDOWS    1 ///< True (1) when code is compiled on Windows
#define SC_PLATFORM_EMSCRIPTEN 0 ///< True (1) when code is compiled on Emscripten
#elif defined(__EMSCRIPTEN__)
#define SC_PLATFORM_APPLE      0 ///< True (1) when code is compiled on macOS and iOS
#define SC_PLATFORM_LINUX      0 ///< True (1) when code is compiled on Linux
#define SC_PLATFORM_WINDOWS    0 ///< True (1) when code is compiled on Windows
#define SC_PLATFORM_EMSCRIPTEN 1 ///< True (1) when code is compiled on Emscripten
#elif defined(__linux__)
#define SC_PLATFORM_APPLE      0
#define SC_PLATFORM_LINUX      1 ///< True (1) when code is compiled on Linux
#define SC_PLATFORM_WINDOWS    0 ///< True (1) when code is compiled on Windows
#define SC_PLATFORM_EMSCRIPTEN 0 ///< True (1) when code is compiled on Emscripten
#else
#error "Unsupported platform"
#endif

#if defined(_WIN64)
#define SC_PLATFORM_64_BIT 1 ///< True (1) when compiling to a 64 bit platform
#elif defined(_WIN32)
#define SC_PLATFORM_64_BIT 0 ///< True (1) when compiling to a 64 bit platform
#else
#define SC_PLATFORM_64_BIT 1 ///< True (1) when compiling to a 64 bit platform
#endif

#if defined(_M_ARM64) || defined(__aarch64__)
#define SC_PLATFORM_ARM64 1 ///< True (1) when compiling on ARM64, including Apple Silicon
#else
#define SC_PLATFORM_ARM64 0 ///< True (1) when compiling on ARM64, including Apple Silicon
#endif

//! @}
