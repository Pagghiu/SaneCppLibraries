// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define SC_CONFIGURATION_DEBUG   1
#define SC_CONFIGURATION_RELEASE 0
#else
#define SC_CONFIGURATION_DEBUG   0
#define SC_CONFIGURATION_RELEASE 1
#endif

#if defined(__APPLE__)
#define SC_PLATFORM_APPLE      1
#define SC_PLATFORM_LINUX      0
#define SC_PLATFORM_WINDOWS    0
#define SC_PLATFORM_EMSCRIPTEN 0
#elif defined(_WIN32) || defined(_WIN64)
#define SC_PLATFORM_APPLE      0
#define SC_PLATFORM_LINUX      0
#define SC_PLATFORM_WINDOWS    1
#define SC_PLATFORM_EMSCRIPTEN 0
#elif defined(__EMSCRIPTEN__)
#define SC_PLATFORM_APPLE      0
#define SC_PLATFORM_LINUX      0
#define SC_PLATFORM_WINDOWS    0
#define SC_PLATFORM_EMSCRIPTEN 1
#elif defined(__linux__)
#define SC_PLATFORM_APPLE      0
#define SC_PLATFORM_LINUX      1
#define SC_PLATFORM_WINDOWS    0
#define SC_PLATFORM_EMSCRIPTEN 0
#else
#error "Unsupported platform"
#endif

#if defined(_WIN64)
#define SC_PLATFORM_64_BIT 1
#elif defined(_WIN32)
#define SC_PLATFORM_64_BIT 0
#else
#define SC_PLATFORM_64_BIT 1
#endif

#if defined(_M_ARM64) || defined(__aarch64__)
#define SC_PLATFORM_ARM64 1
#else
#define SC_PLATFORM_ARM64 0
#endif
