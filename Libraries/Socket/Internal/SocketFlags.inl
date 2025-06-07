// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/Assert.h"
#include "../../Socket/SocketDescriptor.h"

#if !SC_PLATFORM_WINDOWS
#include <netdb.h> // AF_INET / IPPROTO_TCP / AF_UNSPEC
#endif

SC::SocketFlags::AddressFamily SC::SocketFlags::AddressFamilyFromInt(int value)
{
    switch (value)
    {
    case AF_INET: return SocketFlags::AddressFamilyIPV4;
    case AF_INET6: return SocketFlags::AddressFamilyIPV6;
    }
    Assert::unreachable();
}

unsigned char SC::SocketFlags::toNative(SocketFlags::AddressFamily type)
{
    switch (type)
    {
    case SocketFlags::AddressFamilyIPV4: return AF_INET;
    case SocketFlags::AddressFamilyIPV6: return AF_INET6;
    }
    Assert::unreachable();
}

SC::SocketFlags::SocketType SC::SocketFlags::SocketTypeFromInt(int value)
{
    switch (value)
    {
    case SOCK_STREAM: return SocketStream;
    case SOCK_DGRAM: return SocketDgram;
    }
    Assert::unreachable();
}
int SC::SocketFlags::toNative(SocketType type)
{
    switch (type)
    {
    case SocketStream: return SOCK_STREAM;
    case SocketDgram: return SOCK_DGRAM;
    }
    Assert::unreachable();
}

SC::SocketFlags::ProtocolType SC::SocketFlags::ProtocolTypeFromInt(int value)
{
    switch (value)
    {
    case IPPROTO_TCP: return ProtocolTcp;
    case IPPROTO_UDP: return ProtocolUdp;
    }
    Assert::unreachable();
}

int SC::SocketFlags::toNative(ProtocolType family)
{
    switch (family)
    {
    case ProtocolTcp: return IPPROTO_TCP;
    case ProtocolUdp: return IPPROTO_UDP;
    }
    Assert::unreachable();
}
