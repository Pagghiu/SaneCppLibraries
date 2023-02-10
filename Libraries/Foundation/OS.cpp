// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "OS.h"

#if SC_PLATFORM_WINDOWS
#include "OSInternalWindows.inl"
#elif SC_PLATFORM_EMSCRIPTEN
#include "OSInternalEmscripten.inl"
#else
#include "OSInternalPosix.inl"
#endif

bool SC::OSPaths::close() { return true; }

bool SC::printBacktrace() { return OS::printBacktrace(); }
