// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "HttpConnection.h"
#include "HttpExport.h"
#include "HttpURLParser.h"

namespace SC
{
//! @addtogroup group_http
//! @{

struct HttpWebSocketTransportView;

/// @brief Mutable transport setup view used by `HttpAsyncClient` after the TCP socket connects.
///
/// The default setup keeps `connection` using its socket streams. A custom setup can install alternate active streams
/// with `HttpConnectionBase::setTransportStreams()` and must call `complete` when the transport is ready for HTTP.
struct SC_HTTP_EXPORT HttpAsyncClientTransportSetup
{
    HttpConnectionBase* connection = nullptr;
    AsyncEventLoop*     eventLoop  = nullptr;

    const HttpURLParser* url = nullptr;

    Function<void(Result)> complete;
};

template <int ReadQueue, int WriteQueue, int HeaderBytes, int StreamBytes>
struct SC_HTTP_EXPORT HttpAsyncClientConnection
    : public HttpStaticConnection<ReadQueue, WriteQueue, HeaderBytes, StreamBytes, 8, HttpConnectionBase>
{
    static constexpr int ExtraBuffers = 8;

    HttpAsyncClientConnection()
    {
        this->readableSocketStream.setAutoDestroy(false);
        this->writableSocketStream.setAutoDestroy(false);
    }
};

/// @brief TLS policy for `HttpAsyncClient` HTTPS requests.
///
/// The pointed-to CA buffers and paths must outlive requests using these options. `https://` transport wiring is still
/// in progress, but the policy lives on the client so request and response message types stay HTTP-only.
struct SC_HTTP_EXPORT HttpAsyncClientTlsOptions
{
    bool verifyPeer = true;

    Span<const char> caCertificates;
    StringSpan       caCertificatesPath;
};

/// @brief Asynchronous HTTP/1.1 client using caller-provided fixed storage
///
/// `HttpAsyncClient` processes a single request at a time and can sequentially reuse the same connection when
/// keep-alive is enabled and the next request targets the same host and port.
///
/// Use the convenience wrappers (`get`, `head`, `put`, `post`, `patch`, `deleteRequest`, `options`,
/// `postMultipart`) when the request body is already available in memory. Use `start()` when the request must be
/// customized inside `onPrepareRequest`, for example to stream the request body with
/// `HttpAsyncClientRequest::setBody(AsyncReadableStream&, uint64_t)` or to write it manually through
/// `HttpAsyncClientRequest::getWritableStream()`.
///
/// `onResponse` is called after response headers have been parsed. The response body is then read incrementally from
/// `HttpAsyncClientResponse::getReadableStream()`, and the readable stream `eventEnd` signals the end of the response
/// body.
///
/// Example without a streamed request body:
/// \snippet Tests/Libraries/Http/HttpAsyncClientTest.cpp HttpAsyncClientBasicSnippet
///
/// Example streaming the request body:
/// \snippet Tests/Libraries/Http/HttpAsyncClientTest.cpp HttpAsyncClientStreamSnippet
struct SC_HTTP_EXPORT HttpAsyncClient
{
    struct Header
    {
        StringSpan name;
        StringSpan value;
    };

    struct RequestOptions
    {
        enum class BodyMode : uint8_t
        {
            None,
            Span,
            Stream,
            Multipart,
        };

        HttpParser::Method method = HttpParser::Method::HttpGET;
        StringSpan         url;

        Span<const Header> headers;

        Span<const char>     body;
        AsyncReadableStream* bodyStream      = nullptr;
        HttpMultipartWriter* multipartWriter = nullptr;

        uint64_t bodyLength = 0;

        BodyMode bodyMode  = BodyMode::None;
        bool     keepAlive = false;

        RequestOptions& clearBody()
        {
            body            = {};
            bodyStream      = nullptr;
            multipartWriter = nullptr;
            bodyLength      = 0;
            bodyMode        = BodyMode::None;
            return *this;
        }

        RequestOptions& setBody(Span<const char> newBody)
        {
            clearBody();
            body       = newBody;
            bodyLength = newBody.sizeInBytes();
            bodyMode   = BodyMode::Span;
            return *this;
        }

        RequestOptions& setBody(StringSpan newBody) { return setBody(newBody.toCharSpan()); }

        RequestOptions& setBody(const char* newBody)
        {
            return setBody(StringSpan::fromNullTerminated(newBody, StringEncoding::Ascii));
        }

        RequestOptions& setBody(AsyncReadableStream& newBodyStream, uint64_t newBodyLength)
        {
            clearBody();
            bodyStream = &newBodyStream;
            bodyLength = newBodyLength;
            bodyMode   = BodyMode::Stream;
            return *this;
        }

        RequestOptions& setMultipart(HttpMultipartWriter& newMultipartWriter)
        {
            clearBody();
            multipartWriter = &newMultipartWriter;
            bodyMode        = BodyMode::Multipart;
            return *this;
        }
    };

    /// @brief Initializes the client with caller-provided connection storage
    /// The storage must outlive the client and provides buffers, queues and socket state.
    Result init(HttpConnectionBase& storage);

    /// @brief Closes any active connection and releases references to the initialized storage
    Result close();

    /// @brief Enables opt-in gzip/deflate response decompression.
    ///
    /// The caller owns the transform and must initialize its read/write queues with the client's buffer pool before
    /// starting a request. When a response declares `Content-Encoding: gzip` or `deflate`, `onResponse` observes the
    /// decoded body through `HttpAsyncClientResponse::getReadableStream()`.
    void setResponseDecompression(SyncZLibTransformStream& decoder) { responseDecoder = &decoder; }

    /// @brief Disables response decompression for future requests.
    void clearResponseDecompression() { responseDecoder = nullptr; }

    /// @brief Sets TLS verification options used by future HTTPS transport integration.
    void setTlsOptions(const HttpAsyncClientTlsOptions& options) { tlsOptions = options; }

    /// @brief Restores default TLS verification options.
    void clearTlsOptions() { tlsOptions = {}; }

    [[nodiscard]] const HttpAsyncClientTlsOptions& getTlsOptions() const { return tlsOptions; }

    /// @brief Sets an optional transport setup hook invoked after TCP connect and before HTTP request bytes are sent.
    ///
    /// The hook can leave the socket streams active for plain HTTP, or install alternate active streams and complete
    /// later. This keeps TLS and other transport adapters outside the core Http library.
    void setTransportSetup(Function<Result(HttpAsyncClientTransportSetup&)>&& setup) { transportSetup = move(setup); }

    /// @brief Clears the optional transport setup hook and restores default socket transport setup.
    void clearTransportSetup() { transportSetup = {}; }

    /// @brief Hands the connected socket streams to a WebSocket owner after a validated `101` response.
    Result detachWebSocketTransport(HttpWebSocketTransportView& transport);

    /// @brief Starts a request that must be configured inside `onPrepareRequest`
    /// `onPrepareRequest` must send the headers before returning, typically by calling
    /// `HttpAsyncClientRequest::sendHeaders()`.
    Result start(AsyncEventLoop& loop, HttpParser::Method method, StringSpan url, bool keepAlive = false);

    /// @brief Starts an auto-sent request described by caller-owned request options.
    /// @warning Header/body/multipart storage must remain valid until the request has been sent.
    Result sendRequest(AsyncEventLoop& loop, const RequestOptions& options);

    /// @brief Convenience wrapper for a GET request without a request body
    Result get(AsyncEventLoop& loop, StringSpan url, bool keepAlive = false);

    /// @brief Convenience wrapper for a HEAD request without a request body
    Result head(AsyncEventLoop& loop, StringSpan url, bool keepAlive = false);

    /// @brief Convenience wrapper for an OPTIONS request without a request body
    Result options(AsyncEventLoop& loop, StringSpan url, bool keepAlive = false);

    /// @brief Convenience wrapper for a DELETE request without a request body
    Result deleteRequest(AsyncEventLoop& loop, StringSpan url, bool keepAlive = false);

    /// @brief Convenience wrapper for a PUT request with a fixed in-memory body
    Result put(AsyncEventLoop& loop, StringSpan url, Span<const char> body, bool keepAlive = false);
    Result put(AsyncEventLoop& loop, StringSpan url, StringSpan body, bool keepAlive = false)
    {
        return put(loop, url, body.toCharSpan(), keepAlive);
    }

    /// @brief Convenience wrapper for a POST request with a fixed in-memory body
    Result post(AsyncEventLoop& loop, StringSpan url, Span<const char> body, bool keepAlive = false);
    Result post(AsyncEventLoop& loop, StringSpan url, StringSpan body, bool keepAlive = false)
    {
        return post(loop, url, body.toCharSpan(), keepAlive);
    }

    /// @brief Convenience wrapper for a PATCH request with a fixed in-memory body
    Result patch(AsyncEventLoop& loop, StringSpan url, Span<const char> body, bool keepAlive = false);
    Result patch(AsyncEventLoop& loop, StringSpan url, StringSpan body, bool keepAlive = false)
    {
        return patch(loop, url, body.toCharSpan(), keepAlive);
    }

    /// @brief Convenience wrapper for a multipart/form-data POST request
    Result postMultipart(AsyncEventLoop& loop, StringSpan url, HttpMultipartWriter& writer, bool keepAlive = false);

    [[nodiscard]] HttpAsyncClientResponse&       getResponse() { return response; }
    [[nodiscard]] const HttpAsyncClientResponse& getResponse() const { return response; }

    /// @brief Called after the request has been created and can still be customized
    Function<void(HttpAsyncClientRequest&)> onPrepareRequest;

    /// @brief Called after the response headers have been parsed
    Function<void(HttpAsyncClientResponse&)> onResponse;

    /// @brief Called on connection, protocol or streaming errors
    Function<void(Result)> onError;

  private:
    struct RequestPreset
    {
        enum class BodyMode : uint8_t
        {
            None,
            Span,
            Stream,
            Multipart,
        };

        HttpParser::Method method = HttpParser::Method::HttpGET;

        StringSpan url;

        bool keepAlive = false;
        bool autoSend  = false;

        Span<const char> bodySpan;

        BodyMode bodyMode      = BodyMode::None;
        uint64_t contentLength = 0;

        Span<const Header> headers;

        AsyncReadableStream* bodyStream      = nullptr;
        HttpMultipartWriter* multipartWriter = nullptr;
    };

    enum class State : uint8_t
    {
        Idle,
        Connecting,
        Sending,
        WaitingResponse,
        StreamingResponse,
    };

    Result startRequest(AsyncEventLoop& loop, const RequestPreset& preset);
    Result prepareRequest(const RequestPreset& preset);
    Result startPreparedRequest(const RequestPreset& preset);
    Result ensureConnected();
    Result beginSocketConnection();
    Result beginResponseRead();
    Result beginRequestSend();
    Result onResponseBodyStreamRead();
    Result validateActiveRequest() const;

    void                 completeTransportSetup(Result result);
    [[nodiscard]] Result rememberConnectedOrigin();

    void finalizeResponse(bool finishBodyStream);
    void closeConnection();
    void finishResponse();
    void fail(Result error);

    void onConnected(AsyncSocketConnect::Result& result);
    void onReadableError(Result result);
    void onWritableError(Result result);
    void onPipelineError(Result result);
    void onReadableEnd();
    void onHeadersBufferWritten(AsyncBufferView::ID bufferID);
    void onResponseData(AsyncBufferView::ID bufferID);
    void onResponseBodyData(AsyncBufferView::ID bufferID);
    void onCompressedResponseBodyData(AsyncBufferView::ID bufferID);
    void onCompressedResponseBodyWritten(AsyncBufferView::ID bufferID);
    void onCompressedResponseBodyEnd();
    void onCompressedResponseError(Result result);

    [[nodiscard]] bool   canReuseConnectionFor(StringSpan protocol, StringSpan host, uint16_t port) const;
    [[nodiscard]] bool   responseMustNotHaveBody() const;
    [[nodiscard]] bool   responseHasKnownLength() const;
    [[nodiscard]] Result prepareResponseDecompression();
    [[nodiscard]] Result startResponseStreams();
    void                 detachResponseDecompression();

    HttpConnectionBase* connection = nullptr;

    AsyncEventLoop*                                  eventLoop      = nullptr;
    HttpAsyncClientRequest*                          currentRequest = nullptr;
    HttpAsyncClientRequest                           request;
    HttpAsyncClientResponse                          response;
    AsyncSocketConnect                               connectAsync;
    RequestPreset                                    currentPreset;
    SyncZLibTransformStream*                         responseDecoder       = nullptr;
    bool                                             responseDecoderActive = false;
    HttpAsyncClientTlsOptions                        tlsOptions;
    Function<Result(HttpAsyncClientTransportSetup&)> transportSetup;

    State state = State::Idle;

    StringSpan currentProtocol;
    char       currentProtocolStorage[16] = {0};

    StringSpan currentHost;
    char       currentHostStorage[256] = {0};
    uint16_t   currentPort             = 0;

    HttpURLParser currentURL;
    uint32_t      requestCount = 0;

    bool hasOpenConnection = false;
    bool responseDelivered = false;
    bool responseFinalized = false;
    bool webSocketUpgraded = false;
};

//! @}
} // namespace SC
