// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "HttpParser.h"

#include "../Async/Async.h"
#include "../AsyncStreams/AsyncRequestStreams.h"
#include "../Foundation/Function.h"
#include "../Foundation/StringSpan.h"

namespace SC
{
//! @addtogroup group_http
//! @{

/// @brief Http request received from a client
struct SC_COMPILER_EXPORT HttpRequest
{
    /// @brief Gets the associated HttpParser
    const HttpParser& getParser() const { return parser; }

    /// @brief Gets the request URL
    StringSpan getURL() const { return url; }

    /// @brief Resets this object for it to be re-usable
    void reset();

  private:
    friend struct HttpServer;
    friend struct HttpResponse;
    friend struct HttpAsyncServer;

    /// @brief Finds a specific HttpParser::Result in the list of parsed header
    /// @param token The result to look for (Method, Url etc.)
    /// @param res A StringSpan, pointing at headerBuffer containing the found result
    /// @return `true` if the result has been found
    [[nodiscard]] bool findParserToken(HttpParser::Token token, StringSpan& res) const;

    /// @brief Parses an incoming slice of data (must be slice of availableHeader)
    Result writeHeaders(const uint32_t maxHeaderSize, Span<const char> readData);

    struct SC_COMPILER_EXPORT HttpHeaderOffset
    {
        HttpParser::Token token = HttpParser::Token::Method;

        uint32_t start  = 0;
        uint32_t length = 0;
    };
    Span<char> readHeaders;     ///< Headers read so far
    Span<char> availableHeader; ///< Space to save headers to

    bool headersEndReceived = false; ///< All headers have been received
    bool parsedSuccessfully = true;  ///< Request headers have been parsed successfully

    HttpParser parser; ///< The parser used to parse headers
    StringSpan url;    ///< The url extracted from parsed headers

    static constexpr size_t MaxNumHeaders = 64;

    HttpHeaderOffset headerOffsets[MaxNumHeaders]; ///< Headers, defined as offsets in headerBuffer
    size_t           numHeaders = 0;
};

/// @brief Http response that will be sent to a client
struct SC_COMPILER_EXPORT HttpResponse
{
    /// @brief Starts the response with a http standard code (200 OK, 404 NOT FOUND etc.)
    Result startResponse(int httpCode);

    /// @brief Writes an http header to this response
    Result addHeader(StringSpan headerName, StringSpan headerValue);

    /// @brief Start sending response headers, before sending any data
    Result sendHeaders();

    /// @brief Resets this object for it to be re-usable
    void reset();

    /// @brief Finalizes the writable stream after sending all in progress writes
    Result end();

    /// @brief Obtain writable stream for sending content back to connected client
    AsyncWritableStream& getWritableStream() { return *writableStream; }

  private:
    friend struct HttpServer;
    friend struct HttpAsyncServer;

    /// @brief Uses unused header memory data from the HttpRequest for the response
    void grabUnusedHeaderMemory(HttpRequest& request);

    Span<char> responseHeaders;
    size_t     responseHeadersCapacity = 0;

    bool headersSent = false;

    AsyncWritableStream* writableStream = nullptr;
};

struct SC_COMPILER_EXPORT HttpServerClient
{
    HttpServerClient();

    struct SC_COMPILER_EXPORT ID
    {
        size_t getIndex() const { return index; }

      private:
        friend struct HttpServer;
        size_t index = 0;
    };

    /// @brief Prepare this client for re-use, marking it as Inactive
    void reset();

    /// @brief The Client ID used to find this client in HttpServer clients Span
    ID getClientID() const { return clientID; }

    HttpRequest  request;
    HttpResponse response;

  private:
    enum class State
    {
        Inactive,
        Active
    };
    friend struct HttpServer;
    friend struct HttpAsyncServer;

    State state = State::Inactive;
    ID    clientID;

    ReadableSocketStream readableSocketStream;
    WritableSocketStream writableSocketStream;

    SocketDescriptor socket;
};

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
    /// @brief Initializes the server with memory buffers for clients and headers
    Result init(Span<HttpServerClient> clients, Span<char> headersMemory);

    /// @brief Closes the server, removing references to the memory buffers passed during init
    Result close();

    /// @brief Return the number of active clients
    [[nodiscard]] size_t getNumActiveClients() const { return numClients; }

    /// @brief Return the total number of clients (active + inactive)
    [[nodiscard]] size_t getNumTotalClients() const { return clients.sizeInElements(); }

    /// @brief Returns a client by ID
    [[nodiscard]] HttpServerClient& getClient(HttpServerClient::ID clientID) { return clients[clientID.index]; }

    /// @brief Returns a client in the `[0, getNumTotalClients]` range
    [[nodiscard]] HttpServerClient& getClientAt(size_t idx) { return clients[idx]; }

    /// @brief Finds an available client (if any), activates it and returns its ID to use with getClient(id)
    [[nodiscard]] bool activateAvailableClient(HttpServerClient::ID& clientID);

    /// @brief De-activates a client previously returned by activateAvailableClient
    [[nodiscard]] bool deactivateClient(HttpServerClient::ID clientID);

    /// @brief Called after enough data from a newly connected client has arrived, causing all headers to be parsed.
    Function<void(HttpServerClient&)> onRequest;

  private:
    void closeAsync(HttpServerClient& requestClient);

    Span<HttpServerClient> clients;
    Span<char>             headersMemory;

    size_t numClients = 0;
};
//! @}

} // namespace SC
