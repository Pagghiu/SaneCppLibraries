// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Foundation/Result.h"
#include "../Foundation/StringView.h"
#include "../System/Time.h"
#include "SocketDescriptor.h"

namespace SC
{
struct NetworkSocket;
struct TCPClient;
struct TCPServer;
} // namespace SC

struct SC::NetworkSocket
{
    SocketDescriptorNativeHandle socket;
};

struct SC::TCPServer : public NetworkSocket
{
    [[nodiscard]] ReturnCode listen(StringView interfaceAddres, uint32_t port);
    [[nodiscard]] ReturnCode close();
    [[nodiscard]] ReturnCode accept(TCPClient& newClient);
};

struct SC::TCPClient : public NetworkSocket
{
    [[nodiscard]] ReturnCode connect(StringView address, uint32_t port);
    [[nodiscard]] ReturnCode close();
    [[nodiscard]] ReturnCode write(Span<const char> data);
    [[nodiscard]] ReturnCode read(Span<char> data);
    [[nodiscard]] ReturnCode readWithTimeout(Span<char> data, IntegerMilliseconds timeout);
};
