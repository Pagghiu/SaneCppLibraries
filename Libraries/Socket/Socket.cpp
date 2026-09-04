// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "Socket.h"

#define SC_ASSERT_PROVIDER SocketAssert
#include "../Common/Assert.inl"

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
    SocketAddress socketInfo;

    socklen_t socketInfoLen = sizeof(socketInfo.handle);
    if (::getsockname(handle, &socketInfo.handle.reinterpret_as<struct sockaddr>(), &socketInfoLen) == SOCKET_ERROR)
    {
        return Result::Error("getsockname failed");
    }
    socketInfo.nativeSize = socketInfoLen;
    addressFamily         = socketInfo.getAddressFamily();
    return Result(true);
}
