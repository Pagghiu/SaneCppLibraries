// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Socket/Socket.h"

#include "../../Common/Deferred.h"

#include <arpa/inet.h>   // inet_pton
#include <errno.h>       // errno
#include <fcntl.h>       // fcntl
#include <netdb.h>       // AF_INET / IPPROTO_TCP / AF_UNSPEC
#include <netinet/tcp.h> // TCP_NODELAY
#include <sys/socket.h>  // sendto / recvmsg
#include <sys/uio.h>     // iovec
#include <unistd.h>      // close

namespace SC
{
namespace
{
static ResultSocket getFileFlags(int flagRead, const int fileDescriptor, int& outFlags)
{
    do
    {
        outFlags = ::fcntl(fileDescriptor, flagRead);
    } while (outFlags == -1 && errno == EINTR);
    if (outFlags == -1)
        return ResultSocket::withNativeError(SocketError::DescriptorQueryFailed,
                                             SocketErrorDetail::PosixGetDescriptorFlags, static_cast<uint32_t>(errno));
    return {};
}
static ResultSocket setFileFlags(int flagRead, int flagWrite, const int fileDescriptor, const bool setFlag,
                                 const int flag)
{
    int oldFlags;
    do
    {
        oldFlags = ::fcntl(fileDescriptor, flagRead);
    } while (oldFlags == -1 && errno == EINTR);
    if (oldFlags == -1)
        return ResultSocket::withNativeError(SocketError::DescriptorQueryFailed,
                                             SocketErrorDetail::PosixGetDescriptorFlags, static_cast<uint32_t>(errno));
    const int newFlags = setFlag ? oldFlags | flag : oldFlags & (~flag);
    if (newFlags != oldFlags)
    {
        int res;
        do
        {
            res = ::fcntl(fileDescriptor, flagWrite, newFlags);
        } while (res == -1 && errno == EINTR);
        if (res == -1)
            return ResultSocket::withNativeError(SocketError::DescriptorConfigurationFailed,
                                                 SocketErrorDetail::PosixSetDescriptorFlags,
                                                 static_cast<uint32_t>(errno));
    }
    return {};
}
template <int flag>
static ResultSocket hasFileDescriptorFlags(int fileDescriptor, bool& hasFlag)
{
    static_assert(flag == FD_CLOEXEC, "hasFileDescriptorFlags invalid value");
    int flags = 0;
    SC_TRY(getFileFlags(F_GETFD, fileDescriptor, flags));
    hasFlag = (flags & flag) != 0;
    return {};
}
template <int flag>
static ResultSocket setFileDescriptorFlags(int fileDescriptor, bool setFlag)
{
    static_assert(flag == FD_CLOEXEC, "setFileDescriptorFlags invalid value");
    return setFileFlags(F_GETFD, F_SETFD, fileDescriptor, setFlag, flag);
}
template <int flag>
static ResultSocket hasFileStatusFlags(int fileDescriptor, bool& hasFlag)
{
    static_assert(flag == O_NONBLOCK, "hasFileStatusFlags invalid value");
    int flags = 0;
    SC_TRY(getFileFlags(F_GETFL, fileDescriptor, flags));
    hasFlag = (flags & flag) != 0;
    return {};
}
template <int flag>
static ResultSocket setFileStatusFlags(int fileDescriptor, bool setFlag)
{
    static_assert(flag == O_NONBLOCK, "setFileStatusFlags invalid value");
    return setFileFlags(F_GETFL, F_SETFL, fileDescriptor, setFlag, flag);
}
} // namespace

ResultSocket detail::SocketDescriptorDefinition::releaseHandle(Handle& handle)
{
    ::close(handle);
    handle = Invalid;
    return {};
}

ResultSocket SocketDescriptor::setInheritable(bool inheritable)
{
    // On POSIX, inheritable = false means set FD_CLOEXEC
    return setFileDescriptorFlags<FD_CLOEXEC>(handle, !inheritable);
}

ResultSocket SocketDescriptor::setBlocking(bool blocking)
{
    // On POSIX, blocking = false means set O_NONBLOCK
    return setFileStatusFlags<O_NONBLOCK>(handle, !blocking);
}

ResultSocket SocketDescriptor::setTcpNoDelay(bool tcpNoDelay)
{
    int active = tcpNoDelay ? 1 : 0;
    if (::setsockopt(handle, IPPROTO_TCP, TCP_NODELAY, &active, sizeof(active)) != 0)
        return ResultSocket::withNativeError(SocketError::SocketOptionFailed, SocketErrorDetail::SetTcpNoDelay,
                                             static_cast<uint32_t>(errno));
    return {};
}

ResultSocket SocketDescriptor::setBroadcast(bool enableBroadcast)
{
    int active = enableBroadcast ? 1 : 0;
    if (::setsockopt(handle, SOL_SOCKET, SO_BROADCAST, &active, sizeof(active)) != 0)
        return ResultSocket::withNativeError(SocketError::SocketOptionFailed, SocketErrorDetail::SetBroadcast,
                                             static_cast<uint32_t>(errno));
    return {};
}

ResultSocket SocketDescriptor::joinMulticastGroup(const SocketIPAddress& multicastAddress,
                                                  const SocketIPAddress& interfaceAddress)
{
    if (multicastAddress.getAddressFamily() != interfaceAddress.getAddressFamily())
        return {SocketError::AddressFamilyMismatch};
    if (multicastAddress.getAddressFamily() == SocketFlags::AddressFamilyIPV4)
    {
        struct ip_mreq mreq;
        mreq.imr_multiaddr = multicastAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        mreq.imr_interface = interfaceAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        if (::setsockopt(handle, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) != 0)
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed, SocketErrorDetail::JoinIPv4Multicast,
                                                 static_cast<uint32_t>(errno));
        return {};
    }
    else
    {
        struct ipv6_mreq mreq;
        mreq.ipv6mr_multiaddr = multicastAddress.handle.reinterpret_as<struct sockaddr_in6>().sin6_addr;
        mreq.ipv6mr_interface = 0;
        if (::setsockopt(handle, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) != 0)
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed, SocketErrorDetail::JoinIPv6Multicast,
                                                 static_cast<uint32_t>(errno));
        return {};
    }
}

ResultSocket SocketDescriptor::leaveMulticastGroup(const SocketIPAddress& multicastAddress,
                                                   const SocketIPAddress& interfaceAddress)
{
    if (multicastAddress.getAddressFamily() != interfaceAddress.getAddressFamily())
        return {SocketError::AddressFamilyMismatch};
    if (multicastAddress.getAddressFamily() == SocketFlags::AddressFamilyIPV4)
    {
        struct ip_mreq mreq;
        mreq.imr_multiaddr = multicastAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        mreq.imr_interface = interfaceAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        if (::setsockopt(handle, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) != 0)
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed, SocketErrorDetail::LeaveIPv4Multicast,
                                                 static_cast<uint32_t>(errno));
        return {};
    }
    else
    {
        struct ipv6_mreq mreq;
        mreq.ipv6mr_multiaddr = multicastAddress.handle.reinterpret_as<struct sockaddr_in6>().sin6_addr;
        mreq.ipv6mr_interface = 0;
        if (::setsockopt(handle, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &mreq, sizeof(mreq)) != 0)
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed, SocketErrorDetail::LeaveIPv6Multicast,
                                                 static_cast<uint32_t>(errno));
        return {};
    }
}

ResultSocket SocketDescriptor::setMulticastLoopback(SocketFlags::AddressFamily addressFamily, bool enableLoopback)
{
    int active = enableLoopback ? 1 : 0;
    if (addressFamily == SocketFlags::AddressFamilyIPV4)
    {
        if (::setsockopt(handle, IPPROTO_IP, IP_MULTICAST_LOOP, &active, sizeof(active)) != 0)
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed,
                                                 SocketErrorDetail::SetIPv4MulticastLoopback,
                                                 static_cast<uint32_t>(errno));
        return {};
    }
    else
    {
        if (::setsockopt(handle, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &active, sizeof(active)) != 0)
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed,
                                                 SocketErrorDetail::SetIPv6MulticastLoopback,
                                                 static_cast<uint32_t>(errno));
        return {};
    }
}

ResultSocket SocketDescriptor::setMulticastHops(SocketFlags::AddressFamily addressFamily, int hops)
{
    if (addressFamily == SocketFlags::AddressFamilyIPV4)
    {
        if (::setsockopt(handle, IPPROTO_IP, IP_MULTICAST_TTL, &hops, sizeof(hops)) != 0)
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed,
                                                 SocketErrorDetail::SetIPv4MulticastHops, static_cast<uint32_t>(errno));
        return {};
    }
    else
    {
        if (::setsockopt(handle, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof(hops)) != 0)
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed,
                                                 SocketErrorDetail::SetIPv6MulticastHops, static_cast<uint32_t>(errno));
        return {};
    }
}

ResultSocket SocketDescriptor::setMulticastOutboundInterface(const SocketIPAddress& interfaceAddress)
{
    if (interfaceAddress.getAddressFamily() == SocketFlags::AddressFamilyIPV4)
    {
        struct in_addr addr = interfaceAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        if (::setsockopt(handle, IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr)) != 0)
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed,
                                                 SocketErrorDetail::SetIPv4MulticastInterface,
                                                 static_cast<uint32_t>(errno));
        return {};
    }
    else
    {
        const unsigned int interfaceIndex = interfaceAddress.handle.reinterpret_as<struct sockaddr_in6>().sin6_scope_id;
        if (::setsockopt(handle, IPPROTO_IPV6, IPV6_MULTICAST_IF, &interfaceIndex, sizeof(interfaceIndex)) != 0)
            return ResultSocket::withNativeError(SocketError::SocketOptionFailed,
                                                 SocketErrorDetail::SetIPv6MulticastInterface,
                                                 static_cast<uint32_t>(errno));
        return {};
    }
}

ResultSocket SocketDescriptor::isInheritable(bool& hasValue) const
{
    bool cloexec = false;
    SC_TRY(hasFileDescriptorFlags<FD_CLOEXEC>(handle, cloexec));
    hasValue = !cloexec;
    return {};
}

ResultSocket SocketDescriptor::shutdown(SocketFlags::ShutdownType shutdownType)
{
    if (shutdownType != SocketFlags::ShutdownBoth)
        return {SocketError::InvalidShutdownType, SocketErrorDetail::ShutdownSocket};
    int how = 0;
    if (::shutdown(handle, how) != 0)
        return ResultSocket::withNativeError(SocketError::ShutdownFailed, SocketErrorDetail::ShutdownSocket,
                                             static_cast<uint32_t>(errno));
    return {};
}

ResultSocket SocketDescriptor::create(SocketFlags::AddressFamily addressFamily, SocketFlags::SocketType socketType,
                                      SocketFlags::ProtocolType protocol, SocketFlags::BlockingType blocking,
                                      SocketFlags::InheritableType inheritable)
{
    if (not SocketNetworking::isNetworkingInited())
        return {SocketError::NetworkingNotInitialized};
#if SC_PLATFORM_EMSCRIPTEN
    if (addressFamily == SocketFlags::AddressFamilyUnix)
        return {SocketError::OperationUnsupported};
#endif
    SC_SOCKET_TRUST_RESULT(close());

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
    if (handle == SocketDescriptor::Invalid)
        return ResultSocket::withNativeError(SocketError::SocketCreationFailed, SocketErrorDetail::CreateSocket,
                                             static_cast<uint32_t>(errno));
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
    return {};
}

ResultSocket SocketDescriptor::sendTo(Span<const char> data, const SocketAddress& destination)
{
    if (not destination.isValid())
        return {SocketError::InvalidAddress, SocketErrorDetail::SendDatagram};
    const socklen_t addressSize = static_cast<socklen_t>(destination.sizeOfHandle());
    ssize_t         sent;
    do
    {
        sent = ::sendto(handle, data.data(), data.sizeInBytes(), 0,
                        &destination.handle.reinterpret_as<const struct sockaddr>(), addressSize);
    } while (sent == -1 and errno == EINTR);
    if (sent == -1)
    {
        const uint32_t nativeError = static_cast<uint32_t>(errno);
        if (nativeError == EAGAIN or nativeError == EWOULDBLOCK)
            return {SocketError::WouldBlock, SocketErrorDetail::SendDatagram};
        return ResultSocket::withNativeError(SocketError::SendFailed, SocketErrorDetail::SendDatagram, nativeError);
    }
    if (static_cast<size_t>(sent) != data.sizeInBytes())
        return ResultSocket::withActualBytes(SocketError::IncompleteSend, SocketErrorDetail::SendDatagram,
                                             static_cast<uint32_t>(sent));
    return {};
}

ResultSocket SocketDescriptor::sendTo(Span<const char> data, const SocketIPAddress& destination)
{
    return sendTo(data, SocketAddress(destination));
}

ResultSocket SocketDescriptor::receiveFrom(Span<char> buffer, Span<char>& receivedData, SocketAddress& sourceAddress)
{
    SocketAddress receivedSourceAddress;
    struct iovec  receiveBuffer = {};
    receiveBuffer.iov_base      = buffer.data();
    receiveBuffer.iov_len       = buffer.sizeInBytes();

    struct msghdr message = {};
    message.msg_name      = &receivedSourceAddress.handle.reinterpret_as<struct sockaddr>();
    message.msg_iov       = &receiveBuffer;
    message.msg_iovlen    = 1;

    ssize_t received;
    do
    {
        message.msg_namelen = sizeof(receivedSourceAddress.handle);
        message.msg_flags   = 0;
        received            = ::recvmsg(handle, &message, 0);
    } while (received == -1 and errno == EINTR);
    if (received == -1)
    {
        const uint32_t nativeError = static_cast<uint32_t>(errno);
        if (nativeError == EAGAIN or nativeError == EWOULDBLOCK)
            return {SocketError::WouldBlock, SocketErrorDetail::ReceiveDatagram};
        return ResultSocket::withNativeError(SocketError::ReceiveFailed, SocketErrorDetail::ReceiveDatagram,
                                             nativeError);
    }
    if ((message.msg_flags & MSG_TRUNC) != 0)
        return {SocketError::DatagramTruncated, SocketErrorDetail::ReceiveDatagram};
    receivedSourceAddress.nativeSize = message.msg_namelen;
    receivedData                     = {buffer.data(), static_cast<size_t>(received)};
    sourceAddress                    = receivedSourceAddress;
    return {};
}

ResultSocket SocketDescriptor::receiveFrom(Span<char> buffer, Span<char>& receivedData, SocketIPAddress& sourceAddress)
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

void SocketNetworking::initNetworking() {}
void SocketNetworking::shutdownNetworking() {}
bool SocketNetworking::isNetworkingInited() { return true; }
} // namespace SC
