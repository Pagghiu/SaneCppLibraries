// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../SocketDescriptor.h"

#include "../../Strings/SmallString.h"
#include "../../Strings/StringConverter.h"

SC::Result SC::SocketDNS::resolveDNS(StringView host, String& ipAddress)
{
    struct addrinfo hints, *res, *p;

    // Setup hints structure
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;   // Use either IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // Use SOCK_STREAM for TCP

    StringConverter converter(ipAddress);

    StringView nullTerminated;
    SC_TRY(converter.convertNullTerminateFastPath(host, nullTerminated));
    // Get address information
    const int status = ::getaddrinfo(nullTerminated.bytesIncludingTerminator(), NULL, &hints, &res);
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

        // Convert IP address to a readable string
        char ipstr[INET6_ADDRSTRLEN];
        ::inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        SC_TRY(ipAddress.assign(StringView::fromNullTerminated(ipstr, StringEncoding::Ascii)));
    }

    ::freeaddrinfo(res); // Free the linked list
    return Result(true);
}
