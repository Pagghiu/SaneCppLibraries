// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/Assert.h"
#include "../../Socket/Socket.h"
#include "SocketInternal.h"

#include <errno.h> // errno
#if !SC_PLATFORM_WINDOWS
#include <netinet/in.h> // sockaddr_in
#include <sys/select.h> // select
#endif

SC::Result SC::SocketClient::connect(StringSpan address, uint16_t port)
{
    SocketIPAddress nativeAddress;
    SC_TRY(nativeAddress.fromAddressPort(address, port));
    return connect(nativeAddress);
}

SC::Result SC::SocketClient::connect(SocketIPAddress ipAddress)
{
    SC_TRY(SocketNetworking::isNetworkingInited());
    SocketDescriptor::Handle openedSocket;
    SC_TRUST_RESULT(socket.get(openedSocket, Result::Error("invalid connect socket")));
    socklen_t nativeSize = ipAddress.sizeOfHandle();
    int       res;
    do
    {
        res = ::connect(openedSocket, &ipAddress.handle.reinterpret_as<const struct sockaddr>(), nativeSize);
    } while (res == SOCKET_ERROR and errno == EINTR);
    SC_TRY_MSG(res != SOCKET_ERROR, "connect failed");
    return Result(true);
}

SC::Result SC::SocketClient::write(Span<const char> data)
{
    SocketDescriptor::Handle nativeSocket;
    SC_TRY(socket.get(nativeSocket, Result::Error("Invalid socket")));
#if SC_PLATFORM_WINDOWS
    const int sizeInBytes = static_cast<int>(data.sizeInBytes());
#else
    const auto sizeInBytes = data.sizeInBytes();
#endif
    const auto written = ::send(nativeSocket, data.data(), sizeInBytes, 0);
    SC_TRY_MSG(written >= 0, "send error");
    SC_TRY_MSG(static_cast<decltype(data.sizeInBytes())>(written) == data.sizeInBytes(), "send didn't write all bytes");
    return Result(true);
}

SC::Result SC::SocketClient::read(Span<char> data, Span<char>& readData)
{
    SocketDescriptor::Handle nativeSocket;
    SC_TRY(socket.get(nativeSocket, Result::Error("Invalid socket")));
#if SC_PLATFORM_WINDOWS
    const int sizeInBytes = static_cast<int>(data.sizeInBytes());
#else
    const auto sizeInBytes = data.sizeInBytes();
#endif
    const auto recvSize = ::recv(nativeSocket, data.data(), sizeInBytes, 0);
    SC_TRY_MSG(recvSize >= 0, "recv error");
    readData = {data.data(), static_cast<size_t>(recvSize)};
    return Result(true);
}

SC::Result SC::SocketClient::readWithTimeout(Span<char> data, Span<char>& readData, int64_t timeout)
{
    SocketDescriptor::Handle nativeSocket;
    SC_TRY(socket.get(nativeSocket, Result::Error("Invalid socket")));
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(nativeSocket, &fds);

    struct timeval tv;
    tv.tv_sec  = static_cast<int>(timeout / 1000);
    tv.tv_usec = (int)((timeout % 1000) * 1000);
#if SC_PLATFORM_WINDOWS
    int maxFd = -1;
#else
    int maxFd = nativeSocket;
#endif
    const auto result = ::select(maxFd + 1, &fds, nullptr, nullptr, &tv);
    SC_TRY_MSG(result != SOCKET_ERROR, "select failed");
    return FD_ISSET(nativeSocket, &fds) ? read(data, readData) : Result(false);
}
