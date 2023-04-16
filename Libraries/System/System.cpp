// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "System.h"

#if SC_PLATFORM_WINDOWS
#include "SystemInternalWindows.inl"
#elif SC_PLATFORM_EMSCRIPTEN
#include "SystemInternalEmscripten.inl"
#elif SC_PLATFORM_APPLE
#include "SystemInternalApple.inl"
#include "SystemInternalPosix.inl"
#endif

bool SC::printBacktrace() { return SystemDebug::printBacktrace(); }
