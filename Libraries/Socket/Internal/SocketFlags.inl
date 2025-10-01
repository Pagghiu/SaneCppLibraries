// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/Assert.h"
#include "../../Socket/Socket.h"

#if !SC_PLATFORM_WINDOWS
#include <netdb.h> // AF_INET / IPPROTO_TCP / AF_UNSPEC
#endif

SC::SocketFlags::AddressFamily SC::SocketFlags::AddressFamilyFromInt(int value)
{
    return value == AF_INET ? SocketFlags::AddressFamilyIPV4 : SocketFlags::AddressFamilyIPV6;
}

unsigned char SC::SocketFlags::toNative(SocketFlags::AddressFamily type)
{
    return type == SocketFlags::AddressFamilyIPV4 ? AF_INET : AF_INET6;
}

int SC::SocketFlags::toNative(SocketType type) { return type == SocketStream ? SOCK_STREAM : SOCK_DGRAM; }

int SC::SocketFlags::toNative(ProtocolType family) { return family == ProtocolTcp ? IPPROTO_TCP : IPPROTO_UDP; }
