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

SC::Result SC::SocketNetworking::initNetworking()
{
    if (isNetworkingInited() == false)
    {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        {
            return Result::Error("WSAStartup failed");
        }
#if SC_COMPILER_MSVC
        InterlockedExchange(&Internal::get().networkingInited, 1);
#elif SC_COMPILER_CLANG
        atomic_store(&Internal::get().networkingInited, true);
#elif SC_COMPILER_GCC
        Internal::get().store(true);
#endif
    }
    return Result(true);
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
