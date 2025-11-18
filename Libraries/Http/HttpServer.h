// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "HttpParser.h"

#include "../Async/Async.h"
#include "../Foundation/Function.h"
#include "../Foundation/StringSpan.h"
#include "../Memory/Buffer.h"

namespace SC
{
struct SC_COMPILER_EXPORT HttpServer;
struct SC_COMPILER_EXPORT HttpServerClient;
namespace detail
{
struct SC_COMPILER_EXPORT HttpHeaderOffset
{
    HttpParser::Token token = HttpParser::Token::Method;

    uint32_t start  = 0;
    uint32_t length = 0;
};
} // namespace detail

//! @addtogroup group_http
//! @{

/// @brief Http request received from a client
struct SC_COMPILER_EXPORT HttpRequest
{
    /// @brief Finds a specific HttpParser::Result in the list of parsed header
    /// @param token The result to look for (Method, Url etc.)
    /// @param res A StringSpan, pointing at headerBuffer containing the found result
    /// @return `true` if the result has been found
    [[nodiscard]] bool find(HttpParser::Token token, StringSpan& res) const;

    /// @brief Gets the associated HttpParser
    const HttpParser& getParser() const { return parser; }

    /// @brief Gets the request URL
    StringSpan getURL() const { return url; }

    void reset();

    Result parse(const uint32_t maxHeaderSize, Span<const char> readData);

  private:
    friend struct HttpServer;
    using HttpHeaderOffset = detail::HttpHeaderOffset; // TODO: hide class implementation

    bool headersEndReceived = false; ///< All headers have been received
    bool parsedSuccessfully = true;  ///< Request headers have been parsed successfully

    HttpParser parser; ///< The parser used to parse headers
    StringSpan url;    ///< The url extracted from parsed headers

    Span<char> readHeaders;
    Span<char> availableHeader; ///< Space to save headers to

    static constexpr size_t MaxNumHeaders = 64;
    HttpHeaderOffset        headerOffsets[MaxNumHeaders]; ///< Headers, defined as offsets in headerBuffer
    size_t                  numHeaders = 0;
};

/// @brief Http response that will be sent to a client
struct SC_COMPILER_EXPORT HttpResponse
{
    /// @brief Starts the response with a http standard code (200 OK, 404 NOT FOUND etc.)
    Result startResponse(int httpCode);

    /// @brief Writes an http header to this response
    Result addHeader(StringSpan headerName, StringSpan headerValue);

    /// @brief Appends some data to the response
    Result write(Span<const char> data);

    /// @brief Finalizes response appending some data
    /// @warning The SC::HttpResponse / SC::HttpRequest pair will be invalidated on next SC::AsyncEventLoop run
    Result end(Span<const char> data);
    Result end();

    void reset()
    {
        responseEnded = false;
        outputBuffer.clear();
    }

  private:
    friend struct HttpServer;
    [[nodiscard]] bool mustBeFlushed() const { return responseEnded or outputBuffer.size() > highwaterMark; }

    HttpServer* server = nullptr;

    size_t key;

    Buffer outputBuffer;

    bool   responseEnded = false;
    size_t highwaterMark = 1024;
};

struct SC_COMPILER_EXPORT HttpServerClient
{
    enum class State
    {
        Free,
        Used
    };
    State state = State::Free;

    HttpRequest  request;
    HttpResponse response;

    void setFree()
    {
        request.reset();
        response.reset();
        state = State::Free;
    }

    SocketDescriptor   socket;
    char               debugName[16] = {0};
    AsyncSocketReceive asyncReceive;
    AsyncSocketSend    asyncSend;
};
#if !DOXYGEN
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT Function<void(HttpRequest&, HttpResponse&)>;
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT Span<HttpServerClient>;
#endif
/// @brief Async Http server
///
/// Usage:
/// - Use the SC::HttpServer::onRequest callback to intercept new clients connecting
/// - Write to SC::HttpResponse or use SC::HttpWebServer to statically serve files
///
/// @see SC::HttpWebServer
///
/// \snippet Tests/Libraries/Http/HttpServerTest.cpp HttpServerSnippet
struct SC_COMPILER_EXPORT HttpServer
{
    struct Memory
    {
        IGrowableBuffer& headersMemory;

        Span<HttpServerClient> clients;
    };
    /// @brief Starts the http server on the given AsyncEventLoop, address and port
    /// @param loop The event loop to be used, where to add the listening socket
    /// @param address The address of local interface where to listen to
    /// @param port The local port where to start listening to
    /// @param memory Memory buffers to be used by the http server
    /// @return Valid Result if http listening has been started successfully
    Result start(AsyncEventLoop& loop, StringSpan address, uint16_t port, Memory& memory);

    /// @brief Stops http server asynchronously pushing cancel and close requests for next SC::AsyncEventLoop::runOnce
    Result stopAsync();

    /// @brief Stops http server synchronously waiting for SC::AsyncEventLoop::runNoWait to cancel or close all requests
    Result stopSync();

    /// @brief Check if the server is started
    [[nodiscard]] bool isStarted() const { return started; }

    /// @brief Called after enough data from a newly connected client has arrived, causing all headers to be parsed.
    /// @warning Both references can be invalidated in later stages of the http request lifetime.
    /// If necessary, store the client key returned by SC::HttpResponse::getClientKey and use it with
    /// SC::HttpServer::getRequest, SC::HttpServer::getResponse or SC::HttpServer::getSocket
    Function<void(HttpRequest&, HttpResponse&)> onRequest;

  private:
    void onNewClient(AsyncSocketAccept::Result& result);
    void onReceive(AsyncSocketReceive::Result& result);
    void onAfterSend(AsyncSocketSend::Result& result);
    void closeAsync(HttpServerClient& requestClient);

    IGrowableBuffer* headersMemory = nullptr;

    Span<HttpServerClient> clients;
    size_t                 numClients = 0;

    SocketDescriptor  serverSocket;
    AsyncSocketAccept asyncServerAccept;

    uint32_t maxHeaderSize = 8 * 1024;

    bool started  = false;
    bool stopping = false;

    AsyncEventLoop* eventLoop = nullptr;
};
//! @}

} // namespace SC
