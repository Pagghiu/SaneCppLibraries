// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "System.h"

#if SC_PLATFORM_WINDOWS
#include "Internal/SystemWindows.inl"
#elif SC_PLATFORM_EMSCRIPTEN
#include "Internal/SystemEmscripten.inl"
#include <emscripten.h>
#elif SC_PLATFORM_APPLE
#include "Internal/SystemApple.inl"
#include "Internal/SystemPosix.inl"
#endif
