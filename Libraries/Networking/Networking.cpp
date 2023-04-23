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

#include <netdb.h>
#include <sys/select.h> // fd_set for emscripten
#include <unistd.h>
constexpr int  SOCKET_ERROR = -1;
SC::ReturnCode SC::Network::init() { return true; }

SC::ReturnCode SC::Network::shutdown() { return true; }
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
#endif
    SC_TRY_IF(socket.assign(openedSocket));

    if (::bind(openedSocket, addressInfos->ai_addr, static_cast<int>(addressInfos->ai_addrlen)) == SOCKET_ERROR)
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
        openedSocket = ::socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (openedSocket == SocketDescriptorNativeInvalid)
            continue;
        if (::connect(openedSocket, it->ai_addr, (int)it->ai_addrlen) == 0)
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
    const auto written = ::send(nativeSocket, data.data(), static_cast<int>(data.sizeInBytes()), 0);
    if (written != data.sizeInBytes())
    {
        return "send error"_a8;
    }
    return true;
}

SC::ReturnCode SC::TCPClient::read(Span<char> data)
{
    SocketDescriptorNative nativeSocket;
    SC_TRY_IF(socket.get(nativeSocket, "Invalid socket"_a8));
    const auto result = ::recv(nativeSocket, data.data(), static_cast<int>(data.sizeInBytes()), 0);
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
    int maxFd = nativeSocket;
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
