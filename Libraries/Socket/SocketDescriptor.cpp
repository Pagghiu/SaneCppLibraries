// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../Foundation/Strings/StringConverter.h"
#include "../System/System.h"
#include "../System/Time.h"
#include <string.h> // strlen

#if SC_PLATFORM_WINDOWS

#include <WinSock2.h>
#include <Ws2tcpip.h> // sockadd_in6

using socklen_t = int;
#include "SocketDescriptorInternalWindows.inl"

#else

#include "SocketDescriptorInternalPosix.inl"
#include <arpa/inet.h>  // inet_pton
#include <sys/select.h> // fd_set for emscripten

#endif

namespace SC
{
struct NetworkingInternal
{
    [[nodiscard]] static ReturnCode parseIPV4(StringView ipAddress, uint16_t port, struct sockaddr_in& inaddr)
    {
        SmallString<64> buffer = StringEncoding::Ascii;
        StringView      ipNullTerm;
        SC_TRY(StringConverter(buffer).convertNullTerminateFastPath(ipAddress, ipNullTerm));
        memset(&inaddr, 0, sizeof(inaddr));
        inaddr.sin_port   = htons(port);
        inaddr.sin_family = SocketFlags::toNative(SocketFlags::AddressFamilyIPV4);
        const auto res    = ::inet_pton(inaddr.sin_family, ipNullTerm.bytesIncludingTerminator(), &inaddr.sin_addr);
        if (res == 0)
        {
            return ReturnCode::Error("inet_pton Invalid IPV4 Address");
        }
        else if (res == -1)
        {
            return ReturnCode::Error("inet_pton IPV4 failed");
        }
        return ReturnCode(true);
    }

    [[nodiscard]] static ReturnCode parseIPV6(StringView ipAddress, uint16_t port, struct sockaddr_in6& inaddr)
    {
        SmallString<64> buffer = StringEncoding::Ascii;
        StringView      ipNullTerm;
        SC_TRY(StringConverter(buffer).convertNullTerminateFastPath(ipAddress, ipNullTerm));
        memset(&inaddr, 0, sizeof(inaddr));
        inaddr.sin6_port   = htons(port);
        inaddr.sin6_family = SocketFlags::toNative(SocketFlags::AddressFamilyIPV6);
        const auto res     = ::inet_pton(inaddr.sin6_family, ipNullTerm.bytesIncludingTerminator(), &inaddr.sin6_addr);
        if (res == 0)
        {
            return ReturnCode::Error("inet_pton Invalid IPV6 Address");
        }
        else if (res == -1)
        {
            return ReturnCode::Error("inet_pton IPV6 failed");
        }
        return ReturnCode(true);
    }
};
} // namespace SC

SC::ReturnCode SC::SocketDescriptor::getAddressFamily(SocketFlags::AddressFamily& addressFamily) const
{

    struct sockaddr_in6 socketInfo;
    socklen_t           socketInfoLen = sizeof(socketInfo);

    if (::getsockname(handle, reinterpret_cast<struct sockaddr*>(&socketInfo), &socketInfoLen) == SOCKET_ERROR)
    {
        return ReturnCode::Error("getsockname failed");
    }
    addressFamily = SocketFlags::AddressFamilyFromInt(socketInfo.sin6_family);
    return ReturnCode(true);
}

SC::uint32_t SC::SocketIPAddress::sizeOfHandle() const
{
    switch (addressFamily)
    {
    case SocketFlags::AddressFamilyIPV4: return sizeof(sockaddr_in);
    case SocketFlags::AddressFamilyIPV6: return sizeof(sockaddr_in6);
    }
    Assert::unreachable();
}

SC::ReturnCode SC::SocketIPAddress::fromAddressPort(StringView interfaceAddress, uint16_t port)
{
    static_assert(sizeof(sockaddr_in6) >= sizeof(sockaddr_in), "size");
    static_assert(alignof(sockaddr_in6) >= alignof(sockaddr_in), "size");

    ReturnCode ipParsedOk = NetworkingInternal::parseIPV4(interfaceAddress, port, handle.reinterpret_as<sockaddr_in>());
    if (not ipParsedOk)
    {
        ipParsedOk = NetworkingInternal::parseIPV6(interfaceAddress, port, handle.reinterpret_as<sockaddr_in6>());
        if (not ipParsedOk)
        {
            return ipParsedOk;
        }
        addressFamily = SocketFlags::AddressFamilyIPV6;
    }
    else
    {
        addressFamily = SocketFlags::AddressFamilyIPV4;
    }
    return ReturnCode(true);
}

SC::ReturnCode SC::SocketServer::close() { return socket.close(); }

// TODO: Add EINTR checks for all SocketServer/SocketClient os calls.

SC::ReturnCode SC::SocketServer::listen(SocketIPAddress nativeAddress, uint32_t numberOfWaitingConnections)
{
    SC_TRY(SystemFunctions::isNetworkingInited());
    SC_TRY_MSG(socket.isValid(), "Invalid socket");
    SocketDescriptor::Handle listenSocket;
    SC_TRUST_RESULT(socket.get(listenSocket, ReturnCode::Error("invalid listen socket")));

    // TODO: Expose SO_REUSEADDR as an option?
    int value = 1;
#if SC_PLATFORM_WINDOWS
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&value), sizeof(value));
#elif !SC_PLATFORM_EMSCRIPTEN
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
#else
    SC_COMPILER_UNUSED(value);
#endif
    if (::bind(listenSocket, &nativeAddress.handle.reinterpret_as<const struct sockaddr>(),
               nativeAddress.sizeOfHandle()) == SOCKET_ERROR)
    {
        SC_TRUST_RESULT(socket.close());
        return ReturnCode::Error("Could not bind socket to port");
    }
    if (::listen(listenSocket, static_cast<int>(numberOfWaitingConnections)) == SOCKET_ERROR)
    {
        SC_TRUST_RESULT(socket.close());
        return ReturnCode::Error("Could not listen");
    }
    return ReturnCode(true);
}

SC::ReturnCode SC::SocketServer::accept(SocketFlags::AddressFamily addressFamily, SocketDescriptor& newClient)
{
    SC_TRY_MSG(not newClient.isValid(), "destination socket already in use");
    SocketDescriptor::Handle listenDescriptor;
    SC_TRY(socket.get(listenDescriptor, ReturnCode::Error("Invalid socket")));
    SocketIPAddress          nativeAddress(addressFamily);
    socklen_t                nativeSize = nativeAddress.sizeOfHandle();
    SocketDescriptor::Handle acceptedClient =
        ::accept(listenDescriptor, &nativeAddress.handle.reinterpret_as<struct sockaddr>(), &nativeSize);
    SC_TRY_MSG(acceptedClient != SocketDescriptor::Invalid, "accept failed");
    return newClient.assign(acceptedClient);
}

SC::ReturnCode SC::SocketClient::connect(StringView address, uint16_t port)
{
    SocketIPAddress nativeAddress;
    SC_TRY(nativeAddress.fromAddressPort(address, port));
    if (not socket.isValid())
    {
        SC_TRY(socket.create(nativeAddress.getAddressFamily(), SocketFlags::SocketStream, SocketFlags::ProtocolTcp));
    }
    return connect(nativeAddress);
}

SC::ReturnCode SC::SocketClient::connect(SocketIPAddress ipAddress)
{
    SC_TRY(SystemFunctions::isNetworkingInited());
    SocketDescriptor::Handle openedSocket;
    SC_TRUST_RESULT(socket.get(openedSocket, ReturnCode::Error("invalid connect socket")));
    socklen_t nativeSize = ipAddress.sizeOfHandle();
    int       res;
    do
    {
        res = ::connect(openedSocket, &ipAddress.handle.reinterpret_as<const struct sockaddr>(), nativeSize);
    } while (res == SOCKET_ERROR and errno == EINTR);
    if (res == SOCKET_ERROR)
    {
        return ReturnCode::Error("connect failed");
    }
    return ReturnCode(true);
}

SC::ReturnCode SC::SocketClient::close() { return socket.close(); }

SC::ReturnCode SC::SocketClient::write(Span<const char> data)
{
    SocketDescriptor::Handle nativeSocket;
    SC_TRY(socket.get(nativeSocket, ReturnCode::Error("Invalid socket")));
#if SC_PLATFORM_WINDOWS
    const int sizeInBytes = static_cast<int>(data.sizeInBytes());
#else
    const auto sizeInBytes = data.sizeInBytes();
#endif
    const auto written = ::send(nativeSocket, data.data(), sizeInBytes, 0);
    if (written < 0)
    {
        return ReturnCode::Error("send error");
    }
    if (static_cast<decltype(data.sizeInBytes())>(written) != data.sizeInBytes())
    {
        return ReturnCode::Error("send error");
    }
    return ReturnCode(true);
}

SC::ReturnCode SC::SocketClient::read(Span<char> data, Span<char>& readData)
{
    SocketDescriptor::Handle nativeSocket;
    SC_TRY(socket.get(nativeSocket, ReturnCode::Error("Invalid socket")));
#if SC_PLATFORM_WINDOWS
    const int sizeInBytes = static_cast<int>(data.sizeInBytes());
#else
    const auto sizeInBytes = data.sizeInBytes();
#endif
    const auto recvSize = ::recv(nativeSocket, data.data(), sizeInBytes, 0);
    if (recvSize < 0)
    {
        return ReturnCode::Error("recv error");
    }
    readData = {data.data(), static_cast<size_t>(recvSize)};
    return ReturnCode(true);
}

SC::ReturnCode SC::SocketClient::readWithTimeout(Span<char> data, Span<char>& readData, IntegerMilliseconds timeout)
{
    SocketDescriptor::Handle nativeSocket;
    SC_TRY(socket.get(nativeSocket, ReturnCode::Error("Invalid socket")));
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
    const auto result = select(maxFd + 1, &fds, nullptr, nullptr, &tv);
    if (result == SOCKET_ERROR)
    {
        return ReturnCode::Error("select failed");
    }
    if (FD_ISSET(nativeSocket, &fds))
    {
        return read(data, readData);
    }
    return ReturnCode(false);
}

SC::SocketFlags::AddressFamily SC::SocketFlags::AddressFamilyFromInt(int value)
{
    switch (value)
    {
    case AF_INET: return SocketFlags::AddressFamilyIPV4;
    case AF_INET6: return SocketFlags::AddressFamilyIPV6;
    }
    Assert::unreachable();
}

unsigned char SC::SocketFlags::toNative(SocketFlags::AddressFamily type)
{
    switch (type)
    {
    case SocketFlags::AddressFamilyIPV4: return AF_INET;
    case SocketFlags::AddressFamilyIPV6: return AF_INET6;
    }
    Assert::unreachable();
}

SC::SocketFlags::SocketType SC::SocketFlags::SocketTypeFromInt(int value)
{
    switch (value)
    {
    case SOCK_STREAM: return SocketStream;
    case SOCK_DGRAM: return SocketDgram;
    }
    Assert::unreachable();
}
int SC::SocketFlags::toNative(SocketType type)
{
    switch (type)
    {
    case SocketStream: return SOCK_STREAM;
    case SocketDgram: return SOCK_DGRAM;
    }
    Assert::unreachable();
}

SC::SocketFlags::ProtocolType SC::SocketFlags::ProtocolTypeFromInt(int value)
{
    switch (value)
    {
    case IPPROTO_TCP: return ProtocolTcp;
    case IPPROTO_UDP: return ProtocolUdp;
    }
    Assert::unreachable();
}

int SC::SocketFlags::toNative(ProtocolType family)
{
    switch (family)
    {
    case ProtocolTcp: return IPPROTO_TCP;
    case ProtocolUdp: return IPPROTO_UDP;
    }
    Assert::unreachable();
}

SC::ReturnCode SC::DNSResolver::resolve(StringView host, String& ipAddress)
{
    struct addrinfo hints, *res, *p;
    int             status;

    // Setup hints structure
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;   // Use either IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // Use SOCK_STREAM for TCP

    StringConverter converter(ipAddress);

    StringView nullTerminated;
    SC_TRY(converter.convertNullTerminateFastPath(host, nullTerminated));
    // Get address information
    if ((status = getaddrinfo(nullTerminated.bytesIncludingTerminator(), NULL, &hints, &res)) != 0)
    {
        return ReturnCode::Error("DNSResolver::resolve: getaddrinfo error");
    }

    // Loop through results and print IP addresses
    for (p = res; p != NULL; p = p->ai_next)
    {
        void* addr;
        char  ipstr[INET6_ADDRSTRLEN];

        if (p->ai_family == AF_INET)
        {
            struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
            addr                     = &(ipv4->sin_addr);
        }
        else
        {
            struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)p->ai_addr;
            addr                      = &(ipv6->sin6_addr);
        }

        // Convert IP address to a readable string
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        StringView ipView(ipstr, ::strlen(ipstr), true, StringEncoding::Ascii);
        SC_TRY(ipAddress.assign(ipView));
    }

    freeaddrinfo(res); // Free the linked list
    return ReturnCode(true);
}
