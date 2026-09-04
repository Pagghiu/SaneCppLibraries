// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Socket/Socket.h"

#if !SC_PLATFORM_WINDOWS
#include <netdb.h> // AF_INET / IPPROTO_TCP / AF_UNSPEC
#endif

SC::SocketFlags::AddressFamily SC::SocketFlags::AddressFamilyFromInt(int value)
{
    if (value == AF_INET)
    {
        return SocketFlags::AddressFamilyIPV4;
    }
    if (value == AF_INET6)
    {
        return SocketFlags::AddressFamilyIPV6;
    }
#if !SC_PLATFORM_WINDOWS && !SC_PLATFORM_EMSCRIPTEN
    if (value == AF_UNIX)
    {
        return SocketFlags::AddressFamilyUnix;
    }
#endif
    SC_SOCKET_ASSERT_RELEASE(false);
    return SocketFlags::AddressFamilyIPV4;
}

unsigned char SC::SocketFlags::toNative(SocketFlags::AddressFamily type)
{
    switch (type)
    {
    case SocketFlags::AddressFamilyIPV4: return AF_INET;
    case SocketFlags::AddressFamilyIPV6: return AF_INET6;
    case SocketFlags::AddressFamilyUnix:
#if !SC_PLATFORM_WINDOWS && !SC_PLATFORM_EMSCRIPTEN
        return AF_UNIX;
#else
        return 0;
#endif
    }
    return 0;
}

int SC::SocketFlags::toNative(SocketType type) { return type == SocketStream ? SOCK_STREAM : SOCK_DGRAM; }

int SC::SocketFlags::toNative(ProtocolType protocol)
{
    switch (protocol)
    {
    case ProtocolDefault: return 0;
    case ProtocolTcp: return IPPROTO_TCP;
    case ProtocolUdp: return IPPROTO_UDP;
    }
    return 0;
}
