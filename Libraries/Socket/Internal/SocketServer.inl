// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/Assert.h"
#include "../../Socket/Socket.h"
#include "SocketInternal.h"

#if !SC_PLATFORM_WINDOWS
#include <sys/socket.h> // bind
#endif

SC::Result SC::SocketServer::close() { return socket.close(); }

// TODO: Add EINTR checks for all SocketServer/SocketClient os calls.

SC::Result SC::SocketServer::bind(SocketIPAddress nativeAddress)
{
    SC_TRY(SocketNetworking::isNetworkingInited());
    SC_TRY_MSG(socket.isValid(), "Invalid socket");
    SocketDescriptor::Handle listenSocket;
    SC_TRUST_RESULT(socket.get(listenSocket, Result::Error("invalid listen socket")));

    // TODO: Expose SO_REUSEADDR as an option?
    int value = 1;
#if SC_PLATFORM_WINDOWS
    ::setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&value), sizeof(value));
#elif !SC_PLATFORM_EMSCRIPTEN
    ::setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
#else
    SC_COMPILER_UNUSED(value);
#endif
    const struct sockaddr* sa     = &nativeAddress.handle.reinterpret_as<const struct sockaddr>();
    const socklen_t        saSize = nativeAddress.sizeOfHandle();
    if (::bind(listenSocket, sa, saSize) == SOCKET_ERROR)
    {
        SC_TRUST_RESULT(socket.close());
        return Result::Error("Could not bind socket to port");
    }
    return Result(true);
}

SC::Result SC::SocketServer::listen(uint32_t numberOfWaitingConnections)
{
    SC_TRY(SocketNetworking::isNetworkingInited());
    SC_TRY_MSG(socket.isValid(), "Invalid socket");
    SocketDescriptor::Handle listenSocket;
    SC_TRUST_RESULT(socket.get(listenSocket, Result::Error("invalid listen socket")));
    if (::listen(listenSocket, static_cast<int>(numberOfWaitingConnections)) == SOCKET_ERROR)
    {
        SC_TRUST_RESULT(socket.close());
        return Result::Error("Could not listen");
    }
    return Result(true);
}

SC::Result SC::SocketServer::accept(SocketFlags::AddressFamily addressFamily, SocketDescriptor& newClient)
{
    SC_TRY_MSG(not newClient.isValid(), "destination socket already in use");
    SocketDescriptor::Handle listenDescriptor;
    SC_TRY(socket.get(listenDescriptor, Result::Error("Invalid socket")));
    SocketIPAddress nativeAddress(addressFamily);
    socklen_t       nativeSize = nativeAddress.sizeOfHandle();

    SocketDescriptor::Handle acceptedClient =
        ::accept(listenDescriptor, &nativeAddress.handle.reinterpret_as<struct sockaddr>(), &nativeSize);
    SC_TRY_MSG(acceptedClient != SocketDescriptor::Invalid, "accept failed");
    return newClient.assign(acceptedClient);
}
