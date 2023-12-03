// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Async/Async.h"
#include "../Containers/SmallVector.h"
#include "../Strings/String.h"
namespace SC
{
struct HttpClient;
} // namespace SC

struct SC::HttpClient
{
    [[nodiscard]] Result get(Async::EventLoop& loop, StringView url);

    Delegate<HttpClient&> callback;

    [[nodiscard]] StringView getResponse() const;
    [[nodiscard]] Result     setCustomDebugName(const StringView debugName)
    {
        return Result(customDebugName.assign(debugName));
    }

  private:
    void onConnected(Async::SocketConnect::Result& result);
    void onAfterSend(Async::SocketSend::Result& result);
    void onAfterRead(Async::SocketReceive::Result& result);

    SmallVector<char, 1024> content;

    String customDebugName;

    // TODO: can we find a way to putt all asyncs in a single tagged union when they're not used in parallel?
    Async::SocketConnect connectAsync;
    Async::SocketSend    sendAsync;
    Async::SocketReceive receiveAsync;
    SocketDescriptor     clientSocket;
    Async::EventLoop*    eventLoop = nullptr;
};
