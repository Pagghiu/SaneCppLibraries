// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Async/EventLoop.h"
#include "../Foundation/Containers/SmallVector.h"
#include "../Foundation/Strings/String.h"
namespace SC
{
struct HttpClient;
} // namespace SC

struct SC::HttpClient
{
    [[nodiscard]] ReturnCode start(EventLoop& loop, StringView ipAddress, uint16_t port, StringView requestContent,
                                   Function<void(HttpClient&)>&& cb);

    StringView getResponse() const;
    ReturnCode setCustomDebugName(const StringView debugName) { return customDebugName.assign(debugName); }

  private:
    void onConnected(AsyncSocketConnect::Result& result);
    void onAfterSend(AsyncSocketSend::Result& result);
    void onAfterRead(AsyncSocketReceive::Result& result);

    Function<void(HttpClient&)> callback;
    SmallVector<char, 1024>     content;

    String customDebugName;

    // TODO: can we find a way to putt all asyncs in a single tagged union when they're not used in parallel?
    AsyncSocketConnect connectAsync;
    AsyncSocketSend    sendAsync;
    AsyncSocketReceive receiveAsync;
    SocketDescriptor   clientSocket;
    EventLoop*         eventLoop = nullptr;
};
