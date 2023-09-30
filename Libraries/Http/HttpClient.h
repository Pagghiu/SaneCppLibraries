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
    [[nodiscard]] Result get(EventLoop& loop, StringView url);

    Delegate<HttpClient&> callback;

    [[nodiscard]] StringView getResponse() const;
    [[nodiscard]] Result     setCustomDebugName(const StringView debugName)
    {
        return Result(customDebugName.assign(debugName));
    }

  private:
    void onConnected(AsyncSocketConnect::Result& result);
    void onAfterSend(AsyncSocketSend::Result& result);
    void onAfterRead(AsyncSocketReceive::Result& result);

    SmallVector<char, 1024> content;

    String customDebugName;

    // TODO: can we find a way to putt all asyncs in a single tagged union when they're not used in parallel?
    AsyncSocketConnect connectAsync;
    AsyncSocketSend    sendAsync;
    AsyncSocketReceive receiveAsync;
    SocketDescriptor   clientSocket;
    EventLoop*         eventLoop = nullptr;
};
