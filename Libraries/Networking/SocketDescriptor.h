// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Foundation/Opaque.h"
#include "../Threading/Threading.h"

namespace SC
{
#if SC_PLATFORM_WINDOWS
using SocketDescriptorNative                                          = uint64_t; // SOCKET
static constexpr SocketDescriptorNative SocketDescriptorNativeInvalid = ~0ull;    // INVALID_SOCKET
#else
using SocketDescriptorNative                                          = int; // Posix FD
static constexpr SocketDescriptorNative SocketDescriptorNativeInvalid = -1;  // Posix Invalid FD
#endif
ReturnCode SocketDescriptorNativeClose(SocketDescriptorNative&);
struct SocketDescriptorNativeHandle : public UniqueTaggedHandle<SocketDescriptorNative, SocketDescriptorNativeInvalid,
                                                                ReturnCode, SocketDescriptorNativeClose>
{
    enum BlockingType
    {
        NonBlocking,
        Blocking
    };
    enum InheritableType
    {
        NonInheritable,
        Inheritable
    };
    enum IPType
    {
        IPTypeV4,
        IPTypeV6,
    };
    enum Protocol
    {
        ProtocolTcp,
    };
    [[nodiscard]] ReturnCode create(IPType ipType, Protocol protocol, BlockingType blocking = NonBlocking,
                                    InheritableType inheritable = NonInheritable);
    [[nodiscard]] ReturnCode isInheritable(bool& value) const;
    [[nodiscard]] ReturnCode setInheritable(bool value);
    [[nodiscard]] ReturnCode setBlocking(bool value);
};
struct Network;
} // namespace SC
struct SC::Network
{
    [[nodiscard]] static ReturnCode init();
    [[nodiscard]] static ReturnCode shutdown();
#if SC_PLATFORM_WINDOWS
  private:
    static bool  inited;
    static Mutex mutex;
#endif
};
