// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Socket.h"

namespace SC
{
struct SocketIPAddressInternal;
}
struct SC::SocketIPAddressInternal
{
    [[nodiscard]] static Result parseIPV4(SpanStringView ipAddress, uint16_t port, struct sockaddr_in& inaddr)
    {
        char buffer[64] = {0};
        SC_TRY_MSG(ipAddress.writeNullTerminated(buffer), "ipAddress too long");
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

    [[nodiscard]] static Result parseIPV6(SpanStringView ipAddress, uint16_t port, struct sockaddr_in6& inaddr)
    {
        char buffer[64] = {0};
        SC_TRY_MSG(ipAddress.writeNullTerminated(buffer), "ipAddress too long");
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
    default: Assert::unreachable();
    }
}

SC::SocketFlags::AddressFamily SC::SocketIPAddress::getAddressFamily() const
{
    const sockaddr_in* sa = &handle.reinterpret_as<struct sockaddr_in const>();
    if (sa->sin_family == AF_INET)
    {
        return SocketFlags::AddressFamilyIPV4;
    }
    else if (sa->sin_family == AF_INET6)
    {
        return SocketFlags::AddressFamilyIPV6;
    }
    Assert::unreachable();
}

SC::uint16_t SC::SocketIPAddress::getPort() const
{
    const sockaddr_in* sa = &handle.reinterpret_as<struct sockaddr_in const>();
    if (sa->sin_family == AF_INET)
    {
        const sockaddr_in* sa_in = (struct sockaddr_in*)sa;
        return ntohs(sa_in->sin_port);
    }
    else if (sa->sin_family == AF_INET6)
    {
        const sockaddr_in6* sa_in6 = (struct sockaddr_in6*)sa;
        return ntohs(sa_in6->sin6_port);
    }
    Assert::unreachable();
}

SC::uint32_t SC::SocketIPAddress::sizeOfHandle() const
{
    switch (getAddressFamily())
    {
    case SocketFlags::AddressFamilyIPV4: return sizeof(sockaddr_in);
    case SocketFlags::AddressFamilyIPV6: return sizeof(sockaddr_in6);
    }
    Assert::unreachable();
}

bool SC::SocketIPAddress::isValid() const
{
    static_assert(MAX_ASCII_STRING_LENGTH <= INET6_ADDRSTRLEN, "MAX_ASCII_STRING_LENGTH");
    char ipstr[INET6_ADDRSTRLEN];
    return not toString(ipstr).text.empty();
}

SC::SpanStringView SC::SocketIPAddress::formatAddress(Span<char> inOutString) const
{
    const sockaddr* sa = &handle.reinterpret_as<struct sockaddr const>();

    char* ipstr = inOutString.data();

    if (sa->sa_family == AF_INET)
    {
        struct sockaddr_in* sa_in = (struct sockaddr_in*)sa;
        if (::inet_ntop(AF_INET, &(sa_in->sin_addr), ipstr, (socklen_t)inOutString.sizeInBytes()) == nullptr)
        {
            return {};
        }
        return {ipstr, ::strlen(ipstr)};
    }
    else if (sa->sa_family == AF_INET6)
    {
        struct sockaddr_in6* sa_in6 = (struct sockaddr_in6*)sa;
        if (::inet_ntop(AF_INET6, &(sa_in6->sin6_addr), ipstr, (socklen_t)inOutString.sizeInBytes()) == nullptr)
        {
            return {};
        }
        return {ipstr, ::strlen(ipstr)};
    }
    return {};
}

SC::Result SC::SocketIPAddress::fromAddressPort(SpanStringView interfaceAddress, uint16_t port)
{
    static_assert(sizeof(sockaddr_in6) >= sizeof(sockaddr_in), "size");
    static_assert(alignof(sockaddr_in6) >= alignof(sockaddr_in), "size");

    Result res = SocketIPAddressInternal::parseIPV4(interfaceAddress, port, handle.reinterpret_as<sockaddr_in>());
    if (not res)
    {
        res = SocketIPAddressInternal::parseIPV6(interfaceAddress, port, handle.reinterpret_as<sockaddr_in6>());
    }
    return res;
}
