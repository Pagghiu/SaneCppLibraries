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
SC::ReturnCode SC::SystemFunctions::initNetworking() { return ReturnCode(true); }
SC::ReturnCode SC::SystemFunctions::shutdownNetworking() { return ReturnCode(true); }
bool           SC::SystemFunctions::isNetworkingInited() { return ReturnCode(true); }
#endif

SC::SystemFunctions::~SystemFunctions() { SC_TRUST_RESULT(shutdownNetworking()); }
