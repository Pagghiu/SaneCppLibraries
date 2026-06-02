// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#if defined(SC_FOUNDATION_PLATFORM_MACROS_DEFINITION_H)
#if SC_FOUNDATION_PLATFORM_MACROS_DEFINITION_H != 1
#error "PlatformMacrosType.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_PLATFORM_MACROS_DEFINITION_H 1 // Increment to indicate a new version of the file

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

#endif // SC_FOUNDATION_PLATFORM_MACROS_DEFINITION_H
