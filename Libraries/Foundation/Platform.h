// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define SC_DEBUG   1
#define SC_RELEASE 0
#else
#define SC_DEBUG   0
#define SC_RELEASE 1
#endif

#if __APPLE__
#define SC_PLATFORM_APPLE      1
#define SC_PLATFORM_LINUX      0
#define SC_PLATFORM_WINDOWS    0
#define SC_PLATFORM_EMSCRIPTEN 0
#elif _WIN32 || _WIN64
#define SC_PLATFORM_APPLE      0
#define SC_PLATFORM_LINUX      0
#define SC_PLATFORM_WINDOWS    1
#define SC_PLATFORM_EMSCRIPTEN 0
#elif __EMSCRIPTEN__
#define SC_PLATFORM_APPLE      0
#define SC_PLATFORM_LINUX      0
#define SC_PLATFORM_WINDOWS    0
#define SC_PLATFORM_EMSCRIPTEN 1
#elif __linux__
#define SC_PLATFORM_APPLE      0
#define SC_PLATFORM_LINUX      1
#define SC_PLATFORM_WINDOWS    0
#define SC_PLATFORM_EMSCRIPTEN 0
#endif
