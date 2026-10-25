// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncUV.h"

#include "../../../../Libraries/Socket/Socket.h"
#if !SC_PLATFORM_WINDOWS
#include <net/if.h>
#endif
#include <string.h>

UV_API int uv_interface_addresses(uv_interface_address_t** addresses, int* count)
{
    if (addresses == nullptr or count == nullptr)
    {
        return UV_EINVAL;
    }
    *addresses = nullptr;
    *count     = 0;
    return 0;
}
UV_API void uv_free_interface_addresses(uv_interface_address_t*, int) {}

UV_API int uv_ip4_addr(const char* ip, int port, struct sockaddr_in* addr)
{
    using namespace SC;
    SocketIPAddress ipAddress;
    StringSpan      ipStringView = {{ip, ::strlen(ip)}, true, StringEncoding::Ascii};
    Result          res          = ipAddress.fromAddressPort(ipStringView, static_cast<uint16_t>(port));
    if (not res or (ipAddress.getAddressFamily() != SC::SocketFlags::AddressFamilyIPV4))
        return UV_EINVAL;
#ifdef SIN6_LEN
    ipAddress.handle.reinterpret_as<sockaddr_in>().sin_len = sizeof(sockaddr_in);
#endif
    memcpy(addr, &ipAddress.handle.reinterpret_as<sockaddr_in>(), sizeof(sockaddr_in));
    return 0;
}

UV_API int uv_ip6_addr(const char* ip, int port, struct sockaddr_in6* addr)
{
    using namespace SC;
    SocketIPAddress ipAddress;
    StringSpan      ipStringView = {{ip, ::strlen(ip)}, true, StringEncoding::Ascii};
    Result          res          = ipAddress.fromAddressPort(ipStringView, static_cast<uint16_t>(port));
    if (not res or (ipAddress.getAddressFamily() != SC::SocketFlags::AddressFamilyIPV6))
        return UV_EINVAL;
#ifdef SIN6_LEN
    ipAddress.handle.reinterpret_as<sockaddr_in>().sin_len = sizeof(sockaddr_in6);
#endif
    memcpy(addr, &ipAddress.handle.reinterpret_as<sockaddr_in6>(), sizeof(sockaddr_in6));
    return 0;
}

UV_API int uv_inet_ntop(int af, const void* src, char* dst, size_t size)
{
#if SC_PLATFORM_WINDOWS
    const char* result = InetNtopA(af, const_cast<void*>(src), dst, static_cast<DWORD>(size));
#else
    const char* result = inet_ntop(af, src, dst, static_cast<socklen_t>(size));
#endif
    if (result == nullptr)
    {
        return UV_ENOSPC;
    }
    return 0;
}

UV_API int uv_inet_pton(int af, const char* src, void* dst)
{
    if (af != AF_INET and af != AF_INET6)
    {
        return UV_EAFNOSUPPORT;
    }
    const char* address = src;
    char        addressStorage[128];
    if (af == AF_INET6)
    {
        const char* scope = strchr(src, '%');
        if (scope != nullptr)
        {
            const size_t length = static_cast<size_t>(scope - src);
            if (length == 0 or length >= sizeof(addressStorage))
            {
                return UV_EINVAL;
            }
            memcpy(addressStorage, src, length);
            addressStorage[length] = '\0';
            address                = addressStorage;
        }
    }
#if SC_PLATFORM_WINDOWS
    int result = InetPtonA(af, address, dst);
#else
    int result = inet_pton(af, address, dst);
#endif
    if (result <= 0)
    {
        return UV_EINVAL;
    }
    else
    {
        return 0;
    }
}

UV_API int uv_if_indextoname(unsigned int ifindex, char* buffer, size_t* size)
{
    if (buffer == nullptr or size == nullptr or *size == 0)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    if (ifindex == static_cast<unsigned int>(-1))
    {
        return UV_ENOENT;
    }
    char local[32];
    snprintf(local, sizeof(local), "%u", ifindex);
    const size_t length = strlen(local);
    if (*size <= length)
    {
        *size = length + 1;
        return UV_ENOBUFS;
    }
    memcpy(buffer, local, length + 1);
    *size = length;
    return 0;
#else
    char local[UV_IF_NAMESIZE];
    if (if_indextoname(ifindex, local) == nullptr)
    {
        return UV_ENOENT;
    }
    const size_t length = strlen(local);
    if (*size <= length)
    {
        *size = length + 1;
        return UV_ENOBUFS;
    }
    memcpy(buffer, local, length + 1);
    *size = length;
    return 0;
#endif
}

UV_API int uv_if_indextoiid(unsigned int ifindex, char* buffer, size_t* size)
{
    return uv_if_indextoname(ifindex, buffer, size);
}
