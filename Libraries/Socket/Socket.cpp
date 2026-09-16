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

SC::ResultSocket SC::SocketDescriptor::getAddressFamily(SocketFlags::AddressFamily& addressFamily) const
{
    SocketAddress socketInfo;

    socklen_t socketInfoLen = sizeof(socketInfo.handle);
    if (::getsockname(handle, &socketInfo.handle.reinterpret_as<struct sockaddr>(), &socketInfoLen) == SOCKET_ERROR)
    {
#if SC_PLATFORM_WINDOWS
        const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
#else
        const uint32_t nativeError = static_cast<uint32_t>(errno);
#endif
        return ResultSocket::withNativeError(SocketError::DescriptorQueryFailed, SocketErrorDetail::GetSocketAddress,
                                             nativeError);
    }
    socketInfo.nativeSize = socketInfoLen;
    addressFamily         = socketInfo.getAddressFamily();
    return {};
}
