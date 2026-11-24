// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#if defined(SC_FOUNDATION_PLATFORM_MACROS_DEFINITION_H)
#if SC_FOUNDATION_PLATFORM_MACROS_DEFINITION_H != 4
#error "PlatformMacrosType.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_PLATFORM_MACROS_DEFINITION_H 4 // Increment to indicate a new version of the file

#if defined(__APPLE__)
#define SC_PLATFORM_APPLE      1 ///< True (1) when code is compiled on macOS and iOS
#define SC_PLATFORM_LINUX      0 ///< True (1) when code is compiled on Linux
#define SC_PLATFORM_WINDOWS    0 ///< True (1) when code is compiled on Windows
#define SC_PLATFORM_EMSCRIPTEN 0 ///< True (1) when code is compiled on Emscripten
#define SC_PLATFORM_NETBSD     0 ///< True (1) when code is compiled on NetBSD
#define SC_PLATFORM_FREEBSD    0 ///< True (1) when code is compiled on FreeBSD
#define SC_PLATFORM_OPENBSD    0 ///< True (1) when code is compiled on OpenBSD
#elif defined(_WIN32) || defined(_WIN64)
#define SC_PLATFORM_APPLE      0 ///< True (1) when code is compiled on macOS and iOS
#define SC_PLATFORM_LINUX      0 ///< True (1) when code is compiled on Linux
#define SC_PLATFORM_WINDOWS    1 ///< True (1) when code is compiled on Windows
#define SC_PLATFORM_EMSCRIPTEN 0 ///< True (1) when code is compiled on Emscripten
#define SC_PLATFORM_NETBSD     0 ///< True (1) when code is compiled on NetBSD
#define SC_PLATFORM_FREEBSD    0 ///< True (1) when code is compiled on FreeBSD
#define SC_PLATFORM_OPENBSD    0 ///< True (1) when code is compiled on OpenBSD
#elif defined(__EMSCRIPTEN__)
#define SC_PLATFORM_APPLE      0 ///< True (1) when code is compiled on macOS and iOS
#define SC_PLATFORM_LINUX      0 ///< True (1) when code is compiled on Linux
#define SC_PLATFORM_WINDOWS    0 ///< True (1) when code is compiled on Windows
#define SC_PLATFORM_EMSCRIPTEN 1 ///< True (1) when code is compiled on Emscripten
#define SC_PLATFORM_NETBSD     0 ///< True (1) when code is compiled on NetBSD
#define SC_PLATFORM_FREEBSD    0 ///< True (1) when code is compiled on FreeBSD
#define SC_PLATFORM_OPENBSD    0 ///< True (1) when code is compiled on OpenBSD
#elif defined(__linux__)
#define SC_PLATFORM_APPLE      0
#define SC_PLATFORM_LINUX      1 ///< True (1) when code is compiled on Linux
#define SC_PLATFORM_WINDOWS    0 ///< True (1) when code is compiled on Windows
#define SC_PLATFORM_EMSCRIPTEN 0 ///< True (1) when code is compiled on Emscripten
#define SC_PLATFORM_NETBSD     0 ///< True (1) when code is compiled on NetBSD
#define SC_PLATFORM_FREEBSD    0 ///< True (1) when code is compiled on FreeBSD
#define SC_PLATFORM_OPENBSD    0 ///< True (1) when code is compiled on OpenBSD
#elif defined(__NetBSD__)
#define SC_PLATFORM_APPLE      0
#define SC_PLATFORM_LINUX      0
#define SC_PLATFORM_WINDOWS    0
#define SC_PLATFORM_EMSCRIPTEN 0
#define SC_PLATFORM_NETBSD     1 ///< True (1) when code is compiled on NetBSD
#define SC_PLATFORM_FREEBSD    0 ///< True (1) when code is compiled on FreeBSD
#define SC_PLATFORM_OPENBSD    0 ///< True (1) when code is compiled on OpenBSD
#elif defined(__FreeBSD__)
#define SC_PLATFORM_APPLE      0
#define SC_PLATFORM_LINUX      0
#define SC_PLATFORM_WINDOWS    0
#define SC_PLATFORM_EMSCRIPTEN 0
#define SC_PLATFORM_NETBSD     0
#define SC_PLATFORM_FREEBSD    1 ///< True (1) when code is compiled on FreeBSD
#define SC_PLATFORM_OPENBSD    0 ///< True (1) when code is compiled on OpenBSD
#elif defined(__OpenBSD__)
#define SC_PLATFORM_APPLE      0
#define SC_PLATFORM_LINUX      0
#define SC_PLATFORM_WINDOWS    0
#define SC_PLATFORM_EMSCRIPTEN 0
#define SC_PLATFORM_NETBSD     0
#define SC_PLATFORM_FREEBSD    0
#define SC_PLATFORM_OPENBSD    1 ///< True (1) when code is compiled on OpenBSD
#else
#error "Unsupported platform"
#endif

#endif // SC_FOUNDATION_PLATFORM_MACROS_DEFINITION_H
