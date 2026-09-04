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

SC::Result SC::SocketServer::close() { return socket.close(); }

// TODO: Add EINTR checks for all SocketServer/SocketClient os calls.

SC::Result SC::SocketServer::bind(const SocketAddress& nativeAddress, BindReuseAddress reuseAddress,
                                  BindStatus* outStatus)
{
    if (outStatus != nullptr)
    {
        *outStatus = BindStatus::None;
    }

    SC_TRY(SocketNetworking::isNetworkingInited());
    SC_TRY_MSG(socket.isValid(), "Invalid socket");
    SC_TRY_MSG(nativeAddress.isValid(), "invalid bind address");
    SocketDescriptor::Handle listenSocket;
    SC_SOCKET_TRUST_RESULT(socket.get(listenSocket, Result::Error("invalid listen socket")));

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
        if (outStatus != nullptr and WSAGetLastError() == WSAEADDRINUSE)
        {
            *outStatus = BindStatus::AddressInUse;
        }
#elif !SC_PLATFORM_EMSCRIPTEN
        if (outStatus != nullptr and errno == EADDRINUSE)
        {
            *outStatus = BindStatus::AddressInUse;
        }
#endif
        return Result::Error("Could not bind socket to address");
    }
    return Result(true);
}

SC::Result SC::SocketServer::bind(SocketIPAddress nativeAddress, BindReuseAddress reuseAddress, BindStatus* outStatus)
{
    return bind(SocketAddress(nativeAddress), reuseAddress, outStatus);
}

SC::Result SC::SocketServer::listen(uint32_t numberOfWaitingConnections)
{
    SC_TRY(SocketNetworking::isNetworkingInited());
    SC_TRY_MSG(socket.isValid(), "Invalid socket");
    SocketDescriptor::Handle listenSocket;
    SC_SOCKET_TRUST_RESULT(socket.get(listenSocket, Result::Error("invalid listen socket")));
    SC_TRY_MSG(::listen(listenSocket, static_cast<int>(numberOfWaitingConnections)) != SOCKET_ERROR, "listen failed");
    return Result(true);
}

SC::Result SC::SocketServer::accept(SocketFlags::AddressFamily addressFamily, SocketDescriptor& newClient)
{
    (void)addressFamily;
    return accept(newClient, nullptr);
}

SC::Result SC::SocketServer::accept(SocketDescriptor& newClient, SocketAddress* peerAddress)
{
    SC_TRY_MSG(not newClient.isValid(), "destination socket already in use");
    SocketDescriptor::Handle listenDescriptor;
    SC_TRY(socket.get(listenDescriptor, Result::Error("Invalid socket")));

    SocketAddress receivedPeerAddress;
    socklen_t     nativeSize = sizeof(receivedPeerAddress.handle);
    sockaddr*     nativeAddress =
        peerAddress == nullptr ? nullptr : &receivedPeerAddress.handle.reinterpret_as<struct sockaddr>();
    socklen_t* nativeSizePointer = peerAddress == nullptr ? nullptr : &nativeSize;

    SocketDescriptor::Handle acceptedClient = ::accept(listenDescriptor, nativeAddress, nativeSizePointer);
    SC_TRY_MSG(acceptedClient != SocketDescriptor::Invalid, "accept failed");
    SC_TRY(newClient.assign(acceptedClient));
    if (peerAddress != nullptr)
    {
        receivedPeerAddress.nativeSize = nativeSize;
        *peerAddress                   = receivedPeerAddress;
    }
    return Result(true);
}
