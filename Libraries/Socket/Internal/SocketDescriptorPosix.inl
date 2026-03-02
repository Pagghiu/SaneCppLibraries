// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Socket/Socket.h"

#include "../../Foundation/Assert.h"
#include "../../Foundation/Deferred.h"

#include <arpa/inet.h>   // inet_pton
#include <errno.h>       // errno
#include <fcntl.h>       // fcntl
#include <netdb.h>       // AF_INET / IPPROTO_TCP / AF_UNSPEC
#include <netinet/tcp.h> // TCP_NODELAY
#include <unistd.h>      // close

namespace SC
{
namespace
{
static Result getFileFlags(int flagRead, const int fileDescriptor, int& outFlags)
{
    do
    {
        outFlags = ::fcntl(fileDescriptor, flagRead);
    } while (outFlags == -1 && errno == EINTR);
    SC_TRY_MSG(outFlags != -1, "fcntl getFlag failed");
    return Result(true);
}
static Result setFileFlags(int flagRead, int flagWrite, const int fileDescriptor, const bool setFlag, const int flag)
{
    int oldFlags;
    do
    {
        oldFlags = ::fcntl(fileDescriptor, flagRead);
    } while (oldFlags == -1 && errno == EINTR);
    SC_TRY_MSG(oldFlags != -1, "fcntl getFlag failed");
    const int newFlags = setFlag ? oldFlags | flag : oldFlags & (~flag);
    if (newFlags != oldFlags)
    {
        int res;
        do
        {
            res = ::fcntl(fileDescriptor, flagWrite, newFlags);
        } while (res == -1 && errno == EINTR);
        SC_TRY_MSG(res == 0, "fcntl setFlag failed");
    }
    return Result(true);
}
template <int flag>
static Result hasFileDescriptorFlags(int fileDescriptor, bool& hasFlag)
{
    static_assert(flag == FD_CLOEXEC, "hasFileDescriptorFlags invalid value");
    int flags = 0;
    SC_TRY(getFileFlags(F_GETFD, fileDescriptor, flags));
    hasFlag = (flags & flag) != 0;
    return Result(true);
}
template <int flag>
static Result setFileDescriptorFlags(int fileDescriptor, bool setFlag)
{
    static_assert(flag == FD_CLOEXEC, "setFileDescriptorFlags invalid value");
    return setFileFlags(F_GETFD, F_SETFD, fileDescriptor, setFlag, flag);
}
template <int flag>
static Result hasFileStatusFlags(int fileDescriptor, bool& hasFlag)
{
    static_assert(flag == O_NONBLOCK, "hasFileStatusFlags invalid value");
    int flags = 0;
    SC_TRY(getFileFlags(F_GETFL, fileDescriptor, flags));
    hasFlag = (flags & flag) != 0;
    return Result(true);
}
template <int flag>
static Result setFileStatusFlags(int fileDescriptor, bool setFlag)
{
    static_assert(flag == O_NONBLOCK, "setFileStatusFlags invalid value");
    return setFileFlags(F_GETFL, F_SETFL, fileDescriptor, setFlag, flag);
}
} // namespace

Result detail::SocketDescriptorDefinition::releaseHandle(Handle& handle)
{
    ::close(handle);
    handle = Invalid;
    return Result(true);
}

Result SocketDescriptor::setInheritable(bool inheritable)
{
    // On POSIX, inheritable = false means set FD_CLOEXEC
    return setFileDescriptorFlags<FD_CLOEXEC>(handle, !inheritable);
}

Result SocketDescriptor::setBlocking(bool blocking)
{
    // On POSIX, blocking = false means set O_NONBLOCK
    return setFileStatusFlags<O_NONBLOCK>(handle, !blocking);
}

Result SocketDescriptor::setTcpNoDelay(bool tcpNoDelay)
{
    int active = tcpNoDelay ? 1 : 0;
    SC_TRY_MSG(::setsockopt(handle, IPPROTO_TCP, TCP_NODELAY, &active, sizeof(active)) == 0,
               "setsockopt TCP_NODELAY failed");
    return Result(true);
}

Result SocketDescriptor::setBroadcast(bool enableBroadcast)
{
    int active = enableBroadcast ? 1 : 0;
    SC_TRY_MSG(::setsockopt(handle, SOL_SOCKET, SO_BROADCAST, &active, sizeof(active)) == 0,
               "setsockopt SO_BROADCAST failed");
    return Result(true);
}

Result SocketDescriptor::joinMulticastGroup(const SocketIPAddress& multicastAddress,
                                            const SocketIPAddress& interfaceAddress)
{
    SC_TRY_MSG(multicastAddress.getAddressFamily() == interfaceAddress.getAddressFamily(),
               "multicast and interface address families do not match");
    if (multicastAddress.getAddressFamily() == SocketFlags::AddressFamilyIPV4)
    {
        struct ip_mreq mreq;
        mreq.imr_multiaddr = multicastAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        mreq.imr_interface = interfaceAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        SC_TRY_MSG(::setsockopt(handle, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == 0,
                   "setsockopt IP_ADD_MEMBERSHIP failed");
        return Result(true);
    }
    else
    {
        struct ipv6_mreq mreq;
        mreq.ipv6mr_multiaddr = multicastAddress.handle.reinterpret_as<struct sockaddr_in6>().sin6_addr;
        mreq.ipv6mr_interface = 0;
        SC_TRY_MSG(::setsockopt(handle, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) == 0,
                   "setsockopt IPV6_JOIN_GROUP failed");
        return Result(true);
    }
}

Result SocketDescriptor::leaveMulticastGroup(const SocketIPAddress& multicastAddress,
                                             const SocketIPAddress& interfaceAddress)
{
    SC_TRY_MSG(multicastAddress.getAddressFamily() == interfaceAddress.getAddressFamily(),
               "multicast and interface address families do not match");
    if (multicastAddress.getAddressFamily() == SocketFlags::AddressFamilyIPV4)
    {
        struct ip_mreq mreq;
        mreq.imr_multiaddr = multicastAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        mreq.imr_interface = interfaceAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        SC_TRY_MSG(::setsockopt(handle, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) == 0,
                   "setsockopt IP_DROP_MEMBERSHIP failed");
        return Result(true);
    }
    else
    {
        struct ipv6_mreq mreq;
        mreq.ipv6mr_multiaddr = multicastAddress.handle.reinterpret_as<struct sockaddr_in6>().sin6_addr;
        mreq.ipv6mr_interface = 0;
        SC_TRY_MSG(::setsockopt(handle, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &mreq, sizeof(mreq)) == 0,
                   "setsockopt IPV6_LEAVE_GROUP failed");
        return Result(true);
    }
}

Result SocketDescriptor::setMulticastLoopback(SocketFlags::AddressFamily addressFamily, bool enableLoopback)
{
    int active = enableLoopback ? 1 : 0;
    if (addressFamily == SocketFlags::AddressFamilyIPV4)
    {
        SC_TRY_MSG(::setsockopt(handle, IPPROTO_IP, IP_MULTICAST_LOOP, &active, sizeof(active)) == 0,
                   "setsockopt IP_MULTICAST_LOOP failed");
        return Result(true);
    }
    else
    {
        SC_TRY_MSG(::setsockopt(handle, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &active, sizeof(active)) == 0,
                   "setsockopt IPV6_MULTICAST_LOOP failed");
        return Result(true);
    }
}

Result SocketDescriptor::setMulticastHops(SocketFlags::AddressFamily addressFamily, int hops)
{
    if (addressFamily == SocketFlags::AddressFamilyIPV4)
    {
        SC_TRY_MSG(::setsockopt(handle, IPPROTO_IP, IP_MULTICAST_TTL, &hops, sizeof(hops)) == 0,
                   "setsockopt IP_MULTICAST_TTL failed");
        return Result(true);
    }
    else
    {
        SC_TRY_MSG(::setsockopt(handle, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof(hops)) == 0,
                   "setsockopt IPV6_MULTICAST_HOPS failed");
        return Result(true);
    }
}

Result SocketDescriptor::setMulticastOutboundInterface(const SocketIPAddress& interfaceAddress)
{
    if (interfaceAddress.getAddressFamily() == SocketFlags::AddressFamilyIPV4)
    {
        struct in_addr addr = interfaceAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        SC_TRY_MSG(::setsockopt(handle, IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr)) == 0,
                   "setsockopt IP_MULTICAST_IF failed");
        return Result(true);
    }
    else
    {
        const unsigned int interfaceIndex = interfaceAddress.handle.reinterpret_as<struct sockaddr_in6>().sin6_scope_id;
        SC_TRY_MSG(::setsockopt(handle, IPPROTO_IPV6, IPV6_MULTICAST_IF, &interfaceIndex, sizeof(interfaceIndex)) == 0,
                   "setsockopt IPV6_MULTICAST_IF failed");
        return Result(true);
    }
}

Result SocketDescriptor::isInheritable(bool& hasValue) const
{
    bool cloexec = false;
    SC_TRY(hasFileDescriptorFlags<FD_CLOEXEC>(handle, cloexec));
    hasValue = !cloexec;
    return Result(true);
}

Result SocketDescriptor::shutdown(SocketFlags::ShutdownType shutdownType)
{
    SC_TRY_MSG(shutdownType == SocketFlags::ShutdownBoth, "Invalid shutdown type");
    int how = 0;
    SC_TRY_MSG(::shutdown(handle, how) == 0, "shutdown failed");
    return Result(true);
}

Result SocketDescriptor::create(SocketFlags::AddressFamily addressFamily, SocketFlags::SocketType socketType,
                                SocketFlags::ProtocolType protocol, SocketFlags::BlockingType blocking,
                                SocketFlags::InheritableType inheritable)
{
    SC_TRY(SocketNetworking::isNetworkingInited());
    SC_TRUST_RESULT(close());

    int typeWithAdditions = SocketFlags::toNative(socketType);
#if defined(SOCK_NONBLOCK)
    if (blocking == SocketFlags::NonBlocking)
    {
        typeWithAdditions |= SOCK_NONBLOCK;
    }
#endif // defined(SOCK_NONBLOCK)
#if defined(SOCK_CLOEXEC)
    if (inheritable == SocketFlags::NonInheritable)
    {
        typeWithAdditions |= SOCK_CLOEXEC;
    }
#endif // defined(SOCK_CLOEXEC)
    do
    {
        handle = ::socket(SocketFlags::toNative(addressFamily), typeWithAdditions, SocketFlags::toNative(protocol));
    } while (handle == -1 and errno == EINTR);
#if !defined(SOCK_CLOEXEC)
    if (inheritable == SocketFlags::NonInheritable)
    {
        SC_TRY(setInheritable(false));
    }
#endif // !defined(SOCK_CLOEXEC)
#if !defined(SOCK_NONBLOCK)
    if (blocking == SocketFlags::NonBlocking)
    {
        SC_TRY(setBlocking(false));
    }
#endif // !defined(SOCK_NONBLOCK)

#if defined(SO_NOSIGPIPE)
    {
        int active = 1;
        ::setsockopt(handle, SOL_SOCKET, SO_NOSIGPIPE, &active, sizeof(active));
    }
#endif // defined(SO_NOSIGPIPE)
    return Result(isValid());
}

void SocketNetworking::initNetworking() {}
void SocketNetworking::shutdownNetworking() {}
bool SocketNetworking::isNetworkingInited() { return Result(true); }
} // namespace SC
