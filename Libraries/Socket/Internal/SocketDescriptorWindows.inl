// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include <WinSock2.h>
#include <Ws2tcpip.h> // sockadd_in6

using socklen_t = int;

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#pragma comment(lib, "Ws2_32.lib")

#include "../../Foundation/Assert.h"
#include "../../Foundation/Compiler.h"
#include "../../Socket/Socket.h"

#if SC_COMPILER_CLANG
#include <stdatomic.h>
#endif

SC::Result SC::detail::SocketDescriptorDefinition::releaseHandle(Handle& handle)
{
    const int res = ::closesocket(handle);
    handle        = SocketDescriptor::Invalid;
    return Result(res != -1);
}

SC::Result SC::SocketDescriptor::setInheritable(bool inheritable)
{
    BOOL res =
        ::SetHandleInformation(reinterpret_cast<HANDLE>(handle), HANDLE_FLAG_INHERIT, inheritable ? TRUE : FALSE);
    return res == FALSE ? Result::Error("SetHandleInformation failed") : Result(true);
}

SC::Result SC::SocketDescriptor::setBlocking(bool blocking)
{
    ULONG enable = blocking ? 0 : 1;
    if (::ioctlsocket(handle, FIONBIO, &enable) == SOCKET_ERROR)
    {
        return Result::Error("ioctlsocket failed");
    }
    return Result(true);
}

SC::Result SC::SocketDescriptor::setTcpNoDelay(bool tcpNoDelay)
{
    int active = tcpNoDelay ? 1 : 0;
    if (::setsockopt(handle, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&active), sizeof(active)) ==
        SOCKET_ERROR)
    {
        return Result::Error("setsockopt TCP_NODELAY failed");
    }
    return Result(true);
}

SC::Result SC::SocketDescriptor::setBroadcast(bool enableBroadcast)
{
    int active = enableBroadcast ? 1 : 0;
    if (::setsockopt(handle, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&active), sizeof(active)) ==
        SOCKET_ERROR)
    {
        return Result::Error("setsockopt SO_BROADCAST failed");
    }
    return Result(true);
}

SC::Result SC::SocketDescriptor::joinMulticastGroup(const SocketIPAddress& multicastAddress,
                                                    const SocketIPAddress& interfaceAddress)
{
    if (multicastAddress.getAddressFamily() != interfaceAddress.getAddressFamily())
    {
        return Result::Error("multicast and interface address families do not match");
    }
    if (multicastAddress.getAddressFamily() == SocketFlags::AddressFamilyIPV4)
    {
        struct ip_mreq mreq;
        mreq.imr_multiaddr = multicastAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        mreq.imr_interface = interfaceAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        if (::setsockopt(handle, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq)) ==
            SOCKET_ERROR)
        {
            return Result::Error("setsockopt IP_ADD_MEMBERSHIP failed");
        }
        return Result(true);
    }
    else
    {
        struct ipv6_mreq mreq;
        mreq.ipv6mr_multiaddr = multicastAddress.handle.reinterpret_as<struct sockaddr_in6>().sin6_addr;
        mreq.ipv6mr_interface = 0;
        if (::setsockopt(handle, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq),
                         sizeof(mreq)) == SOCKET_ERROR)
        {
            return Result::Error("setsockopt IPV6_ADD_MEMBERSHIP failed");
        }
        return Result(true);
    }
}

SC::Result SC::SocketDescriptor::leaveMulticastGroup(const SocketIPAddress& multicastAddress,
                                                     const SocketIPAddress& interfaceAddress)
{
    if (multicastAddress.getAddressFamily() != interfaceAddress.getAddressFamily())
    {
        return Result::Error("multicast and interface address families do not match");
    }
    if (multicastAddress.getAddressFamily() == SocketFlags::AddressFamilyIPV4)
    {
        struct ip_mreq mreq;
        mreq.imr_multiaddr = multicastAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        mreq.imr_interface = interfaceAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        if (::setsockopt(handle, IPPROTO_IP, IP_DROP_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq)) ==
            SOCKET_ERROR)
        {
            return Result::Error("setsockopt IP_DROP_MEMBERSHIP failed");
        }
        return Result(true);
    }
    else
    {
        struct ipv6_mreq mreq;
        mreq.ipv6mr_multiaddr = multicastAddress.handle.reinterpret_as<struct sockaddr_in6>().sin6_addr;
        mreq.ipv6mr_interface = 0;
        if (::setsockopt(handle, IPPROTO_IPV6, IPV6_DROP_MEMBERSHIP, reinterpret_cast<const char*>(&mreq),
                         sizeof(mreq)) == SOCKET_ERROR)
        {
            return Result::Error("setsockopt IPV6_DROP_MEMBERSHIP failed");
        }
        return Result(true);
    }
}

SC::Result SC::SocketDescriptor::setMulticastLoopback(SocketFlags::AddressFamily addressFamily, bool enableLoopback)
{
    int active = enableLoopback ? 1 : 0;
    if (addressFamily == SocketFlags::AddressFamilyIPV4)
    {
        if (::setsockopt(handle, IPPROTO_IP, IP_MULTICAST_LOOP, reinterpret_cast<const char*>(&active),
                         sizeof(active)) == SOCKET_ERROR)
        {
            return Result::Error("setsockopt IP_MULTICAST_LOOP failed");
        }
        return Result(true);
    }
    else
    {
        if (::setsockopt(handle, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, reinterpret_cast<const char*>(&active),
                         sizeof(active)) == SOCKET_ERROR)
        {
            return Result::Error("setsockopt IPV6_MULTICAST_LOOP failed");
        }
        return Result(true);
    }
}

SC::Result SC::SocketDescriptor::setMulticastHops(SocketFlags::AddressFamily addressFamily, int hops)
{
    if (addressFamily == SocketFlags::AddressFamilyIPV4)
    {
        DWORD dhops = hops;
        if (::setsockopt(handle, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&dhops), sizeof(dhops)) ==
            SOCKET_ERROR)
        {
            return Result::Error("setsockopt IP_MULTICAST_TTL failed");
        }
        return Result(true);
    }
    else
    {
        DWORD dhops = hops;
        if (::setsockopt(handle, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, reinterpret_cast<const char*>(&dhops),
                         sizeof(dhops)) == SOCKET_ERROR)
        {
            return Result::Error("setsockopt IPV6_MULTICAST_HOPS failed");
        }
        return Result(true);
    }
}

SC::Result SC::SocketDescriptor::setMulticastOutboundInterface(const SocketIPAddress& interfaceAddress)
{
    if (interfaceAddress.getAddressFamily() == SocketFlags::AddressFamilyIPV4)
    {
        struct in_addr addr = interfaceAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        if (::setsockopt(handle, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char*>(&addr), sizeof(addr)) ==
            SOCKET_ERROR)
        {
            return Result::Error("setsockopt IP_MULTICAST_IF failed");
        }
        return Result(true);
    }
    else
    {
        const DWORD interfaceIndex = interfaceAddress.handle.reinterpret_as<struct sockaddr_in6>().sin6_scope_id;
        if (::setsockopt(handle, IPPROTO_IPV6, IPV6_MULTICAST_IF, reinterpret_cast<const char*>(&interfaceIndex),
                         sizeof(interfaceIndex)) == SOCKET_ERROR)
        {
            return Result::Error("setsockopt IPV6_MULTICAST_IF failed");
        }
        return Result(true);
    }
}

SC::Result SC::SocketDescriptor::isInheritable(bool& hasValue) const
{
    DWORD flags;
    if (::GetHandleInformation(reinterpret_cast<HANDLE>(handle), &flags) == FALSE)
    {
        return Result::Error("GetHandleInformation failed");
    }
    hasValue = (flags & HANDLE_FLAG_INHERIT) != 0;
    return Result(true);
}
SC::Result SC::SocketDescriptor::shutdown(SocketFlags::ShutdownType shutdownType)
{
    int how = 0;
    switch (shutdownType)
    {
    case SocketFlags::ShutdownBoth: how = SD_BOTH; break;
    default: return Result::Error("Invalid shutdown type");
    }
    if (::shutdown(handle, how) == 0)
    {
        return Result(true);
    }
    return Result::Error("Failed to shutdown socket");
}

SC::Result SC::SocketDescriptor::create(SocketFlags::AddressFamily addressFamily, SocketFlags::SocketType socketType,
                                        SocketFlags::ProtocolType protocol, SocketFlags::BlockingType blocking,
                                        SocketFlags::InheritableType inheritable)
{
    SC_TRY(SocketNetworking::isNetworkingInited());
    SC_TRUST_RESULT(close());

    DWORD flags = WSA_FLAG_OVERLAPPED;
    if (inheritable == SocketFlags::NonInheritable)
    {
        flags |= WSA_FLAG_NO_HANDLE_INHERIT;
    }
    handle = ::WSASocketW(SocketFlags::toNative(addressFamily), SocketFlags::toNative(socketType),
                          SocketFlags::toNative(protocol), nullptr, 0, flags);
    if (!isValid())
    {
        return Result::Error("WSASocketW failed");
    }
    SC_TRY(setBlocking(blocking == SocketFlags::Blocking));
    return Result(isValid());
}

struct SC::SocketNetworking::Internal
{
#if SC_COMPILER_MSVC
    volatile long networkingInited = 0;
#elif SC_COMPILER_CLANG
    _Atomic bool networkingInited = false;
#elif SC_COMPILER_GCC
    volatile bool networkingInited = false;

    __attribute__((always_inline)) inline bool load() { return __atomic_load_n(&networkingInited, __ATOMIC_SEQ_CST); }
    __attribute__((always_inline)) inline void store(bool value)
    {
        __atomic_store_n(&networkingInited, value, __ATOMIC_SEQ_CST);
    }
#endif

    static Internal& get()
    {
        static Internal internal;
        return internal;
    }
};

bool SC::SocketNetworking::isNetworkingInited()
{
#if SC_COMPILER_MSVC
    return InterlockedCompareExchange(&Internal::get().networkingInited, 0, 0) != 0;
#elif SC_COMPILER_CLANG
    return atomic_load(&Internal::get().networkingInited);
#elif SC_COMPILER_GCC
    return Internal::get().load();
#endif
}

void SC::SocketNetworking::initNetworking()
{
    if (isNetworkingInited() == false)
    {
        WSADATA wsa;
        SC_ASSERT_RELEASE(WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
#if SC_COMPILER_MSVC
        InterlockedExchange(&Internal::get().networkingInited, 1);
#elif SC_COMPILER_CLANG
        atomic_store(&Internal::get().networkingInited, true);
#elif SC_COMPILER_GCC
        Internal::get().store(true);
#endif
    }
}

void SC::SocketNetworking::shutdownNetworking()
{
    WSACleanup();
#if SC_COMPILER_MSVC
    InterlockedExchange(&Internal::get().networkingInited, 0);
#elif SC_COMPILER_CLANG
    atomic_store(&Internal::get().networkingInited, false);
#elif SC_COMPILER_GCC
    Internal::get().store(false);
#endif
}
