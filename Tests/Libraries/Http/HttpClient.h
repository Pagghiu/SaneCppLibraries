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
    /// @param keepConnectionOpen If true, keeps the connection open for subsequent requests
    /// @return Valid Result if dns resolution and creation of underlying client tcp socket succeeded
    Result get(AsyncEventLoop& loop, StringSpan url, bool keepConnectionOpen = false);

    /// @brief Setups this client to execute a `PUT` request on the given url with a body
    /// @param loop The AsyncEventLoop to use for monitoring network packets
    /// @param url The url to `PUT`
    /// @param body The body content to send
    /// @param bodyDelay Artificial time delay before sending body
    /// @return Valid Result if dns resolution and creation of underlying client tcp socket succeeded
    Result put(AsyncEventLoop& loop, StringSpan url, StringSpan body, TimeMs bodyDelay = {});

    /// @brief Setups this client to execute a multipart `POST` request with file upload
    /// @param loop The AsyncEventLoop to use for monitoring network packets
    /// @param url The url to `POST`
    /// @param fieldName Form field name
    /// @param fileName File name for the upload
    /// @param fileContent Content of the file to upload
    /// @param bodyDelay Artificial time delay before sending body
    /// @return Valid Result if dns resolution and creation of underlying client tcp socket succeeded
    Result postMultipart(AsyncEventLoop& loop, StringSpan url, StringSpan fieldName, StringSpan fileName,
                         StringSpan fileContent, TimeMs bodyDelay = {});

    Delegate<HttpClient&> callback; ///< The callback that is called after `GET` operation succeeded

    /// @brief Get the response StringSpan sent by the server
    [[nodiscard]] StringSpan getResponse() const;

  private:
    void startWaiting(AsyncSocketSend::Result& result);
    void startSendingHeaders(AsyncSocketConnect::Result& result);
    void startSendingHeadersOnExistingConnection();
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

    bool hasActiveConnection = false; ///< Whether we have an active connection that can be reused
    bool keepConnectionOpen  = false; ///< Whether to keep connections open for reuse
};
//! @}
} // namespace SC
