// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../Foundation/Assert.h"
#include "../../Foundation/Result.h"
#include "../../Foundation/Span.h"
#include "../../Socket/Socket.h"
#include "SocketInternal.h"

#if !SC_PLATFORM_WINDOWS
#include <arpa/inet.h> // inet_pton
#include <netdb.h>     // AF_INET / IPPROTO_TCP / AF_UNSPEC
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
        SC_ASSERT_RELEASE(sa->sin_family == AF_INET6);
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
        SC_ASSERT_RELEASE(sa->sin_family == AF_INET6);
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
    SC_TRY_MSG(interfaceAddress.getEncoding() == StringEncoding::Ascii, "Only ASCII encoding is supported");

    Result res = SocketIPAddressInternal::parseIPV4(interfaceAddress, port, handle.reinterpret_as<sockaddr_in>());
    if (not res)
    {
        res = SocketIPAddressInternal::parseIPV6(interfaceAddress, port, handle.reinterpret_as<sockaddr_in6>());
    }
    return res;
}
