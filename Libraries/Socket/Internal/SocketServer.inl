// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Socket/Socket.h"
#include "SocketInternal.h"

#if !SC_PLATFORM_WINDOWS
#include <errno.h> // errno
#endif

#if !SC_PLATFORM_WINDOWS
#include <sys/socket.h> // bind
#endif

SC::ResultSocket SC::SocketServer::close() { return socket.close(); }

// TODO: Add EINTR checks for all SocketServer/SocketClient os calls.

SC::ResultSocket SC::SocketServer::bind(const SocketAddress& nativeAddress, BindReuseAddress reuseAddress,
                                        BindStatus* outStatus)
{
    if (outStatus != nullptr)
    {
        *outStatus = BindStatus::None;
    }

    if (not SocketNetworking::isNetworkingInited())
        return {SocketError::NetworkingNotInitialized, SocketErrorDetail::BindSocket};
    if (not socket.isValid())
        return {SocketError::InvalidSocket, SocketErrorDetail::BindSocket};
    if (not nativeAddress.isValid())
        return {SocketError::InvalidAddress, SocketErrorDetail::BindSocket};
    SocketDescriptor::Handle listenSocket;
    SC_TRY(socket.get(listenSocket, ResultSocket(SocketError::InvalidSocket, SocketErrorDetail::BindSocket)));

    const int value = reuseAddress == BindReuseAddress::Enabled ? 1 : 0;
    if (nativeAddress.getAddressFamily() != SocketFlags::AddressFamilyUnix)
    {
#if SC_PLATFORM_WINDOWS
        ::setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&value), sizeof(value));
#elif !SC_PLATFORM_EMSCRIPTEN
        ::setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
#else
        (void)(value);
#endif
    }
    const struct sockaddr* sa     = &nativeAddress.handle.reinterpret_as<const struct sockaddr>();
    const socklen_t        saSize = nativeAddress.sizeOfHandle();
    if (::bind(listenSocket, sa, saSize) == SOCKET_ERROR)
    {
#if SC_PLATFORM_WINDOWS
        const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
        if (outStatus != nullptr and nativeError == WSAEADDRINUSE)
        {
            *outStatus = BindStatus::AddressInUse;
        }
#elif !SC_PLATFORM_EMSCRIPTEN
        const uint32_t nativeError = static_cast<uint32_t>(errno);
        if (outStatus != nullptr and nativeError == EADDRINUSE)
        {
            *outStatus = BindStatus::AddressInUse;
        }
#endif
#if SC_PLATFORM_WINDOWS
        if (nativeError == WSAEADDRINUSE)
            return ResultSocket::withNativeError(SocketError::AddressInUse, SocketErrorDetail::BindSocket, nativeError);
#elif !SC_PLATFORM_EMSCRIPTEN
        if (nativeError == EADDRINUSE)
            return ResultSocket::withNativeError(SocketError::AddressInUse, SocketErrorDetail::BindSocket, nativeError);
#endif
#if SC_PLATFORM_WINDOWS
        return ResultSocket::withNativeError(SocketError::BindFailed, SocketErrorDetail::BindSocket, nativeError);
#elif !SC_PLATFORM_EMSCRIPTEN
        return ResultSocket::withNativeError(SocketError::BindFailed, SocketErrorDetail::BindSocket, nativeError);
#else
        return {SocketError::BindFailed, SocketErrorDetail::BindSocket};
#endif
    }
    return {};
}

SC::ResultSocket SC::SocketServer::bind(SocketIPAddress nativeAddress, BindReuseAddress reuseAddress,
                                        BindStatus* outStatus)
{
    return bind(SocketAddress(nativeAddress), reuseAddress, outStatus);
}

SC::ResultSocket SC::SocketServer::listen(uint32_t numberOfWaitingConnections)
{
    if (not SocketNetworking::isNetworkingInited())
        return {SocketError::NetworkingNotInitialized, SocketErrorDetail::ListenSocket};
    if (not socket.isValid())
        return {SocketError::InvalidSocket, SocketErrorDetail::ListenSocket};
    SocketDescriptor::Handle listenSocket;
    SC_TRY(socket.get(listenSocket, ResultSocket(SocketError::InvalidSocket, SocketErrorDetail::ListenSocket)));
    if (::listen(listenSocket, static_cast<int>(numberOfWaitingConnections)) == SOCKET_ERROR)
    {
#if SC_PLATFORM_WINDOWS
        const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
#else
        const uint32_t nativeError = static_cast<uint32_t>(errno);
#endif
        return ResultSocket::withNativeError(SocketError::ListenFailed, SocketErrorDetail::ListenSocket, nativeError);
    }
    return {};
}

SC::ResultSocket SC::SocketServer::accept(SocketFlags::AddressFamily addressFamily, SocketDescriptor& newClient)
{
    (void)addressFamily;
    return accept(newClient, nullptr);
}

SC::ResultSocket SC::SocketServer::accept(SocketDescriptor& newClient, SocketAddress* peerAddress)
{
    if (newClient.isValid())
        return {SocketError::DestinationSocketInUse, SocketErrorDetail::AcceptConnection};
    SocketDescriptor::Handle listenDescriptor;
    SC_TRY(socket.get(listenDescriptor, ResultSocket(SocketError::InvalidSocket, SocketErrorDetail::AcceptConnection)));

    SocketAddress receivedPeerAddress;
    socklen_t     nativeSize = sizeof(receivedPeerAddress.handle);
    sockaddr*     nativeAddress =
        peerAddress == nullptr ? nullptr : &receivedPeerAddress.handle.reinterpret_as<struct sockaddr>();
    socklen_t* nativeSizePointer = peerAddress == nullptr ? nullptr : &nativeSize;

    SocketDescriptor::Handle acceptedClient = ::accept(listenDescriptor, nativeAddress, nativeSizePointer);
    if (acceptedClient == SocketDescriptor::Invalid)
    {
#if SC_PLATFORM_WINDOWS
        const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
#else
        const uint32_t nativeError = static_cast<uint32_t>(errno);
#endif
        return ResultSocket::withNativeError(SocketError::AcceptFailed, SocketErrorDetail::AcceptConnection,
                                             nativeError);
    }
    if (not newClient.assign(acceptedClient))
        return {SocketError::DestinationSocketInUse, SocketErrorDetail::AcceptConnection};
    if (peerAddress != nullptr)
    {
        receivedPeerAddress.nativeSize = nativeSize;
        *peerAddress                   = receivedPeerAddress;
    }
    return {};
}
