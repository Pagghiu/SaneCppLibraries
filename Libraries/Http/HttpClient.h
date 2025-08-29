// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Async/Async.h"
#include "../Containers/Vector.h"
#include "../Socket/Socket.h"
#include "../Strings/String.h"
namespace SC
{
/// @brief HTTP parser, client and server (see @ref library_http)
struct HttpClient;
} // namespace SC

//! @defgroup group_http Http
//! @copybrief library_http (see @ref library_http for more details)

//! @addtogroup group_http
//! @{

/// @brief Http async client
struct SC::HttpClient
{
    /// @brief Setups this client to execute a `GET` request on the given url
    /// @param loop The AsyncEventLoop to use for monitoring network packets
    /// @param url The url to `GET`
    /// @return Valid Result if dns resolution and creation of underlying client tcp socket succeeded
    Result get(AsyncEventLoop& loop, StringView url);

    Delegate<HttpClient&> callback; ///< The callback that is called after `GET` operation succeeded

    /// @brief Get the response StringView sent by the server
    [[nodiscard]] StringView getResponse() const;

    Result setCustomDebugName(const StringView debugName) { return Result(customDebugName.assign(debugName)); }

  private:
    void onConnected(AsyncSocketConnect::Result& result);
    void onAfterSend(AsyncSocketSend::Result& result);
    void onAfterRead(AsyncSocketReceive::Result& result);

    SmallBuffer<1024> content;

    String customDebugName;

    AsyncSocketConnect connectAsync;
    AsyncSocketSend    sendAsync;
    AsyncSocketReceive receiveAsync;
    SocketDescriptor   clientSocket;
    AsyncEventLoop*    eventLoop = nullptr;
};
//! @}
