// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Networking.h"

#include "../Foundation/StringBuilder.h"
#include "../Foundation/StringConverter.h"
#include "../System/System.h"

#if SC_PLATFORM_WINDOWS

#include <WinSock2.h>
#include <Ws2tcpip.h> // sockadd_in6

using socklen_t = int;

#else

#include <arpa/inet.h>  // inet_pton
#include <sys/select.h> // fd_set for emscripten
constexpr int SOCKET_ERROR = -1;

#endif

namespace SC
{
struct NetworkingInternal
{
    [[nodiscard]] static ReturnCode parseIPV4(StringView ipAddress, uint16_t port, struct sockaddr_in& inaddr)
    {
        SmallString<64> buffer = StringEncoding::Ascii;
        StringView      ipNullTerm;
        SC_TRY_IF(StringConverter(buffer).convertNullTerminateFastPath(ipAddress, ipNullTerm));
        memset(&inaddr, 0, sizeof(inaddr));
        inaddr.sin_port   = htons(port);
        inaddr.sin_family = Descriptor::toNative(Descriptor::AddressFamilyIPV4);
        const auto res    = ::inet_pton(inaddr.sin_family, ipNullTerm.bytesIncludingTerminator(), &inaddr.sin_addr);
        if (res == 0)
        {
            return "inet_pton Invalid IPV4 Address"_a8;
        }
        else if (res == -1)
        {
            return "inet_pton IPV4 failed"_a8;
        }
        return true;
    }

    [[nodiscard]] static ReturnCode parseIPV6(StringView ipAddress, uint16_t port, struct sockaddr_in6& inaddr)
    {
        SmallString<64> buffer = StringEncoding::Ascii;
        StringView      ipNullTerm;
        SC_TRY_IF(StringConverter(buffer).convertNullTerminateFastPath(ipAddress, ipNullTerm));
        memset(&inaddr, 0, sizeof(inaddr));
        inaddr.sin6_port   = htons(port);
        inaddr.sin6_family = Descriptor::toNative(Descriptor::AddressFamilyIPV6);
        const auto res     = ::inet_pton(inaddr.sin6_family, ipNullTerm.bytesIncludingTerminator(), &inaddr.sin6_addr);
        if (res == 0)
        {
            return "inet_pton Invalid IPV6 Address"_a8;
        }
        else if (res == -1)
        {
            return "inet_pton IPV6 failed"_a8;
        }
        return true;
    }
};
} // namespace SC

SC::uint32_t SC::NativeIPAddress::sizeOfHandle() const
{
    switch (addressFamily)
    {
    case Descriptor::AddressFamilyIPV4: return sizeof(sockaddr_in);
    case Descriptor::AddressFamilyIPV6: return sizeof(sockaddr_in6);
    }
    SC_UNREACHABLE();
}

SC::ReturnCode SC::NativeIPAddress::fromAddressPort(StringView interfaceAddress, uint16_t port)
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
        addressFamily = Descriptor::AddressFamilyIPV6;
    }
    else
    {
        addressFamily = Descriptor::AddressFamilyIPV4;
    }
    return true;
}

SC::ReturnCode SC::TCPServer::close() { return socket.close(); }

// TODO: Add EINTR checks for all TCPServer/TCPClient os calls.

SC::ReturnCode SC::TCPServer::listen(StringView interfaceAddress, uint16_t port, uint32_t numberOfWaitingConnections)
{
    SC_TRY_IF(SystemFunctions::isNetworkingInited());

    NativeIPAddress nativeAddress;
    SC_TRY_IF(nativeAddress.fromAddressPort(interfaceAddress, port));
    if (not socket.isValid())
    {
        SC_TRY_IF(socket.create(nativeAddress.getAddressFamily(), Descriptor::SocketStream, Descriptor::ProtocolTcp));
    }

    SocketDescriptor::Handle listenSocket;
    SC_TRUST_RESULT(socket.get(listenSocket, "invalid listen socket"_a8));

    // TODO: Expose SO_REUSEADDR as an option?
#if !SC_PLATFORM_EMSCRIPTEN
    char value = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
#endif
    if (::bind(listenSocket, &nativeAddress.handle.reinterpret_as<const struct sockaddr>(),
               nativeAddress.sizeOfHandle()) == SOCKET_ERROR)
    {
        SC_TRUST_RESULT(socket.close());
        return "Could not bind socket to port"_a8;
    }
    if (::listen(listenSocket, static_cast<int>(numberOfWaitingConnections)) == SOCKET_ERROR)
    {
        SC_TRUST_RESULT(socket.close());
        return "Could not listen"_a8;
    }
    return true;
}

SC::ReturnCode SC::TCPServer::accept(Descriptor::AddressFamily addressFamily, TCPClient& newClient)
{
    SC_TRY_MSG(not newClient.socket.isValid(), "destination socket already in use"_a8);
    SocketDescriptor::Handle listenDescriptor;
    SC_TRY_IF(socket.get(listenDescriptor, "Invalid socket"_a8));
    NativeIPAddress          nativeAddress(addressFamily);
    socklen_t                nativeSize = nativeAddress.sizeOfHandle();
    SocketDescriptor::Handle acceptedClient =
        ::accept(listenDescriptor, &nativeAddress.handle.reinterpret_as<struct sockaddr>(), &nativeSize);
    SC_TRY_MSG(acceptedClient != SocketDescriptor::Invalid, "accept failed"_a8);
    return newClient.socket.assign(acceptedClient);
}

SC::ReturnCode SC::TCPClient::connect(StringView address, uint16_t port)
{
    SC_TRY_IF(SystemFunctions::isNetworkingInited());

    NativeIPAddress nativeAddress;
    SC_TRY_IF(nativeAddress.fromAddressPort(address, port));
    if (not socket.isValid())
    {
        SC_TRY_IF(socket.create(nativeAddress.getAddressFamily(), Descriptor::SocketStream, Descriptor::ProtocolTcp));
    }
    SocketDescriptor::Handle openedSocket;
    SC_TRUST_RESULT(socket.get(openedSocket, "invalid connect socket"_a8));
    socklen_t nativeSize = nativeAddress.sizeOfHandle();
    if (::connect(openedSocket, &nativeAddress.handle.reinterpret_as<const struct sockaddr>(), nativeSize) ==
        SOCKET_ERROR)
    {
        "connect failed"_a8;
    }
    return true;
}

SC::ReturnCode SC::TCPClient::close() { return socket.close(); }

SC::ReturnCode SC::TCPClient::write(Span<const char> data)
{
    SocketDescriptor::Handle nativeSocket;
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
    SocketDescriptor::Handle nativeSocket;
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
    SocketDescriptor::Handle nativeSocket;
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
    const auto result = select(maxFd + 1, &fds, nullptr, nullptr, &tv);
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
