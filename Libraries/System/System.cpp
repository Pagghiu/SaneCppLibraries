// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "System.h"

#if SC_PLATFORM_WINDOWS
#include "SystemInternalWindows.inl"
#elif SC_PLATFORM_EMSCRIPTEN
#include "SystemInternalEmscripten.inl"
#include <emscripten.h>
#elif SC_PLATFORM_APPLE
#include "SystemInternalApple.inl"
#include "SystemInternalPosix.inl"
#endif

#if !SC_PLATFORM_WINDOWS
SC::Result SC::SystemFunctions::initNetworking() { return Result(true); }
SC::Result SC::SystemFunctions::shutdownNetworking() { return Result(true); }
bool       SC::SystemFunctions::isNetworkingInited() { return Result(true); }
#endif

SC::SystemFunctions::~SystemFunctions() { SC_TRUST_RESULT(shutdownNetworking()); }
