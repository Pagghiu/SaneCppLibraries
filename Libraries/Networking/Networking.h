// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Foundation/Result.h"
#include "../Foundation/StringView.h"
#include "../System/Time.h"
#include "../Threading/Atomic.h"
#include "../Threading/Threading.h"
#include "SocketDescriptor.h"

namespace SC
{
struct Network;
struct TCPClient;
struct TCPServer;
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

struct SC::TCPServer
{
    SocketDescriptorNativeHandle socket;

    [[nodiscard]] ReturnCode listen(StringView interfaceAddres, uint32_t port);
    [[nodiscard]] ReturnCode close();
    [[nodiscard]] ReturnCode accept(TCPClient& newClient);
};

struct SC::TCPClient
{
    SocketDescriptorNativeHandle socket;

    [[nodiscard]] ReturnCode connect(StringView address, uint32_t port);
    [[nodiscard]] ReturnCode close();
    [[nodiscard]] ReturnCode write(Span<const char> data);
    [[nodiscard]] ReturnCode read(Span<char> data);
    [[nodiscard]] ReturnCode readWithTimeout(Span<char> data, IntegerMilliseconds timeout);
};
