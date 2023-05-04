// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Opaque.h"
#include "../Foundation/Optional.h"
#include "../Foundation/Result.h"
#include "../System/Descriptors.h"

namespace SC
{
struct String;
template <typename T>
struct Vector;
} // namespace SC

namespace SC
{
struct SocketDescriptor;
struct SocketDescriptorTraits;
struct SocketFlags;
} // namespace SC

#if SC_PLATFORM_WINDOWS

struct SC::SocketDescriptorTraits
{
    using Handle                    = uint64_t; // SOCKET
    static constexpr Handle Invalid = ~0ull;    // INVALID_SOCKET
    static ReturnCode       releaseHandle(Handle& handle);
};

#else

struct SC::SocketDescriptorTraits
{
    using Handle                    = int; // fd
    static constexpr Handle Invalid = -1;  // invalid fd
    static ReturnCode       releaseHandle(Handle& handle);
};

#endif

struct SC::SocketFlags
{
    enum AddressFamily
    {
        AddressFamilyIPV4,
        AddressFamilyIPV6,
    };
    [[nodiscard]] static AddressFamily AddressFamilyFromInt(int value);
    [[nodiscard]] static unsigned char toNative(AddressFamily family);

    enum SocketType
    {
        SocketStream,
        SocketDgram
    };
    [[nodiscard]] static SocketType SocketTypeFromInt(int value);
    [[nodiscard]] static int        toNative(SocketType family);

    enum ProtocolType
    {
        ProtocolTcp,
        ProtocolUdp,
    };
    [[nodiscard]] static ProtocolType ProtocolTypeFromInt(int value);
    [[nodiscard]] static int          toNative(ProtocolType family);
};

struct SC::SocketDescriptor : public UniqueTaggedHandleTraits<SocketDescriptorTraits>
{
    [[nodiscard]] ReturnCode create(SocketFlags::AddressFamily       addressFamily,
                                    SocketFlags::SocketType          socketType  = SocketFlags::SocketStream,
                                    SocketFlags::ProtocolType        protocol    = SocketFlags::ProtocolTcp,
                                    DescriptorFlags::BlockingType    blocking    = DescriptorFlags::Blocking,
                                    DescriptorFlags::InheritableType inheritable = DescriptorFlags::NonInheritable);
    [[nodiscard]] ReturnCode isInheritable(bool& value) const;
    [[nodiscard]] ReturnCode setInheritable(bool value);
    [[nodiscard]] ReturnCode setBlocking(bool value);
    [[nodiscard]] ReturnCode getAddressFamily(SocketFlags::AddressFamily& addressFamily) const;
};
