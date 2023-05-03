// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Foundation/Result.h"
#include "../Foundation/StringView.h"
#include "../System/Descriptors.h"
#include "../System/Time.h"

namespace SC
{
struct TCPClient;
struct TCPServer;
struct NativeIPAddress;
} // namespace SC

struct SC::NativeIPAddress
{
    NativeIPAddress(Descriptor::AddressFamily addressFamily = Descriptor::AddressFamilyIPV4)
        : addressFamily(addressFamily)
    {}

    [[nodiscard]] Descriptor::AddressFamily getAddressFamily() { return addressFamily; }

    [[nodiscard]] ReturnCode fromAddressPort(StringView interfaceAddress, uint16_t port);

  private:
    friend struct TCPClient;
    friend struct TCPServer;
    Descriptor::AddressFamily addressFamily = Descriptor::AddressFamilyIPV4;

    OpaqueHandle<28> handle;
    uint32_t         sizeOfHandle() const;
};

struct SC::TCPServer
{
    SocketDescriptor         socket;
    [[nodiscard]] ReturnCode listen(StringView interfaceAddress, uint16_t port,
                                    uint32_t numberOfWaitingConnections = 1);
    [[nodiscard]] ReturnCode close();
    [[nodiscard]] ReturnCode accept(Descriptor::AddressFamily addressFamily, TCPClient& newClient);
};

struct SC::TCPClient
{
    SocketDescriptor         socket;
    [[nodiscard]] ReturnCode connect(StringView address, uint16_t port);
    [[nodiscard]] ReturnCode close();
    [[nodiscard]] ReturnCode write(Span<const char> data);
    [[nodiscard]] ReturnCode read(Span<char> data);
    [[nodiscard]] ReturnCode readWithTimeout(Span<char> data, IntegerMilliseconds timeout);
};
