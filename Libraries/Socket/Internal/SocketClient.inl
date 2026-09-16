// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Socket/Socket.h"
#include "SocketInternal.h"

#include <errno.h>  // errno
#include <memory.h> // memcpy
#if !SC_PLATFORM_WINDOWS
#include <netinet/in.h> // sockaddr_in
#include <sys/select.h> // select
#endif

SC::ResultSocket SC::SocketClient::connect(StringSpan address, uint16_t port)
{
    SocketIPAddress nativeAddress;
    SC_TRY(nativeAddress.fromAddressPort(address, port));
    return connect(nativeAddress);
}

SC::ResultSocket SC::SocketClient::connect(SocketIPAddress ipAddress) { return connect(SocketAddress(ipAddress)); }

SC::ResultSocket SC::SocketClient::connect(const SocketAddress& address)
{
    if (not SocketNetworking::isNetworkingInited())
        return {SocketError::NetworkingNotInitialized, SocketErrorDetail::ConnectSocket};
    if (not address.isValid())
        return {SocketError::InvalidAddress, SocketErrorDetail::ConnectSocket};
    SocketDescriptor::Handle openedSocket;
    SC_TRY(socket.get(openedSocket, ResultSocket(SocketError::InvalidSocket, SocketErrorDetail::ConnectSocket)));
    socklen_t nativeSize = address.sizeOfHandle();
    int       res;
    do
    {
        res = ::connect(openedSocket, &address.handle.reinterpret_as<const struct sockaddr>(), nativeSize);
    } while (res == SOCKET_ERROR and errno == EINTR);
    if (res == SOCKET_ERROR)
    {
#if SC_PLATFORM_WINDOWS
        const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
#else
        const uint32_t nativeError = static_cast<uint32_t>(errno);
#endif
        return ResultSocket::withNativeError(SocketError::ConnectFailed, SocketErrorDetail::ConnectSocket, nativeError);
    }
    return {};
}

SC::ResultSocket SC::SocketClient::write(Span<const char> data)
{
    SocketDescriptor::Handle nativeSocket;
    SC_TRY(socket.get(nativeSocket, ResultSocket(SocketError::InvalidSocket, SocketErrorDetail::SendStream)));
#if SC_PLATFORM_WINDOWS
    const int sizeInBytes = static_cast<int>(data.sizeInBytes());
#else
    const auto sizeInBytes = data.sizeInBytes();
#endif
    const auto written = ::send(nativeSocket, data.data(), sizeInBytes, 0);
    if (written < 0)
    {
#if SC_PLATFORM_WINDOWS
        const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
        if (nativeError == WSAEWOULDBLOCK)
            return {SocketError::WouldBlock, SocketErrorDetail::SendStream};
#else
        const uint32_t nativeError = static_cast<uint32_t>(errno);
        if (nativeError == EAGAIN or nativeError == EWOULDBLOCK)
            return {SocketError::WouldBlock, SocketErrorDetail::SendStream};
#endif
        return ResultSocket::withNativeError(SocketError::SendFailed, SocketErrorDetail::SendStream, nativeError);
    }
    if (static_cast<decltype(data.sizeInBytes())>(written) != data.sizeInBytes())
        return ResultSocket::withActualBytes(SocketError::IncompleteSend, SocketErrorDetail::SendStream,
                                             static_cast<uint32_t>(written));
    return {};
}

SC::ResultSocket SC::SocketClient::read(Span<char> data, Span<char>& readData)
{
    SocketDescriptor::Handle nativeSocket;
    SC_TRY(socket.get(nativeSocket, ResultSocket(SocketError::InvalidSocket, SocketErrorDetail::ReceiveStream)));
#if SC_PLATFORM_WINDOWS
    const int sizeInBytes = static_cast<int>(data.sizeInBytes());
#else
    const auto sizeInBytes = data.sizeInBytes();
#endif
    const auto recvSize = ::recv(nativeSocket, data.data(), sizeInBytes, 0);
    if (recvSize < 0)
    {
#if SC_PLATFORM_WINDOWS
        const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
        if (nativeError == WSAEWOULDBLOCK)
            return {SocketError::WouldBlock, SocketErrorDetail::ReceiveStream};
#else
        const uint32_t nativeError = static_cast<uint32_t>(errno);
        if (nativeError == EAGAIN or nativeError == EWOULDBLOCK)
            return {SocketError::WouldBlock, SocketErrorDetail::ReceiveStream};
#endif
        return ResultSocket::withNativeError(SocketError::ReceiveFailed, SocketErrorDetail::ReceiveStream, nativeError);
    }
    readData = {data.data(), static_cast<size_t>(recvSize)};
    return {};
}

SC::ResultSocket SC::SocketClient::readWithTimeout(Span<char> data, Span<char>& readData, int64_t timeout)
{
    SocketDescriptor::Handle nativeSocket;
    SC_TRY(socket.get(nativeSocket, ResultSocket(SocketError::InvalidSocket, SocketErrorDetail::WaitForRead)));
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
    if (result == SOCKET_ERROR)
    {
#if SC_PLATFORM_WINDOWS
        const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
#else
        const uint32_t nativeError = static_cast<uint32_t>(errno);
#endif
        return ResultSocket::withNativeError(SocketError::ReceiveFailed, SocketErrorDetail::WaitForRead, nativeError);
    }
    if (result == 0)
        return {SocketError::TimedOut, SocketErrorDetail::WaitForRead};
    return FD_ISSET(nativeSocket, &fds) ? read(data, readData)
                                        : ResultSocket(SocketError::TimedOut, SocketErrorDetail::WaitForRead);
}
