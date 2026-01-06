// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Libraries/Async/Async.h"
#include "Libraries/Foundation/StringSpan.h"
#include "Libraries/Http/HttpParser.h"
#include "Libraries/Memory/Buffer.h"
#include "Libraries/Socket/Socket.h"
namespace SC
{
//! @defgroup group_http Http
//! @copybrief library_http (see @ref library_http for more details)

//! @addtogroup group_http
//! @{

/// @brief Http async client
struct HttpClient
{
    /// @brief Setups this client to execute a `GET` request on the given url
    /// @param loop The AsyncEventLoop to use for monitoring network packets
    /// @param url The url to `GET`
    /// @return Valid Result if dns resolution and creation of underlying client tcp socket succeeded
    Result get(AsyncEventLoop& loop, StringSpan url);

    /// @brief Setups this client to execute a `PUT` request on the given url with a body
    /// @param loop The AsyncEventLoop to use for monitoring network packets
    /// @param url The url to `PUT`
    /// @param body The body content to send
    /// @param bodyDelay Artificial time delay before sending body
    /// @return Valid Result if dns resolution and creation of underlying client tcp socket succeeded
    Result put(AsyncEventLoop& loop, StringSpan url, StringSpan body, TimeMs bodyDelay = {});

    Delegate<HttpClient&> callback; ///< The callback that is called after `GET` operation succeeded

    /// @brief Get the response StringSpan sent by the server
    [[nodiscard]] StringSpan getResponse() const;

  private:
    void startWaiting(AsyncSocketSend::Result& result);
    void startSendingHeaders(AsyncSocketConnect::Result& result);
    void startSendingBody(AsyncLoopTimeout::Result& result);
    void startReceiveResponse(AsyncSocketSend::Result& result);
    void tryParseResponse(AsyncSocketReceive::Result& result);

    HttpParser parser;
    Buffer     content;

    bool headersReceived = false;

    TimeMs bodyDelay;
    size_t headerBytes   = 0;
    size_t receivedBytes = 0;
    size_t parsedBytes   = 0;
    size_t contentLen    = 0;

    AsyncSocketConnect connectAsync;
    AsyncSocketSend    sendAsync;
    AsyncSocketReceive receiveAsync;
    AsyncLoopTimeout   timeoutAsync;
    SocketDescriptor   clientSocket;
    AsyncEventLoop*    eventLoop = nullptr;
};
//! @}
} // namespace SC
