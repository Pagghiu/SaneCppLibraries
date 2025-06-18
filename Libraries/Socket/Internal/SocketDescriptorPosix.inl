// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Socket/Socket.h"

#include "../../File/FileDescriptor.h" // TODO: Remove dependency of File library
#include "../../Foundation/Assert.h"
#include "../../Foundation/Deferred.h"

#include <arpa/inet.h> // inet_pton
#include <errno.h>     // errno
#include <fcntl.h>     // fcntl
#include <netdb.h>     // AF_INET / IPPROTO_TCP / AF_UNSPEC
#include <unistd.h>    // close

SC::Result SC::detail::SocketDescriptorDefinition::releaseHandle(Handle& handle)
{
    ::close(handle);
    handle = Invalid;
    return Result(true);
}

SC::Result SC::SocketDescriptor::setInheritable(bool inheritable)
{
    FileDescriptor fd;
    SC_TRUST_RESULT(fd.assign(handle));
    auto detach = MakeDeferred([&]() { fd.detach(); });
    return fd.setInheritable(inheritable);
}

SC::Result SC::SocketDescriptor::setBlocking(bool blocking)
{
    FileDescriptor fd;
    SC_TRUST_RESULT(fd.assign(handle));
    auto detach = MakeDeferred([&]() { fd.detach(); });
    return fd.setBlocking(blocking);
}

SC::Result SC::SocketDescriptor::isInheritable(bool& hasValue) const
{
    FileDescriptor fd;
    SC_TRUST_RESULT(fd.assign(handle));
    auto detach = MakeDeferred([&]() { fd.detach(); });
    return fd.isInheritable(hasValue);
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
