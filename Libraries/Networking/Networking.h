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
} // namespace SC

struct SC::TCPServer
{
    SocketDescriptor socket;

    [[nodiscard]] ReturnCode listen(StringView interfaceAddres, uint32_t port);
    [[nodiscard]] ReturnCode close();
    [[nodiscard]] ReturnCode accept(TCPClient& newClient);
};

struct SC::TCPClient
{
    SocketDescriptor socket;

    [[nodiscard]] ReturnCode connect(StringView address, uint32_t port);
    [[nodiscard]] ReturnCode close();
    [[nodiscard]] ReturnCode write(Span<const char> data);
    [[nodiscard]] ReturnCode read(Span<char> data);
    [[nodiscard]] ReturnCode readWithTimeout(Span<char> data, IntegerMilliseconds timeout);
};
