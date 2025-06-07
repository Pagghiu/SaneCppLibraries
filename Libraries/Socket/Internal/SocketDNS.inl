// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Socket/Socket.h"
#include "SocketInternal.h"

#if !SC_PLATFORM_WINDOWS
#include <arpa/inet.h> // inet_ntop
#include <netdb.h>     // AF_INET / IPPROTO_TCP / AF_UNSPEC
#endif
#include <string.h>

SC::Result SC::SocketDNS::resolveDNS(StringViewData host, Span<char>& ipAddress)
{
    struct addrinfo hints, *res, *p;

    // Setup hints structure
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;   // Use either IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // Use SOCK_STREAM for TCP

    char nullTerminated[256] = {0};
    SC_TRY_MSG(host.getEncoding() == StringEncoding::Ascii, "Only ASCII encoding is supported");
    SC_TRY_MSG(detail::writeNullTerminatedToBuffer(host.toCharSpan(), nullTerminated), "host is too big");
    // Get address information
    const int status = ::getaddrinfo(nullTerminated, NULL, &hints, &res);
    if (status != 0)
    {
        return Result::Error("SocketDNS::resolveDNS: getaddrinfo error");
    }

    // Loop through results and print IP addresses
    for (p = res; p != NULL; p = p->ai_next)
    {
        void* addr;
        if (p->ai_family == AF_INET)
        {
            addr = &reinterpret_cast<struct sockaddr_in*>(p->ai_addr)->sin_addr;
        }
        else
        {
            addr = &reinterpret_cast<struct sockaddr_in6*>(p->ai_addr)->sin6_addr;
        }

        if (p->ai_next == NULL) // take the last
        {
            // Convert IP address to a readable string
            char ipstr[INET6_ADDRSTRLEN + 1] = {0};
            ::inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
            Span<const char> ipOut = {ipstr, ::strnlen(ipstr, sizeof(ipstr) - 1)};
            SC_TRY_MSG(detail::copyFromTo(ipOut, ipAddress), "ipAddress is insufficient");
        }
    }

    ::freeaddrinfo(res); // Free the linked list
    return Result(true);
}
