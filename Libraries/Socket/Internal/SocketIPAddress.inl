// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../SocketDescriptor.h"

#include "../../Strings/SmallString.h"

namespace SC
{
struct SocketIPAddressInternal;
}
struct SC::SocketIPAddressInternal
{
    [[nodiscard]] static Result parseIPV4(StringView ipAddress, uint16_t port, struct sockaddr_in& inaddr)
    {
        SmallString<64> buffer = StringEncoding::Ascii;
        StringView      ipNullTerm;
        SC_TRY(StringConverter(buffer).convertNullTerminateFastPath(ipAddress, ipNullTerm));
        memset(&inaddr, 0, sizeof(inaddr));
        inaddr.sin_port   = htons(port);
        inaddr.sin_family = SocketFlags::toNative(SocketFlags::AddressFamilyIPV4);
        const auto res    = ::inet_pton(inaddr.sin_family, ipNullTerm.bytesIncludingTerminator(), &inaddr.sin_addr);
        if (res == 0 or res == -1)
        {
            return Result::Error("inet_pton Invalid IPV4 Address");
        }
        return Result(true);
    }

    [[nodiscard]] static Result parseIPV6(StringView ipAddress, uint16_t port, struct sockaddr_in6& inaddr)
    {
        SmallString<64> buffer = StringEncoding::Ascii;
        StringView      ipNullTerm;
        SC_TRY(StringConverter(buffer).convertNullTerminateFastPath(ipAddress, ipNullTerm));
        memset(&inaddr, 0, sizeof(inaddr));
        inaddr.sin6_port   = htons(port);
        inaddr.sin6_family = SocketFlags::toNative(SocketFlags::AddressFamilyIPV6);
        const auto res     = ::inet_pton(inaddr.sin6_family, ipNullTerm.bytesIncludingTerminator(), &inaddr.sin6_addr);
        if (res == 0 or res == -1)
        {
            return Result::Error("inet_pton Invalid IPV6 Address");
        }
        return Result(true);
    }
};

SC::uint32_t SC::SocketIPAddress::sizeOfHandle() const
{
    switch (addressFamily)
    {
    case SocketFlags::AddressFamilyIPV4: return sizeof(sockaddr_in);
    case SocketFlags::AddressFamilyIPV6: return sizeof(sockaddr_in6);
    }
    Assert::unreachable();
}

SC::Result SC::SocketIPAddress::fromAddressPort(StringView interfaceAddress, uint16_t port)
{
    static_assert(sizeof(sockaddr_in6) >= sizeof(sockaddr_in), "size");
    static_assert(alignof(sockaddr_in6) >= alignof(sockaddr_in), "size");

    Result ipParsedOk =
        SocketIPAddressInternal::parseIPV4(interfaceAddress, port, handle.reinterpret_as<sockaddr_in>());
    if (not ipParsedOk)
    {
        ipParsedOk = SocketIPAddressInternal::parseIPV6(interfaceAddress, port, handle.reinterpret_as<sockaddr_in6>());
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
    return Result(true);
}
