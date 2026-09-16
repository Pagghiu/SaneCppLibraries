// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include <WinSock2.h>
#include <Ws2tcpip.h> // sockadd_in6

using socklen_t = int;

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#if SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL
#pragma comment(lib, "Ws2_32.lib")
#endif

#include "../../Common/CompilerMacrosType.h"
#include "../../Socket/Socket.h"

#if SC_COMPILER_CLANG
#include <stdatomic.h>
#endif

SC::ResultSocket SC::detail::SocketDescriptorDefinition::releaseHandle(Handle& handle)
{
    const int      res         = ::closesocket(handle);
    const uint32_t nativeError = res == SOCKET_ERROR ? static_cast<uint32_t>(WSAGetLastError()) : 0;
    handle                     = SocketDescriptor::Invalid;
    return res == SOCKET_ERROR ? ResultSocket::withNativeError(SocketError::SocketCloseFailed,
                                                               SocketErrorDetail::WindowsCloseSocket, nativeError)
                               : ResultSocket();
}

SC::ResultSocket SC::SocketDescriptor::setInheritable(bool inheritable)
{
    BOOL res =
        ::SetHandleInformation(reinterpret_cast<HANDLE>(handle), HANDLE_FLAG_INHERIT, inheritable ? TRUE : FALSE);
    if (res == FALSE)
    {
        const uint32_t nativeError = static_cast<uint32_t>(GetLastError());
        return ResultSocket::withNativeError(SocketError::DescriptorConfigurationFailed,
                                             SocketErrorDetail::WindowsSetHandleInformation, nativeError);
    }
    return {};
}

SC::ResultSocket SC::SocketDescriptor::setBlocking(bool blocking)
{
    ULONG enable = blocking ? 0 : 1;
    if (::ioctlsocket(handle, FIONBIO, &enable) == SOCKET_ERROR)
    {
        const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
        return ResultSocket::withNativeError(SocketError::DescriptorConfigurationFailed,
                                             SocketErrorDetail::WindowsSetBlocking, nativeError);
    }
    return {};
}

SC::ResultSocket SC::SocketDescriptor::setTcpNoDelay(bool tcpNoDelay)
{
    int active = tcpNoDelay ? 1 : 0;
    if (::setsockopt(handle, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&active), sizeof(active)) ==
        SOCKET_ERROR)
    {
        const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
        return ResultSocket::withNativeError(SocketError::SocketOptionFailed, SocketErrorDetail::SetTcpNoDelay,
                                             nativeError);
    }
    return {};
}

SC::ResultSocket SC::SocketDescriptor::setBroadcast(bool enableBroadcast)
{
    int active = enableBroadcast ? 1 : 0;
    if (::setsockopt(handle, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&active), sizeof(active)) ==
        SOCKET_ERROR)
    {
        const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
        return ResultSocket::withNativeError(SocketError::SocketOptionFailed, SocketErrorDetail::SetBroadcast,
                                             nativeError);
    }
    return {};
}

SC::ResultSocket SC::SocketDescriptor::joinMulticastGroup(const SocketIPAddress& multicastAddress,
                                                          const SocketIPAddress& interfaceAddress)
{
    if (multicastAddress.getAddressFamily() != interfaceAddress.getAddressFamily())
    {
        return {SocketError::AddressFamilyMismatch};
    }
    if (multicastAddress.getAddressFamily() == SocketFlags::AddressFamilyIPV4)
    {
        struct ip_mreq mreq;
        mreq.imr_multiaddr = multicastAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        mreq.imr_interface = interfaceAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        if (::setsockopt(handle, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq)) ==
            SOCKET_ERROR)
        {
            const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed, SocketErrorDetail::JoinIPv4Multicast,
                                                 nativeError);
        }
        return {};
    }
    else
    {
        struct ipv6_mreq mreq;
        mreq.ipv6mr_multiaddr = multicastAddress.handle.reinterpret_as<struct sockaddr_in6>().sin6_addr;
        mreq.ipv6mr_interface = 0;
        if (::setsockopt(handle, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq),
                         sizeof(mreq)) == SOCKET_ERROR)
        {
            const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed, SocketErrorDetail::JoinIPv6Multicast,
                                                 nativeError);
        }
        return {};
    }
}

SC::ResultSocket SC::SocketDescriptor::leaveMulticastGroup(const SocketIPAddress& multicastAddress,
                                                           const SocketIPAddress& interfaceAddress)
{
    if (multicastAddress.getAddressFamily() != interfaceAddress.getAddressFamily())
    {
        return {SocketError::AddressFamilyMismatch};
    }
    if (multicastAddress.getAddressFamily() == SocketFlags::AddressFamilyIPV4)
    {
        struct ip_mreq mreq;
        mreq.imr_multiaddr = multicastAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        mreq.imr_interface = interfaceAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        if (::setsockopt(handle, IPPROTO_IP, IP_DROP_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq)) ==
            SOCKET_ERROR)
        {
            const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed, SocketErrorDetail::LeaveIPv4Multicast,
                                                 nativeError);
        }
        return {};
    }
    else
    {
        struct ipv6_mreq mreq;
        mreq.ipv6mr_multiaddr = multicastAddress.handle.reinterpret_as<struct sockaddr_in6>().sin6_addr;
        mreq.ipv6mr_interface = 0;
        if (::setsockopt(handle, IPPROTO_IPV6, IPV6_DROP_MEMBERSHIP, reinterpret_cast<const char*>(&mreq),
                         sizeof(mreq)) == SOCKET_ERROR)
        {
            const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed, SocketErrorDetail::LeaveIPv6Multicast,
                                                 nativeError);
        }
        return {};
    }
}

SC::ResultSocket SC::SocketDescriptor::setMulticastLoopback(SocketFlags::AddressFamily addressFamily,
                                                            bool                       enableLoopback)
{
    int active = enableLoopback ? 1 : 0;
    if (addressFamily == SocketFlags::AddressFamilyIPV4)
    {
        if (::setsockopt(handle, IPPROTO_IP, IP_MULTICAST_LOOP, reinterpret_cast<const char*>(&active),
                         sizeof(active)) == SOCKET_ERROR)
        {
            const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed,
                                                 SocketErrorDetail::SetIPv4MulticastLoopback, nativeError);
        }
        return {};
    }
    else
    {
        if (::setsockopt(handle, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, reinterpret_cast<const char*>(&active),
                         sizeof(active)) == SOCKET_ERROR)
        {
            const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed,
                                                 SocketErrorDetail::SetIPv6MulticastLoopback, nativeError);
        }
        return {};
    }
}

SC::ResultSocket SC::SocketDescriptor::setMulticastHops(SocketFlags::AddressFamily addressFamily, int hops)
{
    if (addressFamily == SocketFlags::AddressFamilyIPV4)
    {
        DWORD dhops = hops;
        if (::setsockopt(handle, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&dhops), sizeof(dhops)) ==
            SOCKET_ERROR)
        {
            const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed,
                                                 SocketErrorDetail::SetIPv4MulticastHops, nativeError);
        }
        return {};
    }
    else
    {
        DWORD dhops = hops;
        if (::setsockopt(handle, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, reinterpret_cast<const char*>(&dhops),
                         sizeof(dhops)) == SOCKET_ERROR)
        {
            const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed,
                                                 SocketErrorDetail::SetIPv6MulticastHops, nativeError);
        }
        return {};
    }
}

SC::ResultSocket SC::SocketDescriptor::setMulticastOutboundInterface(const SocketIPAddress& interfaceAddress)
{
    if (interfaceAddress.getAddressFamily() == SocketFlags::AddressFamilyIPV4)
    {
        struct in_addr addr = interfaceAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        if (::setsockopt(handle, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char*>(&addr), sizeof(addr)) ==
            SOCKET_ERROR)
        {
            const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed,
                                                 SocketErrorDetail::SetIPv4MulticastInterface, nativeError);
        }
        return {};
    }
    else
    {
        const DWORD interfaceIndex = interfaceAddress.handle.reinterpret_as<struct sockaddr_in6>().sin6_scope_id;
        if (::setsockopt(handle, IPPROTO_IPV6, IPV6_MULTICAST_IF, reinterpret_cast<const char*>(&interfaceIndex),
                         sizeof(interfaceIndex)) == SOCKET_ERROR)
        {
            const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed,
                                                 SocketErrorDetail::SetIPv6MulticastInterface, nativeError);
        }
        return {};
    }
}

SC::ResultSocket SC::SocketDescriptor::isInheritable(bool& hasValue) const
{
    DWORD flags;
    if (::GetHandleInformation(reinterpret_cast<HANDLE>(handle), &flags) == FALSE)
    {
        const uint32_t nativeError = static_cast<uint32_t>(GetLastError());
        return ResultSocket::withNativeError(SocketError::DescriptorQueryFailed,
                                             SocketErrorDetail::WindowsGetHandleInformation, nativeError);
    }
    hasValue = (flags & HANDLE_FLAG_INHERIT) != 0;
    return {};
}
SC::ResultSocket SC::SocketDescriptor::shutdown(SocketFlags::ShutdownType shutdownType)
{
    int how = 0;
    switch (shutdownType)
    {
    case SocketFlags::ShutdownBoth: how = SD_BOTH; break;
    default: return {SocketError::InvalidShutdownType, SocketErrorDetail::ShutdownSocket};
    }
    if (::shutdown(handle, how) == 0)
    {
        return {};
    }
    const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
    return ResultSocket::withNativeError(SocketError::ShutdownFailed, SocketErrorDetail::ShutdownSocket, nativeError);
}

SC::ResultSocket SC::SocketDescriptor::create(SocketFlags::AddressFamily addressFamily,
                                              SocketFlags::SocketType socketType, SocketFlags::ProtocolType protocol,
                                              SocketFlags::BlockingType    blocking,
                                              SocketFlags::InheritableType inheritable)
{
    if (addressFamily == SocketFlags::AddressFamilyUnix)
        return {SocketError::OperationUnsupported};
    if (not SocketNetworking::isNetworkingInited())
        return {SocketError::NetworkingNotInitialized};
    SC_SOCKET_TRUST_RESULT(close());

    DWORD flags = WSA_FLAG_OVERLAPPED;
    if (inheritable == SocketFlags::NonInheritable)
    {
        flags |= WSA_FLAG_NO_HANDLE_INHERIT;
    }
    handle = ::WSASocketW(SocketFlags::toNative(addressFamily), SocketFlags::toNative(socketType),
                          SocketFlags::toNative(protocol), nullptr, 0, flags);
    if (!isValid())
    {
        const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
        return ResultSocket::withNativeError(SocketError::SocketCreationFailed, SocketErrorDetail::CreateSocket,
                                             nativeError);
    }
    SC_TRY(setBlocking(blocking == SocketFlags::Blocking));
    return {};
}

SC::ResultSocket SC::SocketDescriptor::sendTo(Span<const char> data, const SocketAddress& destination)
{
    if (not destination.isValid())
        return {SocketError::InvalidAddress, SocketErrorDetail::SendDatagram};
    const int addressSize = static_cast<int>(destination.sizeOfHandle());
    const int sent        = ::sendto(handle, data.data(), static_cast<int>(data.sizeInBytes()), 0,
                                     &destination.handle.reinterpret_as<const struct sockaddr>(), addressSize);
    if (sent == SOCKET_ERROR)
    {
        const uint32_t nativeError = static_cast<uint32_t>(WSAGetLastError());
        if (nativeError == WSAEWOULDBLOCK)
            return {SocketError::WouldBlock, SocketErrorDetail::SendDatagram};
        return ResultSocket::withNativeError(SocketError::SendFailed, SocketErrorDetail::SendDatagram, nativeError);
    }
    if (static_cast<size_t>(sent) != data.sizeInBytes())
        return ResultSocket::withActualBytes(SocketError::IncompleteSend, SocketErrorDetail::SendDatagram,
                                             static_cast<uint32_t>(sent));
    return {};
}

SC::ResultSocket SC::SocketDescriptor::sendTo(Span<const char> data, const SocketIPAddress& destination)
{
    return sendTo(data, SocketAddress(destination));
}

SC::ResultSocket SC::SocketDescriptor::receiveFrom(Span<char> buffer, Span<char>& receivedData,
                                                   SocketAddress& sourceAddress)
{
    SocketAddress receivedSourceAddress;
    int           addressSize  = sizeof(receivedSourceAddress.handle);
    const int     received     = ::recvfrom(handle, buffer.data(), static_cast<int>(buffer.sizeInBytes()), 0,
                                            &receivedSourceAddress.handle.reinterpret_as<struct sockaddr>(), &addressSize);
    const int     receiveError = received == SOCKET_ERROR ? WSAGetLastError() : 0;
    if (receiveError == WSAEWOULDBLOCK)
        return {SocketError::WouldBlock, SocketErrorDetail::ReceiveDatagram};
    if (receiveError == WSAEMSGSIZE)
        return {SocketError::DatagramTruncated, SocketErrorDetail::ReceiveDatagram};
    if (received == SOCKET_ERROR)
        return ResultSocket::withNativeError(SocketError::ReceiveFailed, SocketErrorDetail::ReceiveDatagram,
                                             static_cast<uint32_t>(receiveError));
    receivedSourceAddress.nativeSize = static_cast<uint32_t>(addressSize);
    receivedData                     = {buffer.data(), static_cast<size_t>(received)};
    sourceAddress                    = receivedSourceAddress;
    return {};
}

SC::ResultSocket SC::SocketDescriptor::receiveFrom(Span<char> buffer, Span<char>& receivedData,
                                                   SocketIPAddress& sourceAddress)
{
    Span<char>    stagedReceivedData;
    SocketAddress stagedSourceAddress;
    SC_TRY(receiveFrom(buffer, stagedReceivedData, stagedSourceAddress));

    SocketIPAddress stagedIPAddress;
    SC_TRY(stagedSourceAddress.getIPAddress(stagedIPAddress));
    receivedData  = stagedReceivedData;
    sourceAddress = stagedIPAddress;
    return {};
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
        SC_SOCKET_ASSERT_RELEASE(WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
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
