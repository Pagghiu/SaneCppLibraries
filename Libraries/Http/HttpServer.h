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
#if SC_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4251)
#endif
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

    /// @brief Resets this object for it to be re-usable
    void reset();

    /// @brief Parses an incoming slice of data (must be slice of availableHeader)
    Result parse(const uint32_t maxHeaderSize, Span<const char> readData);

  private:
    friend struct HttpServer;
    friend struct HttpAsyncServer;
    using HttpHeaderOffset = detail::HttpHeaderOffset;

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

    /// @brief Obtain writable stream to write content
    AsyncWritableStream& getWritableStream() { return *writableStream; }

  private:
    friend struct HttpServer;
    friend struct HttpAsyncServer;

    Span<char> responseHeaders;
    size_t     responseHeadersCapacity = 0;

    bool headersSent = false;

    AsyncWritableStream* writableStream = nullptr;
};

struct SC_COMPILER_EXPORT HttpServerClient
{
    HttpServerClient();

    void reset()
    {
        request.reset();
        response.reset();
        state = State::Free;
    }

    HttpRequest  request;
    HttpResponse response;

  private:
    enum class State
    {
        Free,
        Used
    };
    friend struct HttpServer;
    friend struct HttpAsyncServer;
    char debugName[16] = {0};

    State  state = State::Free;
    size_t index = 0;

    ReadableSocketStream readableSocketStream;
    WritableSocketStream writableSocketStream;

    SocketDescriptor socket;
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
    /// @brief Starts the http server
    /// @param memory Memory buffers to be used by the http server
    Result start(Memory& memory);

    /// @brief Called after enough data from a newly connected client has arrived, causing all headers to be parsed.
    /// @warning Both references can be invalidated in later stages of the http request lifetime.
    /// If necessary, store the client key returned by SC::HttpResponse::getClientKey and use it with
    /// SC::HttpServer::getRequest, SC::HttpServer::getResponse or SC::HttpServer::getSocket
    Function<void(HttpRequest&, HttpResponse&)> onRequest;

    Span<HttpServerClient> clients;

    [[nodiscard]] size_t getNumClients() const { return numClients; }

    [[nodiscard]] bool canAcceptMoreClients() const { return numClients < clients.sizeInElements(); }
    [[nodiscard]] bool allocateClient(size_t& idx);
    [[nodiscard]] bool deallocateClient(HttpServerClient& client);

    uint32_t maxHeaderSize = 8 * 1024;

  private:
    void closeAsync(HttpServerClient& requestClient);

    IGrowableBuffer* headersMemory = nullptr;
    size_t           numClients    = 0;
};
//! @}
#if SC_COMPILER_MSVC
#pragma warning(pop)
#endif

} // namespace SC
