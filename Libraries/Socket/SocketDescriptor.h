// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../File/FileDescriptor.h"
#include "../Foundation/Language/Opaque.h"
#include "../Foundation/Language/Optional.h"
#include "../Foundation/Language/Result.h"
#include "../System/Time.h" // IntegerMilliseconds

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
struct SocketIPAddress;
} // namespace SC

#if SC_PLATFORM_WINDOWS

struct SC::SocketDescriptorTraits
{
    using Handle                    = size_t;                  // SOCKET
    static constexpr Handle Invalid = ~static_cast<Handle>(0); // INVALID_SOCKET
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

struct SC::SocketIPAddress
{
    SocketIPAddress(SocketFlags::AddressFamily addressFamily = SocketFlags::AddressFamilyIPV4)
        : addressFamily(addressFamily)
    {}

    [[nodiscard]] SocketFlags::AddressFamily getAddressFamily() { return addressFamily; }

    [[nodiscard]] ReturnCode fromAddressPort(StringView interfaceAddress, uint16_t port);

    // TODO: maybe we should only save a binary address instead of native structs (IPV6 would need 16 bytes)
    uint32_t         sizeOfHandle() const;
    OpaqueHandle<28> handle;

  private:
    SocketFlags::AddressFamily addressFamily = SocketFlags::AddressFamilyIPV4;
};

struct SC::SocketDescriptor : public UniqueTaggedHandleTraits<SocketDescriptorTraits>
{
    [[nodiscard]] ReturnCode create(SocketFlags::AddressFamily   addressFamily,
                                    SocketFlags::SocketType      socketType  = SocketFlags::SocketStream,
                                    SocketFlags::ProtocolType    protocol    = SocketFlags::ProtocolTcp,
                                    SocketFlags::BlockingType    blocking    = SocketFlags::Blocking,
                                    SocketFlags::InheritableType inheritable = SocketFlags::NonInheritable);
    [[nodiscard]] ReturnCode isInheritable(bool& value) const;
    [[nodiscard]] ReturnCode setInheritable(bool value);
    [[nodiscard]] ReturnCode setBlocking(bool value);
    [[nodiscard]] ReturnCode getAddressFamily(SocketFlags::AddressFamily& addressFamily) const;
};

namespace SC
{
struct SocketClient;
struct SocketServer;
struct DNSResolver;
} // namespace SC

struct SC::SocketServer
{
    SocketServer(SocketDescriptor& socket) : socket(socket) {}

    [[nodiscard]] ReturnCode listen(SocketIPAddress nativeAddress, uint32_t numberOfWaitingConnections = 511);
    [[nodiscard]] ReturnCode close();
    [[nodiscard]] ReturnCode accept(SocketFlags::AddressFamily addressFamily, SocketDescriptor& newClient);

  private:
    SocketDescriptor& socket;
};

struct SC::SocketClient
{
    SocketClient(SocketDescriptor& socket) : socket(socket) {}

    [[nodiscard]] ReturnCode connect(StringView address, uint16_t port);
    [[nodiscard]] ReturnCode connect(SocketIPAddress ipAddress);
    [[nodiscard]] ReturnCode close();
    [[nodiscard]] ReturnCode write(Span<const char> data);
    [[nodiscard]] ReturnCode read(Span<char> data, Span<char>& readData);
    [[nodiscard]] ReturnCode readWithTimeout(Span<char> data, Span<char>& readData, IntegerMilliseconds timeout);

  private:
    SocketDescriptor& socket;
};

struct SC::DNSResolver
{
    [[nodiscard]] static ReturnCode resolve(StringView host, String& ipAddress);
};
