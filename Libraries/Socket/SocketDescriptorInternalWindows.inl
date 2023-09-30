// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include <WinSock2.h>
#include <Ws2tcpip.h> // sockadd_in6
using socklen_t = int;

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "../Foundation/Containers/Vector.h"
#include "SocketDescriptor.h"

SC::ReturnCode SC::SocketDescriptorTraits::releaseHandle(Handle& handle)
{
    const int res = ::closesocket(handle);
    handle        = SocketDescriptor::Invalid;
    return ReturnCode(res != -1);
}

SC::ReturnCode SC::SocketDescriptor::setInheritable(bool inheritable)
{
    if (::SetHandleInformation(reinterpret_cast<HANDLE>(handle), HANDLE_FLAG_INHERIT, inheritable ? TRUE : FALSE) ==
        FALSE)
    {
        "SetHandleInformation failed";
    }
    return ReturnCode(true);
}

SC::ReturnCode SC::SocketDescriptor::setBlocking(bool blocking)
{
    ULONG enable = blocking ? 0 : 1;
    if (::ioctlsocket(handle, FIONBIO, &enable) == SOCKET_ERROR)
    {
        return ReturnCode::Error("ioctlsocket failed");
    }
    return ReturnCode(true);
}

SC::ReturnCode SC::SocketDescriptor::isInheritable(bool& hasValue) const
{
    DWORD flags;
    if (::GetHandleInformation(reinterpret_cast<HANDLE>(handle), &flags) == FALSE)
    {
        return ReturnCode::Error("GetHandleInformation failed");
    }
    hasValue = (flags & HANDLE_FLAG_INHERIT) != 0;
    return ReturnCode(true);
}

SC::ReturnCode SC::SocketDescriptor::create(SocketFlags::AddressFamily addressFamily,
                                            SocketFlags::SocketType socketType, SocketFlags::ProtocolType protocol,
                                            SocketFlags::BlockingType    blocking,
                                            SocketFlags::InheritableType inheritable)
{
    SC_TRY(SystemFunctions::isNetworkingInited());
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
        return ReturnCode::Error("WSASocketW failed");
    }
    SC_TRY(setBlocking(blocking == SocketFlags::Blocking));
    return ReturnCode(isValid());
}
