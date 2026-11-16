// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "Libraries/Common/PlatformMacrosType.h"
#include "Libraries/Common/PlatformType.h"
#include "Libraries/Common/StringPath.h"
#include "Libraries/Containers/Array.h"

static_assert(SC_PLATFORM_APPLE + SC_PLATFORM_LINUX + SC_PLATFORM_WINDOWS + SC_PLATFORM_EMSCRIPTEN +
                      SC_PLATFORM_NETBSD + SC_PLATFORM_FREEBSD + SC_PLATFORM_OPENBSD ==
                  1,
              "Exactly one host platform must be selected");
#if SC_PLATFORM_APPLE
static_assert(SC::HostPlatform == SC::Platform::Apple, "Platform detection must agree");
#elif SC_PLATFORM_LINUX
static_assert(SC::HostPlatform == SC::Platform::Linux, "Platform detection must agree");
#elif SC_PLATFORM_WINDOWS
static_assert(SC::HostPlatform == SC::Platform::Windows, "Platform detection must agree");
#elif SC_PLATFORM_EMSCRIPTEN
static_assert(SC::HostPlatform == SC::Platform::Emscripten, "Platform detection must agree");
#elif SC_PLATFORM_NETBSD
static_assert(SC::HostPlatform == SC::Platform::NetBSD, "Platform detection must agree");
#elif SC_PLATFORM_FREEBSD
static_assert(SC::HostPlatform == SC::Platform::FreeBSD, "Platform detection must agree");
#elif SC_PLATFORM_OPENBSD
static_assert(SC::HostPlatform == SC::Platform::OpenBSD, "Platform detection must agree");
#endif

// This bounded core check can also run independently while a platform cannot build SCTest yet.
inline bool runPlatformSmoke()
{
    SC::Array<int, 3> values;
    if (not values.push_back(7) or not values.push_back(11) or values.size() != 2 or values[1] != 11)
        return false;
    if (not values.push_back(13) or values.push_back(17) or values.size() != 3)
        return false;

    SC::StringNativeBuffer<8> text;
    return text.assign("bsd") and text.append("!") and text.view() == "bsd!";
}
