// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "HttpParser.h"

#include "../Containers/ArenaMapKey.h"
#include "../Foundation/Function.h"
#include "../Strings/SmallString.h" // Contains Vector<char> export

namespace SC
{
struct SC_COMPILER_EXPORT HttpServer;
struct SC_COMPILER_EXPORT HttpRequest;
struct SC_COMPILER_EXPORT HttpResponse;
struct SC_COMPILER_EXPORT HttpRequest;

struct AsyncEventLoop;
struct SocketDescriptor;
struct HttpServerClient;
} // namespace SC

//! @addtogroup group_http
//! @{

namespace SC
{
namespace detail
{
struct SC_COMPILER_EXPORT HttpHeaderOffset
{
    HttpParser::Result result = HttpParser::Result::Method;

    uint32_t start  = 0;
    uint32_t length = 0;
};
} // namespace detail

#if !DOXYGEN
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT Vector<detail::HttpHeaderOffset>;
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT ArenaMapKey<HttpServerClient>;
#endif
} // namespace SC

/// @brief Http request received from a client
struct SC::HttpRequest
{
    /// @brief Finds a specific HttpParser::Result in the list of parsed header
    /// @param result The result to look for (Method, Url etc.)
    /// @param res A StringView, pointing at headerBuffer containing the found result
    /// @return `true` if the result has been found
    [[nodiscard]] bool find(HttpParser::Result result, StringView& res) const;

    /// @brief Gets the associated HttpParser
    const HttpParser& getParser() const { return parser; }

    /// @brief Gets the request URL
    StringView getURL() const { return url; }

  private:
    friend struct HttpServer;
    using HttpHeaderOffset = detail::HttpHeaderOffset; // TODO: hide class implementation

    bool headersEndReceived = false; ///< All headers have been received
    bool parsedSuccessfully = true;  ///< Request headers have been parsed successfully

    HttpParser   parser;       ///< The parser used to parse headers
    StringView   url;          ///< The url extracted from parsed headers
    Vector<char> headerBuffer; ///< Buffer containing all headers

    Vector<HttpHeaderOffset> headerOffsets; ///< Headers, defined as offsets in headerBuffer
};

/// @brief Http response that will be sent to a client
struct SC::HttpResponse
{
    ArenaMapKey<HttpServerClient> getClientKey() const { return key; }

    /// @brief Starts the response with a http standard code (200 OK, 404 NOT FOUND etc.)
    [[nodiscard]] Result startResponse(int httpCode);

    /// @brief Writes an http header to this response
    [[nodiscard]] Result addHeader(StringView headerName, StringView headerValue);

    /// @brief Appends some data to the response
    [[nodiscard]] Result write(Span<const char> data);

    /// @brief Finalizes response appending some data
    /// @warning The SC::HttpResponse / SC::HttpRequest pair will be invalidated on next SC::AsyncEventLoop run
    [[nodiscard]] Result end(Span<const char> data);

    HttpServer& getServer() { return *server; }

  private:
    friend struct HttpServer;
    [[nodiscard]] bool mustBeFlushed() const { return responseEnded or outputBuffer.size() > highwaterMark; }

    HttpServer* server = nullptr;

    ArenaMapKey<HttpServerClient> key;

    Vector<char> outputBuffer;

    bool   responseEnded = false;
    size_t highwaterMark = 1024;
};

namespace SC
{
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT Function<void(HttpRequest&, HttpResponse&)>;
}

/// @brief Async Http server
///
/// Usage:
/// - Use the SC::HttpServer::onRequest callback to intercept new clients connecting
/// - Write to SC::HttpResponse or use SC::HttpWebServer to statically serve files
///
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
    /// @param maxConcurrentRequests Maximum number of concurrent requests
    /// @param address The address of local interface where to listen to
    /// @param port The local port where to start listening to
    /// @return Valid Result if http listening has been started successfully
    [[nodiscard]] Result start(AsyncEventLoop& loop, uint32_t maxConcurrentRequests, StringView address, uint16_t port);

    /// @brief Stops http server asyncronously pushing cancel and close requests for next SC::AsyncEventLoop::runOnce
    [[nodiscard]] Result stopAsync();

    /// @brief Stops http server synchronously waiting for SC::AsyncEventLoop::runNoWait to cancel or close all requests
    [[nodiscard]] Result stopSync();

    /// @brief Check if the server is started
    [[nodiscard]] bool isStarted() const;

    /// @brief Called after enough data from a newly connected client has arrived, causing all headers to be parsed.
    /// @warning Both references can be invalidated in later stages of the http request lifetime.
    /// If necessary, store the client key returned by SC::HttpResponse::getClientKey and use it with
    /// SC::HttpServer::getRequest, SC::HttpServer::getResponse or SC::HttpServer::getSocket
    Function<void(HttpRequest&, HttpResponse&)> onRequest;

    /// @brief Obtain client request (or a nullptr if it doesn't exists) with the key returned by
    /// SC::HttpResponse::getClientKey
    [[nodiscard]] HttpRequest* getRequest(ArenaMapKey<HttpServerClient> key) const;

    /// @brief Obtain client response (or a nullptr if it doesn't exists) with the key returned by
    /// SC::HttpResponse::getClientKey
    [[nodiscard]] HttpResponse* getResponse(ArenaMapKey<HttpServerClient> key) const;

    /// @brief Obtain client socket (or a nullptr if it doesn't exists) with the key returned by
    /// SC::HttpResponse::getClientKey
    [[nodiscard]] SocketDescriptor* getSocket(ArenaMapKey<HttpServerClient> key) const;

    /// @brief Return maximum number of concurrent requests, corresponding to size of clients arena
    [[nodiscard]] uint32_t getMaxConcurrentRequests() const;

  private:
    struct Internal;
#if SC_PLATFORM_WINDOWS
    uint64_t internalRaw[72];
#else
    uint64_t internalRaw[32];
#endif
    Internal& internal;
};

//! @}
