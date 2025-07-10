// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Socket/Socket.h"

#include "../../Foundation/Assert.h"
#include "../../Foundation/Deferred.h"

#include <arpa/inet.h> // inet_pton
#include <errno.h>     // errno
#include <fcntl.h>     // fcntl
#include <netdb.h>     // AF_INET / IPPROTO_TCP / AF_UNSPEC
#include <unistd.h>    // close

namespace
{
static SC::Result getFileFlags(int flagRead, const int fileDescriptor, int& outFlags)
{
    do
    {
        outFlags = ::fcntl(fileDescriptor, flagRead);
    } while (outFlags == -1 && errno == EINTR);
    if (outFlags == -1)
    {
        return SC::Result::Error("fcntl getFlag failed");
    }
    return SC::Result(true);
}
static SC::Result setFileFlags(int flagRead, int flagWrite, const int fileDescriptor, const bool setFlag,
                               const int flag)
{
    int oldFlags;
    do
    {
        oldFlags = ::fcntl(fileDescriptor, flagRead);
    } while (oldFlags == -1 && errno == EINTR);
    if (oldFlags == -1)
    {
        return SC::Result::Error("fcntl getFlag failed");
    }
    const int newFlags = setFlag ? oldFlags | flag : oldFlags & (~flag);
    if (newFlags != oldFlags)
    {
        int res;
        do
        {
            res = ::fcntl(fileDescriptor, flagWrite, newFlags);
        } while (res == -1 && errno == EINTR);
        if (res != 0)
        {
            return SC::Result::Error("fcntl setFlag failed");
        }
    }
    return SC::Result(true);
}
template <int flag>
static SC::Result hasFileDescriptorFlags(int fileDescriptor, bool& hasFlag)
{
    static_assert(flag == FD_CLOEXEC, "hasFileDescriptorFlags invalid value");
    int flags = 0;
    SC_TRY(getFileFlags(F_GETFD, fileDescriptor, flags));
    hasFlag = (flags & flag) != 0;
    return SC::Result(true);
}
template <int flag>
static SC::Result setFileDescriptorFlags(int fileDescriptor, bool setFlag)
{
    static_assert(flag == FD_CLOEXEC, "setFileDescriptorFlags invalid value");
    return setFileFlags(F_GETFD, F_SETFD, fileDescriptor, setFlag, flag);
}
template <int flag>
static SC::Result hasFileStatusFlags(int fileDescriptor, bool& hasFlag)
{
    static_assert(flag == O_NONBLOCK, "hasFileStatusFlags invalid value");
    int flags = 0;
    SC_TRY(getFileFlags(F_GETFL, fileDescriptor, flags));
    hasFlag = (flags & flag) != 0;
    return SC::Result(true);
}
template <int flag>
static SC::Result setFileStatusFlags(int fileDescriptor, bool setFlag)
{
    static_assert(flag == O_NONBLOCK, "setFileStatusFlags invalid value");
    return setFileFlags(F_GETFL, F_SETFL, fileDescriptor, setFlag, flag);
}
} // namespace

SC::Result SC::detail::SocketDescriptorDefinition::releaseHandle(Handle& handle)
{
    ::close(handle);
    handle = Invalid;
    return Result(true);
}

SC::Result SC::SocketDescriptor::setInheritable(bool inheritable)
{
    // On POSIX, inheritable = false means set FD_CLOEXEC
    return setFileDescriptorFlags<FD_CLOEXEC>(handle, !inheritable);
}

SC::Result SC::SocketDescriptor::setBlocking(bool blocking)
{
    // On POSIX, blocking = false means set O_NONBLOCK
    return setFileStatusFlags<O_NONBLOCK>(handle, !blocking);
}

SC::Result SC::SocketDescriptor::isInheritable(bool& hasValue) const
{
    bool cloexec = false;
    SC_TRY(hasFileDescriptorFlags<FD_CLOEXEC>(handle, cloexec));
    hasValue = !cloexec;
    return SC::Result(true);
}

SC::Result SC::SocketDescriptor::shutdown(SocketFlags::ShutdownType shutdownType)
{
    int how = 0;
    switch (shutdownType)
    {
    case SocketFlags::ShutdownRead: how = SHUT_RD; break;
    case SocketFlags::ShutdownWrite: how = SHUT_WR; break;
    case SocketFlags::ShutdownBoth: how = SHUT_RDWR; break;
    default: return Result::Error("Invalid shutdown type");
    }
    if (::shutdown(handle, how) == 0)
    {
        return Result(true);
    }
    const int err = errno;
    switch (err)
    {
    case ENOTCONN: return Result::Error("Socket is not connected");
    case ESHUTDOWN: return Result::Error("Socket is already shutdown");
    case EINVAL: return Result::Error("Invalid shutdown type");
    case ENOTSOCK: return Result::Error("Socket is not a socket");
    default: return Result::Error("Failed to shutdown socket");
    }
}

SC::Result SC::SocketDescriptor::create(SocketFlags::AddressFamily addressFamily, SocketFlags::SocketType socketType,
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

SC::Result SC::SocketNetworking::initNetworking() { return Result(true); }
SC::Result SC::SocketNetworking::shutdownNetworking() { return Result(true); }
bool       SC::SocketNetworking::isNetworkingInited() { return Result(true); }
