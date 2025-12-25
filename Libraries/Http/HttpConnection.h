// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "HttpParser.h"

#include "../AsyncStreams/AsyncStreams.h"
#include "../Foundation/StringSpan.h"

namespace SC
{
//! @addtogroup group_http
//! @{

/// @brief Incoming message from the perspective of the participants of an HTTP transaction
struct SC_COMPILER_EXPORT HttpRequest
{
    /// @brief Gets the associated HttpParser
    const HttpParser& getParser() const { return parser; }

    /// @brief Gets the request URL
    StringSpan getURL() const { return url; }

    /// @brief Resets this object for it to be re-usable
    void reset();

  private:
    friend struct HttpConnectionsPool;
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

/// @brief Outgoing message from the perspective of the participants of an HTTP transaction
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
    friend struct HttpConnectionsPool;
    friend struct HttpAsyncServer;

    /// @brief Uses unused header memory data from the HttpRequest for the response
    void grabUnusedHeaderMemory(HttpRequest& request);

    Span<char> responseHeaders;
    size_t     responseHeadersCapacity = 0;

    bool headersSent = false;

    AsyncWritableStream* writableStream = nullptr;
};

/// @brief Http connection abstraction holding both the incoming and outgoing messages in an HTTP transaction
struct SC_COMPILER_EXPORT HttpConnection
{
    HttpConnection();

    struct SC_COMPILER_EXPORT ID
    {
        size_t getIndex() const { return index; }

      private:
        friend struct HttpConnectionsPool;
        size_t index = 0;
    };

    /// @brief Prepare this client for re-use, marking it as Inactive
    void reset();

    /// @brief The ID used to find this client in HttpConnectionsPool
    ID getConnectionID() const { return connectionID; }

    HttpRequest  request;
    HttpResponse response;

  protected:
    enum class State
    {
        Inactive,
        Active
    };
    friend struct HttpConnectionsPool;
    friend struct HttpAsyncServer;

    State state = State::Inactive;
    ID    connectionID;

    Span<char> headerMemory;
};

/// @brief View over a contiguous sequence of items with a custom stride between elements.
template <typename Type>
struct SC_COMPILER_EXPORT SpanWithStride
{
    /// @brief Builds an empty SpanWithStride
    constexpr SpanWithStride() : data(nullptr), sizeElements(0), strideInBytes(0) {}

    /// @brief Builds a SpanWithStride from data, size, and stride
    /// @param data pointer to the first element
    /// @param sizeInElements number of elements
    /// @param strideInBytes stride between elements in bytes
    constexpr SpanWithStride(void* data, size_t sizeInElements, size_t strideInBytes)
        : data(data), sizeElements(sizeInElements), strideInBytes(strideInBytes)
    {}

    [[nodiscard]] constexpr size_t sizeInElements() const { return sizeElements; }
    [[nodiscard]] constexpr bool   empty() const { return sizeElements == 0; }

    template <typename U>
    SpanWithStride<U> castTo()
    {
        return {static_cast<U*>(reinterpret_cast<Type*>(data)), sizeElements, strideInBytes};
    }

    [[nodiscard]] Type& operator[](size_t idx)
    {
        return *reinterpret_cast<Type*>(reinterpret_cast<char*>(data) + idx * strideInBytes);
    }

    [[nodiscard]] const Type& operator[](size_t idx) const
    {
        return *reinterpret_cast<const Type*>(reinterpret_cast<const char*>(data) + idx * strideInBytes);
    }

  private:
    void*  data;
    size_t sizeElements;
    size_t strideInBytes;
};

/// @brief A pool of HttpConnection that can be active or inactive
struct SC_COMPILER_EXPORT HttpConnectionsPool
{
    /// @brief Initializes the server with memory buffers for connections and headers
    Result init(SpanWithStride<HttpConnection> connectionsStorage);

    /// @brief Closes the server, removing references to the memory buffers passed during init
    Result close();

    /// @brief Returns only the number of active connections
    [[nodiscard]] size_t getNumActiveConnections() const { return numConnections; }

    /// @brief Returns the total number of connections (active + inactive)
    [[nodiscard]] size_t getNumTotalConnections() const { return connections.sizeInElements(); }

    /// @brief Returns a connection by ID
    [[nodiscard]] HttpConnection& getConnection(HttpConnection::ID connectionID)
    {
        return connections[connectionID.index];
    }

    /// @brief Returns a connection in the `[0, getNumTotalConnections]` range
    [[nodiscard]] HttpConnection& getConnectionAt(size_t idx) { return connections[idx]; }

    /// @brief Finds an available connection (if any), activates it and returns its ID to use with getConnection(id)
    [[nodiscard]] bool activateNew(HttpConnection::ID& connectionID);

    /// @brief De-activates a connection previously returned by activateNew
    [[nodiscard]] bool deactivate(HttpConnection::ID connectionID);

  private:
    SpanWithStride<HttpConnection> connections;

    size_t numConnections = 0;
};
//! @}

} // namespace SC
