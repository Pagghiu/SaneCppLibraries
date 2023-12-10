// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../../Foundation/Deferred.h"
#include "../SocketDescriptor.h"

#include <errno.h>  // errno
#include <fcntl.h>  // fcntl
#include <netdb.h>  // AF_INET / IPPROTO_TCP / AF_UNSPEC
#include <unistd.h> // close
constexpr int SOCKET_ERROR = -1;

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
        setsockopt(handle, SOL_SOCKET, SO_NOSIGPIPE, &active, sizeof(active));
    }
#endif // defined(SO_NOSIGPIPE)
    return Result(isValid());
}
