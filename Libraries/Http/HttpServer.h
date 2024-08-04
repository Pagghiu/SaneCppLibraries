// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "HttpParser.h"

#include "../Foundation/Function.h"
#include "../Strings/String.h" // Contains Vector<char> export

namespace SC
{
struct SC_COMPILER_EXPORT HttpServer;
struct SC_COMPILER_EXPORT HttpHeaderOffset;
struct SC_COMPILER_EXPORT HttpRequest;
struct SC_COMPILER_EXPORT HttpResponse;
struct SC_COMPILER_EXPORT HttpClientChannel;

struct AsyncEventLoop;
} // namespace SC

//! @addtogroup group_http
//! @{

/// @brief Http header
struct SC::HttpHeaderOffset
{
    HttpParser::Result result = HttpParser::Result::Method;

    uint32_t start  = 0;
    uint32_t length = 0;
};

namespace SC
{
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT Vector<HttpHeaderOffset>;
}

/// @brief Http request holding parsed url and headers
struct SC::HttpRequest
{
    bool headersEndReceived = false; ///< All headers have been received
    bool parsedSuccessfully = true;  ///< Request headers have been parsed successfully

    HttpParser parser; ///< The parser used to parse headers
    StringView url;    ///< The url extracted from parsed headers

    Vector<char>             headerBuffer;  ///< Buffer containing all headers
    Vector<HttpHeaderOffset> headerOffsets; ///< Headers, defined as offsets in headerBuffer

    /// @brief Finds a specific HttpParser::Result in the list of parsed header
    /// @param result The result to look for (Method, Url etc.)
    /// @param res A StringView, pointing at headerBuffer containing the found result
    /// @return `true` if the result has been found
    [[nodiscard]] bool find(HttpParser::Result result, StringView& res) const;
};

/// @brief Http request holding output buffer to flush back to client
struct SC::HttpResponse
{
    Vector<char> outputBuffer;

    bool   responseEnded = false;
    size_t highwaterMark = 255;

    [[nodiscard]] Result startResponse(int code);
    [[nodiscard]] Result addHeader(StringView headerName, StringView headerValue);
    [[nodiscard]] Result end(Span<const char> span);

    [[nodiscard]] bool mustBeFlushed() const { return responseEnded or outputBuffer.size() > highwaterMark; }
};

struct SC::HttpClientChannel
{
    HttpRequest  request;
    HttpResponse response;

    [[nodiscard]] Result parse(const uint32_t maxHeaderSize, Span<const char> readData);
};

namespace SC
{
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT Function<void(HttpClientChannel&)>;
}

/// @brief Async Http server
///
/// Use the SC::HttpServer::onClient callback to intercept new clients connecting and
/// return http responses (or pass them to SC::HttpWebServer to statically serve files).
/// @see SC::HttpWebServer
///
/// \snippet Libraries/Http/Tests/HttpServerTest.cpp HttpServerSnippet
struct SC::HttpServer
{
    HttpServer();
    ~HttpServer();
    HttpServer(const HttpServer&)            = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    HttpServer(HttpServer&&)                 = delete;
    HttpServer& operator=(HttpServer&&)      = delete;

    /// @brief Starts the http server on the given AsyncEventLoop, address and port
    /// @param loop The event loop to be used, where to add the listening socket
    /// @param maxConcurrentRequests Maximum number of concurrent requestsx
    /// @param address The address of local interface where to listen to
    /// @param port The local port where to start listening to
    /// @return Valid Result if http listening has been started successfully
    [[nodiscard]] Result start(AsyncEventLoop& loop, uint32_t maxConcurrentRequests, StringView address, uint16_t port);

    /// @brief Stops the http server asyncronously (without waiting)
    [[nodiscard]] Result stopAsync();

    /// @brief Stops the http server synchronously (waiting for all sockets to shutdown)
    [[nodiscard]] Result stopSync();

    [[nodiscard]] bool isStarted() const { return started; }

    uint32_t maxHeaderSize;

    Function<void(HttpClientChannel&)> onClient;

  private:
    struct ClientSocket;
    struct Internal;
#if SC_PLATFORM_WINDOWS
    uint64_t internalRaw[72];
#else
    uint64_t internalRaw[32];
#endif
    Internal& getInternal();

    bool started;
    bool stopping;

    AsyncEventLoop* eventLoop;
};

//! @}
