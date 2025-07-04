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
    if (res == SOCKET_ERROR)
    {
        return Result::Error("connect failed");
    }
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
    if (written < 0)
    {
        return Result::Error("send error");
    }
    if (static_cast<decltype(data.sizeInBytes())>(written) != data.sizeInBytes())
    {
        return Result::Error("send error");
    }
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
    if (recvSize < 0)
    {
        return Result::Error("recv error");
    }
    readData = {data.data(), static_cast<size_t>(recvSize)};
    return Result(true);
}

SC::Result SC::SocketClient::readWithTimeout(Span<char> data, Span<char>& readData, Time::Milliseconds timeout)
{
    SocketDescriptor::Handle nativeSocket;
    SC_TRY(socket.get(nativeSocket, Result::Error("Invalid socket")));
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(nativeSocket, &fds);

    struct timeval tv;
    tv.tv_sec  = static_cast<int>(timeout.ms / 1000);
    tv.tv_usec = (int)((timeout.ms % 1000) * 1000);
#if SC_PLATFORM_WINDOWS
    int maxFd = -1;
#else
    int maxFd = nativeSocket;
#endif
    const auto result = ::select(maxFd + 1, &fds, nullptr, nullptr, &tv);
    if (result == SOCKET_ERROR)
    {
        return Result::Error("select failed");
    }
    if (FD_ISSET(nativeSocket, &fds))
    {
        return read(data, readData);
    }
    return Result(false);
}
