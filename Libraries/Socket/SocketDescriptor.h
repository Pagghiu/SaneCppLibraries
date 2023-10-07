// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../File/FileDescriptor.h"
#include "../Foundation/Language/Opaque.h"
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
    static Result           releaseHandle(Handle& handle);
};

#else

struct SC::SocketDescriptorTraits
{
    using Handle                    = int; // fd
    static constexpr Handle Invalid = -1;  // invalid fd
    static Result           releaseHandle(Handle& handle);
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

    [[nodiscard]] Result fromAddressPort(StringView interfaceAddress, uint16_t port);

    // TODO: maybe we should only save a binary address instead of native structs (IPV6 would need 16 bytes)
    uint32_t         sizeOfHandle() const;
    OpaqueHandle<28> handle;

  private:
    SocketFlags::AddressFamily addressFamily = SocketFlags::AddressFamilyIPV4;
};

struct SC::SocketDescriptor : public UniqueTaggedHandleTraits<SocketDescriptorTraits>
{
    [[nodiscard]] Result create(SocketFlags::AddressFamily   addressFamily,
                                SocketFlags::SocketType      socketType  = SocketFlags::SocketStream,
                                SocketFlags::ProtocolType    protocol    = SocketFlags::ProtocolTcp,
                                SocketFlags::BlockingType    blocking    = SocketFlags::Blocking,
                                SocketFlags::InheritableType inheritable = SocketFlags::NonInheritable);
    [[nodiscard]] Result isInheritable(bool& value) const;
    [[nodiscard]] Result setInheritable(bool value);
    [[nodiscard]] Result setBlocking(bool value);
    [[nodiscard]] Result getAddressFamily(SocketFlags::AddressFamily& addressFamily) const;
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

    [[nodiscard]] Result listen(SocketIPAddress nativeAddress, uint32_t numberOfWaitingConnections = 511);
    [[nodiscard]] Result close();
    [[nodiscard]] Result accept(SocketFlags::AddressFamily addressFamily, SocketDescriptor& newClient);

  private:
    SocketDescriptor& socket;
};

struct SC::SocketClient
{
    SocketClient(SocketDescriptor& socket) : socket(socket) {}

    [[nodiscard]] Result connect(StringView address, uint16_t port);
    [[nodiscard]] Result connect(SocketIPAddress ipAddress);
    [[nodiscard]] Result close();
    [[nodiscard]] Result write(Span<const char> data);
    [[nodiscard]] Result read(Span<char> data, Span<char>& readData);
    [[nodiscard]] Result readWithTimeout(Span<char> data, Span<char>& readData, IntegerMilliseconds timeout);

  private:
    SocketDescriptor& socket;
};

struct SC::DNSResolver
{
    [[nodiscard]] static Result resolve(StringView host, String& ipAddress);
};
