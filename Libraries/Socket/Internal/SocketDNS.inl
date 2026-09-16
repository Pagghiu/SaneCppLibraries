// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Socket/Socket.h"
#include "SocketInternal.h"

#if !SC_PLATFORM_WINDOWS
#include <arpa/inet.h> // inet_ntop
#include <netdb.h>     // AF_INET / IPPROTO_TCP / AF_UNSPEC
#endif
#include <string.h>

SC::ResultSocket SC::SocketDNS::resolveDNS(StringSpan host, Span<char>& ipAddress)
{
    struct addrinfo hints, *res, *p;

    // Setup hints structure
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;   // Use either IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // Use SOCK_STREAM for TCP

    char nullTerminated[256] = {0};
    if (not detail::isASCII(host))
        return {SocketError::UnsupportedTextEncoding, SocketErrorDetail::CopyHostName};
    if (not detail::writeNullTerminatedToBuffer(host.toCharSpan(), nullTerminated))
        return ResultSocket::withRequiredBytes(SocketError::InputCapacityExceeded, SocketErrorDetail::CopyHostName,
                                               static_cast<uint32_t>(host.sizeInBytes() + 1));
    // Get address information
    const int resolverError = ::getaddrinfo(nullTerminated, NULL, &hints, &res);
    if (resolverError != 0)
        return ResultSocket::withResolverError(SocketError::DNSResolutionFailed, SocketErrorDetail::ResolveHostName,
                                               resolverError);

    // Loop through results and print IP addresses
    ResultSocket result;
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
            if (not detail::copyFromTo(ipOut, ipAddress))
            {
                result = ResultSocket::withRequiredBytes(SocketError::OutputCapacityExceeded,
                                                         SocketErrorDetail::CopyResolvedAddress,
                                                         static_cast<uint32_t>(ipOut.sizeInBytes() + 1));
                break;
            }
        }
    }

    ::freeaddrinfo(res); // Free the linked list
    return result;
}
