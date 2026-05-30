// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "HttpExport.h"
#include "HttpParser.h"

#include "../Async/Async.h"
#include "../AsyncStreams/AsyncRequestStreams.h"
#include "../Foundation/StringSpan.h"
#include "Internal/HttpFixedBufferWriter.h"
#include "Internal/HttpParsedHeaders.h"

namespace SC
{
//! @addtogroup group_http
//! @{

enum class HttpBodyFramingKind : uint8_t
{
    None,
    ContentLength,
    Chunked,
    CloseDelimited,
};

enum class HttpContentEncoding : uint8_t
{
    Identity,
    GZip,
    Deflate,
};

SC_HTTP_EXPORT StringSpan httpContentEncodingName(HttpContentEncoding encoding);
SC_HTTP_EXPORT Result     httpContentEncodingFromHeader(StringSpan headerValue, HttpContentEncoding& encoding);

struct HttpAsyncClient;
struct SyncZLibTransformStream;

struct SC_HTTP_EXPORT HttpMultipartWriter
{
    struct Part
    {
        StringSpan partName;
        StringSpan fileName;
        StringSpan contentType;

        Span<const char> body;
    };

    static constexpr size_t MaxParts = 8;

    void reset();

    Result setBoundary(StringSpan boundaryValue);
    Result addField(StringSpan fieldName, StringSpan value);
    Result addFile(StringSpan fieldName, StringSpan fileName, Span<const char> body,
                   StringSpan contentType = "application/octet-stream");

    [[nodiscard]] StringSpan  getBoundary() const { return boundary; }
    [[nodiscard]] size_t      getNumParts() const { return numParts; }
    [[nodiscard]] const Part& getPart(size_t idx) const { return parts[idx]; }
    [[nodiscard]] size_t      getContentLength() const;

  private:
    Part       parts[MaxParts];
    size_t     numParts            = 0;
    char       boundaryStorage[71] = {0};
    StringSpan boundary;
};

/// @brief Shared async transport storage for HTTP client and server endpoints
struct SC_HTTP_EXPORT HttpConnectionBase
{
    using ReadableSocketStream = AsyncReadableSocketStream<AsyncEventLoop>;
    using WritableSocketStream = AsyncWritableSocketStream<AsyncEventLoop>;

    ReadableSocketStream readableSocketStream;
    WritableSocketStream writableSocketStream;
    AsyncBuffersPool     buffersPool;
    AsyncPipeline        pipeline;
    SocketDescriptor     socket;

    void setHeaderMemory(Span<char> memory) { headerMemory = memory; }

    [[nodiscard]] Span<char> getHeaderMemory() const { return headerMemory; }

    void setTransportStreams(AsyncReadableStream& readable, AsyncWritableStream& writable);
    void resetTransportStreams();

    [[nodiscard]] AsyncReadableStream& getReadableTransportStream() { return *readableTransportStream; }
    [[nodiscard]] AsyncWritableStream& getWritableTransportStream() { return *writableTransportStream; }

    [[nodiscard]] const AsyncReadableStream& getReadableTransportStream() const { return *readableTransportStream; }
    [[nodiscard]] const AsyncWritableStream& getWritableTransportStream() const { return *writableTransportStream; }

    void reset();

  protected:
    Span<char> headerMemory;

  private:
    AsyncReadableStream* readableTransportStream = &readableSocketStream;
    AsyncWritableStream* writableTransportStream = &writableSocketStream;
};

/// @brief Incoming message from the perspective of the participants of an HTTP transaction
struct SC_HTTP_EXPORT HttpIncomingMessage
{
    struct SC_HTTP_EXPORT BodyStream : public AsyncReadableStream
    {
        BodyStream();

        Result begin(AsyncBuffersPool& buffersPool, Function<Result()>&& onReadRequest);
        bool   pushBodyData(AsyncBufferView::ID bufferID, size_t sizeInBytes);
        void   finishBody();
        void   failBody(Result result);

        Function<Result()> onReadRequest;
        Request            queue[4];

      private:
        virtual Result asyncRead() override;
        virtual Result asyncResumeReading() override;
    };

    /// @brief Gets the associated HttpParser
    const HttpParser& getParser() const { return parsedHeaders.parser; }

    /// @brief Returns true once the incoming message headers have been parsed.
    [[nodiscard]] bool hasReceivedHeaders() const { return parsedHeaders.headersEndReceived; }

    /// @brief Gets whether the other party requested the connection to stay alive
    [[nodiscard]] bool getKeepAlive() const { return parsedHeaders.parser.connectionKeepAlive; }

    /// @brief Gets the parsed HTTP protocol version token, for example `HTTP/1.1`
    [[nodiscard]] StringSpan getVersion() const;

    /// @brief Returns the effective framing mode for the incoming body
    [[nodiscard]] HttpBodyFramingKind getBodyFramingKind() const { return bodyFramingKind; }

    /// @brief Returns how many body bytes are still expected for this message
    [[nodiscard]] uint64_t getBodyBytesRemaining() const { return bodyBytesRemaining; }

    /// @brief Returns true once the full body has been received according to its framing
    [[nodiscard]] bool isBodyComplete() const { return bodyComplete; }

    /// @brief Checks if the request is a multipart/form-data request
    [[nodiscard]] bool isMultipart() const;

    /// @brief Gets the multipart boundary string (if isMultipart() returns true)
    [[nodiscard]] StringSpan getBoundary() const;

    /// @brief Gets the value of a specific header (case-insensitive name matching)
    /// @param headerName The name of the header to find
    /// @param value Output parameter that receives the header value if found
    /// @return `true` if the header was found
    [[nodiscard]] bool getHeader(StringSpan headerName, StringSpan& value) const;

    /// @brief Obtains the readable stream for the message body
    AsyncReadableStream& getReadableStream();

    /// @brief Obtains the readable stream for the message body
    const AsyncReadableStream& getReadableStream() const;

    /// @brief Decrements the remaining body bytes after consuming data
    Result consumeBodyBytes(size_t bytes);

  protected:
    void resetIncoming(HttpParser::Type type, Span<char> memory);

    [[nodiscard]] Span<char> getUnusedHeaderMemory() const { return parsedHeaders.availableHeader; }

    Result initBodyStream(AsyncBuffersPool& buffersPool, Function<Result()>&& onReadRequest);
    Result startBodyStream();
    bool   pushBodyData(AsyncBufferView::ID bufferID, size_t sizeInBytes);
    void   finishBodyStream();
    void   failBodyStream(Result result);
    void   abortBodyStream();
    Result prepareBodyStream(AsyncBuffersPool& buffersPool, Function<Result()>&& onReadRequest,
                             bool allowCloseDelimited);
    Result processBodyData(AsyncReadableStream& sourceStream, AsyncBufferView::ID bufferID, Span<const char> readData,
                           bool allowTrailingData);

    void setBodyBytesRemaining(uint64_t value) { bodyBytesRemaining = value; }
    void setBodyFramingKind(HttpBodyFramingKind value) { bodyFramingKind = value; }
    void setBodyComplete(bool value) { bodyComplete = value; }
    void attachReadableStream(AsyncReadableStream& stream) { readableStream = &stream; }

    /// @brief Finds a specific HttpParser::Result in the list of parsed header
    /// @param token The result to look for (Method, Url etc.)
    /// @param res A StringSpan, pointing at headerBuffer containing the found result
    /// @return `true` if the result has been found
    [[nodiscard]] bool findParserToken(HttpParser::Token token, StringSpan& res) const;

    /// @brief Parses an incoming slice of data eventually copying it to the availableHeader.
    /// If it encounters body data, it will create a child view and unshift it to the stream.
    Result writeHeaders(const uint32_t maxHeaderSize, Span<const char> readData, AsyncReadableStream& stream,
                        AsyncBufferView::ID bufferID, const char* outOfSpaceError, const char* sizeExceededError,
                        bool stopAtHeadersEnd, bool unshiftPendingBodyToStream = true);

    /// @brief Gets the length of the headers in bytes
    [[nodiscard]] size_t getHeadersLength() const;

    enum class ChunkedState : uint8_t
    {
        Size,
        SizeExtension,
        SizeLF,
        Data,
        DataCR,
        DataLF,
        TrailerLineStart,
        TrailerEndLF,
        Finished,
    };

    HttpParsedHeaders    parsedHeaders;
    Span<char>           headerMemory;
    AsyncReadableStream* readableStream     = nullptr;
    uint64_t             bodyBytesRemaining = 0;
    HttpBodyFramingKind  bodyFramingKind    = HttpBodyFramingKind::None;
    bool                 bodyComplete       = true;
    bool                 bodyStreamStarted  = false;
    BodyStream           bodyStream;
    ChunkedState         chunkedState          = ChunkedState::Size;
    uint64_t             chunkedChunkSize      = 0;
    uint64_t             chunkedBytesRemaining = 0;
    bool                 chunkedSizeHasDigits  = false;
};

/// @brief Incoming HTTP request received by the server
struct SC_HTTP_EXPORT HttpRequest : public HttpIncomingMessage
{
    /// @brief Gets the raw request target from the request line
    StringSpan getRequestTarget() const { return url; }

    /// @brief Gets the request URL
    StringSpan getURL() const { return url; }

    /// @brief Resets this object for it to be re-usable
    void reset();

  private:
    friend struct HttpConnectionsPool;
    friend struct HttpResponse;
    friend struct HttpAsyncServer;

    void setHeaderMemory(Span<char> memory);

    /// @brief Parses request headers and extracts the request URL
    Result writeHeaders(const uint32_t maxHeaderSize, Span<const char> readData, AsyncReadableStream& stream,
                        AsyncBufferView::ID bufferID);

    StringSpan url;
};

/// @brief Incoming HTTP response received by the client
struct SC_HTTP_EXPORT HttpAsyncClientResponse : public HttpIncomingMessage
{
  private:
    friend struct HttpAsyncClient;

    void        reset(Span<char> memory);
    Result      writeHeaders(uint32_t maxHeaderSize, Span<const char> readData, AsyncReadableStream& stream,
                             AsyncBufferView::ID bufferID);
    BodyStream& rawBodyStream() { return bodyStream; }
};

/// @brief Outgoing message from the perspective of the participants of an HTTP transaction
struct SC_HTTP_EXPORT HttpOutgoingMessage
{
    struct SC_HTTP_EXPORT ChunkedWritableStream : public AsyncWritableStream
    {
        ChunkedWritableStream();

        Result init(AsyncBuffersPool& buffersPool, AsyncWritableStream& destination);

      protected:
        virtual Result asyncWrite(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb) override;
        virtual bool   canEndWritable() override;

      private:
        void onChunkHeaderWritten(AsyncBufferView::ID);
        void onChunkBodyWritten(AsyncBufferView::ID);
        void onChunkTerminatorWritten(AsyncBufferView::ID);
        void onFinalChunkWritten(AsyncBufferView::ID);

        AsyncWritableStream* destination = nullptr;

        AsyncBufferView::ID                 currentBodyBufferID;
        Function<void(AsyncBufferView::ID)> currentBodyCallback;

        bool finalChunkStarted = false;
        bool finalChunkWritten = false;

        char headerStorage[20] = {0};

        Request queue[4];
    };

    /// @brief Writes an http header to this response
    /// @return Valid Result if header was added successfully.
    /// @warning Adding a "Connection" header can fail if keep-alive has been force disabled
    Result addHeader(StringSpan headerName, StringSpan headerValue);

    /// @brief Adds a formatted `Content-Length` header without caller-side temporary formatting.
    Result addContentLength(uint64_t value);

    /// @brief Enables chunked transfer-encoding for subsequent body writes
    Result setChunkedTransferEncoding();

    /// @brief Start sending response headers, before sending any data
    Result sendHeaders(Function<void(AsyncBufferView::ID)> callback = {});

    /// @brief Resets this object for it to be re-usable
    void reset();

    /// @brief Finalizes the writable stream after sending all in progress writes
    Result end();

    /// @brief Obtain writable stream for sending content back to connected client
    AsyncWritableStream& getWritableStream() { return *writableStream; }

    /// @brief Sets whether to keep the connection alive after this response
    /// @param keepAlive true to keep connection open for more requests, false to close after response
    /// @warning  HttpConnection can force disable keep-alive when running out of connections to prevent server deadlock
    void setKeepAlive(bool keepAlive);

    /// @brief Gets whether the connection should be kept alive after this response
    /// @return true if connection should be kept alive
    [[nodiscard]] bool getKeepAlive() const { return forceDisableKeepAlive ? false : keepAlive; }

  protected:
    enum class KnownHeader : uint8_t
    {
        Connection,
        Host,
        UserAgent,
        ContentLength,
        ContentType,
        TransferEncoding,
        ContentEncoding,
        AcceptEncoding,
    };

    void setHeaderMemory(Span<char> memory);
    void setWritableStream(AsyncWritableStream& stream)
    {
        destinationStream = &stream;
        writableStream    = &stream;
    }

    [[nodiscard]] bool hasHeader(KnownHeader header) const;
    [[nodiscard]] bool hasSentHeaders() const { return headersSent; }
    [[nodiscard]] bool hasEnded() const { return endCalled; }
    [[nodiscard]] bool isChunkedTransferEncodingEnabled() const { return chunkedTransferEncodingEnabled; }

    HttpFixedBufferWriter responseHeaders;
    Span<char>            headerMemory;

    bool headersSent = false;
    bool endCalled   = false;

    bool forceDisableKeepAlive          = false; ///< Whether keep alive has been force disabled permanently
    bool keepAlive                      = true;  ///< Whether to keep connection alive (HTTP/1.1 default)
    bool connectionHeaderAdded          = false; ///< Whether Connection header was manually added
    bool hostHeaderAdded                = false;
    bool userAgentHeaderAdded           = false;
    bool contentLengthAdded             = false;
    bool contentTypeAdded               = false;
    bool transferEncodingAdded          = false;
    bool contentEncodingAdded           = false;
    bool acceptEncodingAdded            = false;
    bool chunkedTransferEncodingEnabled = false;

    AsyncWritableStream*  destinationStream = nullptr;
    AsyncWritableStream*  writableStream    = nullptr;
    ChunkedWritableStream chunkedWritableStream;
};

/// @brief Outgoing HTTP response sent by the server
struct SC_HTTP_EXPORT HttpResponse : public HttpOutgoingMessage
{
    /// @brief Starts the response with a http standard code (200 OK, 404 NOT FOUND etc.)
    Result startResponse(int httpCode);

    /// @brief Starts the response with an explicit status code and reason phrase.
    Result startResponse(int httpCode, StringSpan reasonPhrase);

    /// @brief Starts a fixed-size body response and adds Content-Length plus optional Content-Type.
    Result startBody(int httpCode, uint64_t contentLength, StringSpan contentType = {});

    /// @brief Sends a caller-owned fixed-size byte response.
    /// @warning The body span must remain valid until the writable stream finishes writing it.
    Result sendBytes(int httpCode, Span<const char> body, StringSpan contentType = {});

    /// @brief Sends a caller-owned fixed-size body response.
    /// @warning The body span must remain valid until the writable stream finishes writing it.
    Result sendBody(int httpCode, StringSpan body, StringSpan contentType = {});

    /// @brief Sends a caller-owned text/plain; charset=utf-8 response body.
    /// @warning The body span must remain valid until the writable stream finishes writing it.
    Result sendText(int httpCode, StringSpan body);

    /// @brief Sends a caller-owned application/json response body.
    /// @warning The body span must remain valid until the writable stream finishes writing it.
    Result sendJson(int httpCode, StringSpan body);

    /// @brief Sends an empty response with `Content-Length: 0`.
    Result sendEmpty(int httpCode);

    /// @brief Sends a 405 Method Not Allowed response with an Allow header and `Content-Length: 0`.
    Result sendMethodNotAllowed(StringSpan allow);

    /// @brief Sends a 3xx redirect response with a Location header and `Content-Length: 0`.
    Result sendRedirect(int httpCode, StringSpan location);

  private:
    friend struct HttpConnectionsPool;
    friend struct HttpAsyncServer;

    /// @brief Uses unused header memory data from the HttpRequest for the response
    void grabUnusedHeaderMemory(HttpRequest& request);
};

/// @brief Outgoing HTTP request sent by the client
struct SC_HTTP_EXPORT HttpAsyncClientRequest : public HttpOutgoingMessage
{
    enum class BodyType : uint8_t
    {
        None,
        Manual,
        Span,
        Stream,
        Multipart,
    };

    void reset();

    Result startRequest(HttpParser::Method method, StringSpan url);
    Result setExpectedBodyLength(uint64_t value);
    Result setBody(Span<const char> value);
    Result setBody(StringSpan value) { return setBody(value.toCharSpan()); }
    Result setBody(AsyncReadableStream& stream);
    void   setBody(AsyncReadableStream& stream, uint64_t contentLengthValue);
    Result setCompressedBody(AsyncReadableStream& stream, SyncZLibTransformStream& compressor,
                             HttpContentEncoding encoding);
    void   setMultipart(HttpMultipartWriter& value);
    Result sendHeaders(Function<void(AsyncBufferView::ID)> callback = {});

    [[nodiscard]] HttpParser::Method getMethod() const { return method; }

    [[nodiscard]] StringSpan          getRequestTarget() const { return url; }
    [[nodiscard]] StringSpan          getURL() const { return url; }
    [[nodiscard]] BodyType            getBodyType() const { return bodyType; }
    [[nodiscard]] uint64_t            getContentLength() const { return contentLength; }
    [[nodiscard]] bool                usesChunkedTransferEncoding() const { return chunkedTransferEncodingEnabled; }
    [[nodiscard]] HttpContentEncoding getContentEncoding() const { return contentEncoding; }

  private:
    friend struct HttpAsyncClient;

    void setDefaultHost(StringSpan value) { defaultHost = value; }

    [[nodiscard]] bool hasTransferEncodingHeader() const { return hasHeader(KnownHeader::TransferEncoding); }
    [[nodiscard]] bool hasAcceptEncodingHeader() const { return hasHeader(KnownHeader::AcceptEncoding); }

    [[nodiscard]] AsyncReadableStream*       getBodyStream() const { return bodyStream; }
    [[nodiscard]] SyncZLibTransformStream*   getBodyTransform() const { return bodyTransform; }
    [[nodiscard]] const HttpMultipartWriter* getMultipartWriter() const { return multipartWriter; }
    [[nodiscard]] Span<const char>           getBodySpan() const { return bodySpan; }

    HttpParser::Method       method = HttpParser::Method::HttpGET;
    StringSpan               url;
    AsyncReadableStream*     bodyStream    = nullptr;
    SyncZLibTransformStream* bodyTransform = nullptr;
    BodyType                 bodyType      = BodyType::None;
    Span<const char>         bodySpan;
    uint64_t                 contentLength   = 0;
    HttpContentEncoding      contentEncoding = HttpContentEncoding::Identity;
    HttpMultipartWriter*     multipartWriter = nullptr;
    StringSpan               defaultHost;

    Function<void(AsyncBufferView::ID)> userHeadersSentCallback;
    Function<void(AsyncBufferView::ID)> internalHeadersSentCallback;
};

/// @brief Http connection abstraction holding both the incoming and outgoing messages in an HTTP transaction
struct SC_HTTP_EXPORT HttpConnection : public HttpConnectionBase
{
    HttpConnection();

    struct SC_HTTP_EXPORT ID
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

    /// @brief Marks this HTTP connection as handed off to a WebSocket owner after a successful upgrade.
    void markWebSocketUpgraded() { webSocketUpgraded = true; }

    /// @brief Returns true after the HTTP layer handed this connection to WebSocket code.
    [[nodiscard]] bool isWebSocketUpgraded() const { return webSocketUpgraded; }

    /// @brief Sends a fixed-size response body by copying it into this connection's async buffer pool.
    Result sendBodyCopy(int httpCode, StringSpan body, StringSpan contentType = {});

    /// @brief Sends a text/plain; charset=utf-8 response body by copying it into this connection's async buffer pool.
    Result sendTextCopy(int httpCode, StringSpan body);

    /// @brief Sends an application/json response body by copying it into this connection's async buffer pool.
    Result sendJsonCopy(int httpCode, StringSpan body);

    HttpRequest  request;
    HttpResponse response;

    uint32_t requestCount = 0; ///< Number of requests processed on this connection

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
    bool  webSocketUpgraded = false;
};

/// @brief View over a contiguous sequence of items with a custom stride between elements.
template <typename Type>
struct SC_HTTP_EXPORT SpanWithStride
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

    template <typename U>
    constexpr SpanWithStride(Span<U> span)
        : data(span.data()), sizeElements(span.sizeInElements()), strideInBytes(sizeof(U))
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
struct SC_HTTP_EXPORT HttpConnectionsPool
{
    struct Configuration
    {
        size_t readQueueSize     = 3;
        size_t writeQueueSize    = 3;
        size_t buffersQueueSize  = 6;
        size_t headerBytesLength = 8 * 1024;
        size_t streamBytesLength = 512 * 1024;
    };

    struct SC_HTTP_EXPORT Memory
    {
        Span<AsyncReadableStream::Request> allReadQueue;
        Span<AsyncWritableStream::Request> allWriteQueue;

        Span<AsyncBufferView> allBuffers;

        Span<char> allHeaders;
        Span<char> allStreams;

        Result assignTo(HttpConnectionsPool::Configuration conf, SpanWithStride<HttpConnection> connections);
    };

    /// @brief Initializes the server with memory buffers for connections and headers
    Result init(SpanWithStride<HttpConnection> connectionsStorage);

    /// @brief Closes the server, removing references to the memory buffers passed during init
    Result close();

    /// @brief Returns only the number of active connections
    [[nodiscard]] size_t getNumActiveConnections() const { return numConnections; }

    /// @brief Returns the total number of connections (active + inactive)
    [[nodiscard]] size_t getNumTotalConnections() const { return connections.sizeInElements(); }

    /// @brief Returns only the number of active connections
    [[nodiscard]] size_t getHighestActiveConnection() const { return highestActiveConnection; }

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
    SpanWithStride<HttpConnection> connections; ///< The connection storages

    size_t numConnections = 0; ///< Number of active connections only

    // Optimization for HttpAsyncServer::resize to avoid having to scan all connections
    size_t highestActiveConnection = 0;
};

/// @brief Adds compile-time configurable read and write queues to any class subclassing HttpConnectionBase
template <int ReadQueue, int WriteQueue, int HeaderBytes, int StreamBytes, int ExtraBuffers, typename BaseClass>
struct SC_HTTP_EXPORT HttpStaticConnection : public BaseClass
{
    AsyncReadableStream::Request readQueue[ReadQueue];
    AsyncWritableStream::Request writeQueue[WriteQueue];

    AsyncBufferView buffers[ReadQueue + WriteQueue + ExtraBuffers];

    char headerStorage[HeaderBytes];
    char streamStorage[StreamBytes];

    constexpr HttpStaticConnection()
    {
        constexpr const size_t NumSlices   = ReadQueue;
        constexpr const size_t SliceLength = StreamBytes / NumSlices;

        Span<char> memory = streamStorage;
        for (size_t idx = 0; idx < NumSlices; ++idx)
        {
            Span<char> slice;
            (void)memory.sliceStartLength(idx * SliceLength, SliceLength, slice);
            buffers[idx] = slice;
            buffers[idx].setReusable(true);
        }
        this->setHeaderMemory(headerStorage);
        this->buffersPool.setBuffers(buffers);
        this->readableSocketStream.setReadQueue(readQueue);
        this->writableSocketStream.setWriteQueue(writeQueue);
    }
};

//! @}
} // namespace SC
