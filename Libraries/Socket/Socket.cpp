// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "Socket.h"

#if SC_PLATFORM_WINDOWS
#include "Internal/SocketDescriptorWindows.inl"
#else
#include "Internal/SocketDescriptorPosix.inl"
#endif

#include "Internal/SocketDNS.inl"
#include "Internal/SocketFlags.inl"
#include "Internal/SocketIPAddress.inl"

#include "Internal/SocketClient.inl"
#include "Internal/SocketServer.inl"

SC::Result SC::SocketDescriptor::getAddressFamily(SocketFlags::AddressFamily& addressFamily) const
{
    struct sockaddr_in6 socketInfo;

    socklen_t socketInfoLen = sizeof(socketInfo);
    if (::getsockname(handle, reinterpret_cast<struct sockaddr*>(&socketInfo), &socketInfoLen) == SOCKET_ERROR)
    {
        return Result::Error("getsockname failed");
    }
    addressFamily = SocketFlags::AddressFamilyFromInt(socketInfo.sin6_family);
    return Result(true);
}
