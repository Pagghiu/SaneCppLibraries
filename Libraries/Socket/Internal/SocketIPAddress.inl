// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../Common/Result.h"
#include "../../Common/Span.h"
#include "../../Socket/Socket.h"
#include "SocketInternal.h"

#if !SC_PLATFORM_WINDOWS
#include <arpa/inet.h> // inet_pton
#include <netdb.h>     // AF_INET / IPPROTO_TCP / AF_UNSPEC
#endif
#if !SC_PLATFORM_WINDOWS && !SC_PLATFORM_EMSCRIPTEN
#include <stddef.h> // offsetof
#include <sys/un.h> // sockaddr_un
#endif

namespace SC
{
struct SocketIPAddressInternal;
}
struct SC::SocketIPAddressInternal
{
    [[nodiscard]] static Result parseIPV4(StringSpan ipAddress, uint16_t port, struct sockaddr_in& inaddr)
    {
        char buffer[64] = {0};
        SC_TRY_MSG(detail::writeNullTerminatedToBuffer(ipAddress.toCharSpan(), buffer), "ipAddress too long");
        memset(&inaddr, 0, sizeof(inaddr));
        inaddr.sin_port   = htons(port);
        inaddr.sin_family = SocketFlags::toNative(SocketFlags::AddressFamilyIPV4);
        const auto res    = ::inet_pton(inaddr.sin_family, buffer, &inaddr.sin_addr);
        if (res == 0 or res == -1)
        {
            return Result::Error("inet_pton Invalid IPV4 Address");
        }
        return Result(true);
    }

    [[nodiscard]] static Result parseIPV6(StringSpan ipAddress, uint16_t port, struct sockaddr_in6& inaddr)
    {
        char buffer[64] = {0};
        SC_TRY_MSG(detail::writeNullTerminatedToBuffer(ipAddress.toCharSpan(), buffer), "ipAddress too long");
        memset(&inaddr, 0, sizeof(inaddr));
        inaddr.sin6_port   = htons(port);
        inaddr.sin6_family = SocketFlags::toNative(SocketFlags::AddressFamilyIPV6);
        const auto res     = ::inet_pton(inaddr.sin6_family, buffer, &inaddr.sin6_addr);
        if (res == 0 or res == -1)
        {
            return Result::Error("inet_pton Invalid IPV6 Address");
        }
        return Result(true);
    }
};

SC::SocketIPAddress::SocketIPAddress(SocketFlags::AddressFamily addressFamily)
{
    switch (addressFamily)
    {
    case SocketFlags::AddressFamilyIPV4: {
        sockaddr_in& sa = handle.reinterpret_as<sockaddr_in>();
        sa.sin_family   = AF_INET;
    }
    break;
    case SocketFlags::AddressFamilyIPV6: {
        sockaddr_in6& sa = handle.reinterpret_as<sockaddr_in6>();
        sa.sin6_family   = AF_INET6;
    }
    break;
    case SocketFlags::AddressFamilyUnix: SC_SOCKET_ASSERT_RELEASE(false); break;
    }
}

SC::SocketFlags::AddressFamily SC::SocketIPAddress::getAddressFamily() const
{
    const sockaddr_in* sa = &handle.reinterpret_as<struct sockaddr_in const>();
    if (sa->sin_family == AF_INET)
    {
        return SocketFlags::AddressFamilyIPV4;
    }
    else
    {
        SC_SOCKET_ASSERT_RELEASE(sa->sin_family == AF_INET6);
        return SocketFlags::AddressFamilyIPV6;
    }
}

SC::uint16_t SC::SocketIPAddress::getPort() const
{
    const sockaddr_in* sa = &handle.reinterpret_as<struct sockaddr_in const>();
    if (sa->sin_family == AF_INET)
    {
        const sockaddr_in* sa_in = (struct sockaddr_in*)sa;
        return ntohs(sa_in->sin_port);
    }
    else
    {
        SC_SOCKET_ASSERT_RELEASE(sa->sin_family == AF_INET6);
        const sockaddr_in6* sa_in6 = (struct sockaddr_in6*)sa;
        return ntohs(sa_in6->sin6_port);
    }
}

SC::uint32_t SC::SocketIPAddress::sizeOfHandle() const
{
    return getAddressFamily() == SocketFlags::AddressFamilyIPV4 ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
}

bool SC::SocketIPAddress::isValid() const
{
    static_assert(MAX_ASCII_STRING_LENGTH <= INET6_ADDRSTRLEN, "MAX_ASCII_STRING_LENGTH");
    char       ipstr[INET6_ADDRSTRLEN];
    StringSpan outSpan;
    return toString(ipstr, outSpan);
}

bool SC::SocketIPAddress::toString(Span<char> inputSpan, StringSpan& outputSpan) const
{
    const sockaddr* sa = &handle.reinterpret_as<struct sockaddr>();
    SC_TRY(inputSpan.sizeInBytes() >= MAX_ASCII_STRING_LENGTH);

    char* ipstr = inputSpan.data();

    if (sa->sa_family == AF_INET)
    {
        const struct sockaddr_in* sa_in = &handle.reinterpret_as<struct sockaddr_in>();
        SC_TRY(::inet_ntop(AF_INET, &(sa_in->sin_addr), ipstr, (socklen_t)inputSpan.sizeInBytes()) != 0);
        outputSpan = StringSpan({ipstr, ::strlen(ipstr)}, true, StringEncoding::Ascii);
        return true;
    }
    else if (sa->sa_family == AF_INET6)
    {
        const struct sockaddr_in6* sa_in6 = &handle.reinterpret_as<struct sockaddr_in6>();
        SC_TRY(::inet_ntop(AF_INET6, &(sa_in6->sin6_addr), ipstr, (socklen_t)inputSpan.sizeInBytes()) != 0);
        outputSpan = StringSpan({ipstr, ::strlen(ipstr)}, true, StringEncoding::Ascii);
        return true;
    }
    return false;
}

SC::Result SC::SocketIPAddress::fromAddressPort(StringSpan interfaceAddress, uint16_t port)
{
    static_assert(sizeof(sockaddr_in6) >= sizeof(sockaddr_in), "size");
    static_assert(alignof(sockaddr_in6) >= alignof(sockaddr_in), "size");
    SC_TRY_MSG(detail::isASCII(interfaceAddress), "Only ASCII encoding is supported");

    Result res = SocketIPAddressInternal::parseIPV4(interfaceAddress, port, handle.reinterpret_as<sockaddr_in>());
    if (not res)
    {
        res = SocketIPAddressInternal::parseIPV6(interfaceAddress, port, handle.reinterpret_as<sockaddr_in6>());
    }
    return res;
}

SC::SocketAddress::SocketAddress(const SocketIPAddress& ipAddress)
{
    nativeSize = ipAddress.sizeOfHandle();
    ::memcpy(&handle.reinterpret_as<char>(), &ipAddress.handle.reinterpret_as<const char>(), nativeSize);
}

SC::Result SC::SocketAddress::fromUnixPath(StringSpan path)
{
#if !SC_PLATFORM_WINDOWS && !SC_PLATFORM_EMSCRIPTEN
    SC_TRY_MSG(path.getEncoding() != StringEncoding::Utf16, "SocketAddress::fromUnixPath only ASCII/UTF8 paths");
    SC_TRY_MSG(not path.isEmpty(), "SocketAddress::fromUnixPath path cannot be empty");

    const Span<const char> pathBytes = path.toCharSpan();
    for (size_t idx = 0; idx < pathBytes.sizeInBytes(); ++idx)
    {
        SC_TRY_MSG(pathBytes[idx] != '\0', "SocketAddress::fromUnixPath path contains null bytes");
    }

    static_assert(sizeof(sockaddr_un) <= sizeof(handle), "SocketAddress storage is too small for sockaddr_un");
    sockaddr_un& address = handle.reinterpret_as<sockaddr_un>();
    SC_TRY_MSG(pathBytes.sizeInBytes() < sizeof(address.sun_path), "SocketAddress::fromUnixPath path too long");

    ::memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
#if SC_PLATFORM_APPLE
    address.sun_len = static_cast<unsigned char>(offsetof(sockaddr_un, sun_path) + pathBytes.sizeInBytes() + 1);
#endif
    ::memcpy(address.sun_path, pathBytes.data(), pathBytes.sizeInBytes());
    nativeSize = static_cast<uint32_t>(offsetof(sockaddr_un, sun_path) + pathBytes.sizeInBytes() + 1);
    return Result(true);
#else
    (void)path;
    return Result::Error("Unix-domain sockets are unsupported on this platform");
#endif
}

SC::Result SC::SocketAddress::fromUnixAbstractName(Span<const char> name)
{
#if SC_PLATFORM_LINUX
    SC_TRY_MSG(not name.empty(), "SocketAddress::fromUnixAbstractName name cannot be empty");
    static_assert(sizeof(sockaddr_un) <= sizeof(handle), "SocketAddress storage is too small for sockaddr_un");
    sockaddr_un& address = handle.reinterpret_as<sockaddr_un>();
    SC_TRY_MSG(name.sizeInBytes() + 1 <= sizeof(address.sun_path), "SocketAddress::fromUnixAbstractName name too long");

    ::memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    ::memcpy(address.sun_path + 1, name.data(), name.sizeInBytes());
    nativeSize = static_cast<uint32_t>(offsetof(sockaddr_un, sun_path) + 1 + name.sizeInBytes());
    return Result(true);
#else
    (void)name;
    return Result::Error("Unix abstract namespace is unsupported on this platform");
#endif
}

SC::SocketFlags::AddressFamily SC::SocketAddress::getAddressFamily() const
{
    const sockaddr& address = handle.reinterpret_as<const sockaddr>();
    return SocketFlags::AddressFamilyFromInt(address.sa_family);
}

SC::uint32_t SC::SocketAddress::sizeOfHandle() const { return nativeSize; }

bool SC::SocketAddress::isValid() const
{
    if (nativeSize == 0)
    {
        return false;
    }
    const sockaddr& address = handle.reinterpret_as<const sockaddr>();
    if (address.sa_family == AF_INET or address.sa_family == AF_INET6)
    {
        SocketIPAddress ipAddress;
        return getIPAddress(ipAddress) and ipAddress.isValid();
    }
#if !SC_PLATFORM_WINDOWS && !SC_PLATFORM_EMSCRIPTEN
    if (address.sa_family == AF_UNIX)
    {
        return nativeSize >= offsetof(sockaddr_un, sun_path);
    }
#endif
    return false;
}

SC::Result SC::SocketAddress::getIPAddress(SocketIPAddress& output) const
{
    SC_TRY_MSG(nativeSize == sizeof(sockaddr_in) or nativeSize == sizeof(sockaddr_in6),
               "SocketAddress does not contain an IP address");
    const sockaddr& address = handle.reinterpret_as<const sockaddr>();
    SC_TRY_MSG(address.sa_family == AF_INET or address.sa_family == AF_INET6,
               "SocketAddress does not contain an IP address");
    ::memcpy(&output.handle.reinterpret_as<char>(), &handle.reinterpret_as<const char>(), nativeSize);
    return Result(true);
}

SC::Result SC::SocketAddress::getUnixName(Span<const char>& output, UnixNamespace& unixNamespace) const
{
#if !SC_PLATFORM_WINDOWS && !SC_PLATFORM_EMSCRIPTEN
    const sockaddr_un& address = handle.reinterpret_as<const sockaddr_un>();
    SC_TRY_MSG(address.sun_family == AF_UNIX and nativeSize >= offsetof(sockaddr_un, sun_path),
               "SocketAddress does not contain a Unix-domain address");
    const size_t nameSize = nativeSize - offsetof(sockaddr_un, sun_path);
    if (nameSize == 0)
    {
        output        = {};
        unixNamespace = UnixNamespace::Unnamed;
    }
    else if (address.sun_path[0] == '\0')
    {
        output        = {address.sun_path + 1, nameSize - 1};
        unixNamespace = UnixNamespace::Abstract;
    }
    else
    {
        const size_t pathSize = address.sun_path[nameSize - 1] == '\0' ? nameSize - 1 : nameSize;
        output                = {address.sun_path, pathSize};
        unixNamespace         = UnixNamespace::Pathname;
    }
    return Result(true);
#else
    (void)output;
    (void)unixNamespace;
    return Result::Error("Unix-domain sockets are unsupported on this platform");
#endif
}
