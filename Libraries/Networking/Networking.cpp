// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Networking.h"

#include "../Foundation/StringBuilder.h"
#include "../Foundation/StringConverter.h"

#if SC_PLATFORM_WINDOWS

#include <WinSock2.h>
#include <Ws2tcpip.h> // getaddrinfo

using socklen_t = int;
#pragma comment(lib, "Ws2_32.lib")

bool           SC::Network::inited = false;
SC::Mutex      SC::Network::mutex;
SC::ReturnCode SC::Network::init()
{
    mutex.lock();
    if (inited == false)
    {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        {
            mutex.unlock();
            return "WSAStartup failed"_a8;
        }
        inited = true;
    }
    mutex.unlock();
    return true;
}

SC::ReturnCode SC::Network::shutdown()
{
    mutex.lock();
    WSACleanup();
    inited = false;
    mutex.unlock();
    return true;
}
#else

#include "../System/SystemPosix.h"
#include <netdb.h>
#include <sys/select.h> // fd_set for emscripten
#include <unistd.h>
constexpr int  SOCKET_ERROR = -1;
SC::ReturnCode SC::Network::init() { return true; }

SC::ReturnCode SC::Network::shutdown() { return true; }

#if !defined(SOCK_NONBLOCK) || !defined(SOCK_CLOEXEC)
// on macOS these flags are not supported
#include <fcntl.h>
#endif

#endif

SC::ReturnCode SC::SocketDescriptorNativeClose(SC::SocketDescriptorNative& fd)
{
#if SC_PLATFORM_WINDOWS
    ::closesocket(fd);
#else
    ::close(fd);
#endif
    fd = SocketDescriptorNativeInvalid;
    return true;
}

SC::ReturnCode SC::SocketDescriptorNativeHandle::create(IPType ipType, Protocol protocol, BlockingType blocking,
                                                        InheritableType inheritable)
{
    SC_TRY_IF(Network::init());
    SC_TRUST_RESULT(close());
    int type = AF_UNSPEC;
    switch (ipType)
    {
    case IPTypeV4: type = AF_INET; break;
    case IPTypeV6: type = AF_INET6; break;
    }

    int proto = IPPROTO_TCP;

    switch (protocol)
    {
    case ProtocolTcp: proto = IPPROTO_TCP; break;
    }

#if SC_PLATFORM_WINDOWS
    DWORD flags = 0;
    if (inheritable == NonInheritable)
    {
        flags |= WSA_FLAG_NO_HANDLE_INHERIT;
    }
    if (blocking == NonBlocking)
    {
        flags |= WSA_FLAG_OVERLAPPED;
    }
    handle = ::WSASocketW(type, SOCK_STREAM, proto, nullptr, 0, flags);
    if (!isValid())
    {
        return "WSASocketW failed"_a8;
    }
    SC_TRY_IF(setBlocking(blocking == Blocking));
#else
    int flags = SOCK_STREAM;
#if defined(SOCK_NONBLOCK)
    if (blocking == NonBlocking)
    {
        flags |= SOCK_NONBLOCK;
    }
#endif // defined(SOCK_NONBLOCK)
#if defined(SOCK_CLOEXEC)
    if (inheritable == NonInheritable)
    {
        flags |= SOCK_CLOEXEC;
    }
#endif // defined(SOCK_CLOEXEC)
    do
    {
        handle = ::socket(type, flags, proto);
    } while (handle == -1 and errno == EINTR);
#if !defined(SOCK_CLOEXEC)
    SC_TRY_IF(setInheritable(inheritable == Inheritable));
#endif // !defined(SOCK_CLOEXEC)
#if !defined(SOCK_NONBLOCK)
    SC_TRY_IF(setBlocking(blocking == Blocking));
#endif // !defined(SOCK_NONBLOCK)

#if defined(SO_NOSIGPIPE)
    {
        int active = 1;
        setsockopt(handle, SOL_SOCKET, SO_NOSIGPIPE, &active, sizeof(active));
    }
#endif // defined(SO_NOSIGPIPE)
#endif // !SC_PLATFORM_WINDOWS
    return isValid();
}

#if SC_PLATFORM_WINDOWS
SC::ReturnCode SC::SocketDescriptorNativeHandle::setInheritable(bool inheritable)
{
    if (::SetHandleInformation(reinterpret_cast<HANDLE>(handle), HANDLE_FLAG_INHERIT, inheritable ? TRUE : FALSE) ==
        FALSE)
    {
        "SetHandleInformation failed"_a8;
    }
    return true;
}

SC::ReturnCode SC::SocketDescriptorNativeHandle::setBlocking(bool blocking)
{
    ULONG enable = blocking ? 0 : 1;
    if (::ioctlsocket(handle, FIONBIO, &enable) == SOCKET_ERROR)
    {
        return "ioctlsocket failed"_a8;
    }
    return true;
}

SC::ReturnCode SC::SocketDescriptorNativeHandle::isInheritable(bool& hasValue) const
{
    DWORD flags;
    if (::GetHandleInformation(reinterpret_cast<HANDLE>(handle), &flags) == FALSE)
    {
        return "GetHandleInformation failed"_a8;
    }
    hasValue = (flags & HANDLE_FLAG_INHERIT) != 0;
    return true;
}
#else

SC::ReturnCode SC::SocketDescriptorNativeHandle::setInheritable(bool inheritable)
{
    return FileDescriptorPosixHelpers::setFileDescriptorFlags<FD_CLOEXEC>(handle, not inheritable);
}

SC::ReturnCode SC::SocketDescriptorNativeHandle::setBlocking(bool blocking)
{
    return FileDescriptorPosixHelpers::setFileStatusFlags<O_NONBLOCK>(handle, not blocking);
}

SC::ReturnCode SC::SocketDescriptorNativeHandle::isInheritable(bool& hasValue) const
{
    bool closeOnExec = false;
    auto res         = FileDescriptorPosixHelpers::hasFileDescriptorFlags<FD_CLOEXEC>(handle, closeOnExec);
    hasValue         = not closeOnExec;
    return res;
}
#endif

SC::ReturnCode SC::TCPServer::close() { return socket.close(); }

// TODO: Add EINTR checks for all TCPServer/TCPClient os calls.

SC::ReturnCode SC::TCPServer::listen(StringView interfaceAddress, uint32_t port)
{
    SC_TRY_IF(Network::init());
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    SmallString<12> service = StringEncoding::Ascii; // 10 digits + sign + nullterm
    SC_TRY_IF(StringBuilder(service).format("{}", port));

    SmallString<64> addressBuffer = StringEncoding::Ascii;
    StringView      addressZeroTerminated;
    SC_TRY_IF(StringConverter(addressBuffer).convertNullTerminateFastPath(interfaceAddress, addressZeroTerminated));

    struct addrinfo* addressInfos;

    if (::getaddrinfo(addressZeroTerminated.bytesIncludingTerminator(), service.data.data(), &hints, &addressInfos) !=
        0)
    {
        return "Cannot resolve hostname"_a8;
    }
    auto destroyAddresses = MakeDeferred([&]() { freeaddrinfo(addressInfos); });

    SocketDescriptorNative openedSocket;
    openedSocket = ::socket(addressInfos->ai_family, addressInfos->ai_socktype, addressInfos->ai_protocol);
    SC_TRY_MSG(openedSocket != SocketDescriptorNativeInvalid, "Cannot create listening socket"_a8);
#if SC_PLATFORM_WINDOWS
    char value = 1;
    setsockopt(openedSocket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
    const int addrLen = static_cast<int>(addressInfos->ai_addrlen);
#else
    const auto addrLen = addressInfos->ai_addrlen;
#endif
    SC_TRY_IF(socket.assign(openedSocket));

    if (::bind(openedSocket, addressInfos->ai_addr, addrLen) == SOCKET_ERROR)
    {
        SC_TRUST_RESULT(socket.close());
        return "Could not bind socket to port"_a8;
    }
    constexpr int numberOfWaitingConnections = 2; // TODO: Expose numberOfWaitingConnections?
    if (::listen(openedSocket, numberOfWaitingConnections) == SOCKET_ERROR)
    {
        SC_TRUST_RESULT(socket.close());
        return "Could not listen"_a8;
    }
    return true;
}

SC::ReturnCode SC::TCPServer::accept(TCPClient& newClient)
{
    SC_TRY_MSG(not newClient.socket.isValid(), "destination socket already in use"_a8);
    SocketDescriptorNative listenDescriptor;
    SC_TRY_IF(socket.get(listenDescriptor, "Invalid socket"_a8));

    struct sockaddr_in sAddr;
    socklen_t          sAddrSize = sizeof(sAddr);

    SocketDescriptorNative acceptedClient;
    acceptedClient = ::accept(listenDescriptor, reinterpret_cast<struct sockaddr*>(&sAddr), &sAddrSize);
    SC_TRY_MSG(acceptedClient != SocketDescriptorNativeInvalid, "accept failed"_a8);
    return newClient.socket.assign(acceptedClient);
}

SC::ReturnCode SC::TCPClient::connect(StringView address, uint32_t port)
{
    SC_TRY_IF(Network::init());
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    SmallString<12> service = StringEncoding::Ascii; // 10 digits + sign + nullterm
    SC_TRY_IF(StringBuilder(service).format("{}", port));

    SmallString<64> addressBuffer = StringEncoding::Ascii;
    StringView      addressZeroTerminated;
    SC_TRY_IF(StringConverter(addressBuffer).convertNullTerminateFastPath(address, addressZeroTerminated));

    struct addrinfo* addressInfos;
    if (::getaddrinfo(addressZeroTerminated.bytesIncludingTerminator(), service.data.data(), &hints, &addressInfos) !=
        0)
    {
        return "Cannot resolve hostname"_a8;
    }
    auto destroyAddresses = MakeDeferred([&]() { freeaddrinfo(addressInfos); });

    SocketDescriptorNative openedSocket = SocketDescriptorNativeInvalid;
    for (struct addrinfo* it = addressInfos; it != nullptr; it = it->ai_next)
    {
        // SOCK_NONBLOCK
        openedSocket = ::socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (openedSocket == SocketDescriptorNativeInvalid)
            continue;
#if SC_PLATFORM_WINDOWS
        const int addrLen = static_cast<int>(it->ai_addrlen);
#else
        const auto addrLen = it->ai_addrlen;
#endif
        if (::connect(openedSocket, it->ai_addr, addrLen) == 0)
            break;
        SC_TRY_IF(SocketDescriptorNativeClose(openedSocket));
    }
    SC_TRY_MSG(openedSocket != SocketDescriptorNativeInvalid, "Cannot connect to host"_a8);
    return socket.assign(openedSocket);
}

SC::ReturnCode SC::TCPClient::close() { return socket.close(); }

SC::ReturnCode SC::TCPClient::write(Span<const char> data)
{
    SocketDescriptorNative nativeSocket;
    SC_TRY_IF(socket.get(nativeSocket, "Invalid socket"_a8));
#if SC_PLATFORM_WINDOWS
    const int sizeInBytes = static_cast<int>(data.sizeInBytes());
#else
    const auto sizeInBytes = data.sizeInBytes();
#endif
    const auto written = ::send(nativeSocket, data.data(), sizeInBytes, 0);
    if (written < 0)
    {
        return "send error"_a8;
    }
    if (static_cast<decltype(data.sizeInBytes())>(written) != data.sizeInBytes())
    {
        return "send error"_a8;
    }
    return true;
}

SC::ReturnCode SC::TCPClient::read(Span<char> data)
{
    SocketDescriptorNative nativeSocket;
    SC_TRY_IF(socket.get(nativeSocket, "Invalid socket"_a8));
#if SC_PLATFORM_WINDOWS
    const int sizeInBytes = static_cast<int>(data.sizeInBytes());
#else
    const auto sizeInBytes = data.sizeInBytes();
#endif
    const auto result = ::recv(nativeSocket, data.data(), sizeInBytes, 0);
    if (result < 0)
    {
        return "recv error"_a8;
    }
    return true;
}

SC::ReturnCode SC::TCPClient::readWithTimeout(Span<char> data, IntegerMilliseconds timeout)
{
    SocketDescriptorNative nativeSocket;
    SC_TRY_IF(socket.get(nativeSocket, "Invalid socket"_a8));
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(nativeSocket, &fds);

    struct timeval tv;
    tv.tv_sec  = static_cast<int>(timeout.ms / 1000);
    tv.tv_usec = (int)((timeout.ms % 1000) * 1000);
#if SC_PLATFORM_WINDOWS
    int maxFd = -1;
#else
    int        maxFd       = nativeSocket;
#endif
    const auto result = select(maxFd + 1, &fds, NULL, NULL, &tv);
    if (result == SOCKET_ERROR)
    {
        return "select failed"_a8;
    }
    if (FD_ISSET(nativeSocket, &fds))
    {
        return read(data);
    }
    return false;
}
