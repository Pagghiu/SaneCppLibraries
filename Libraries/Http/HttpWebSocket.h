// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../AsyncStreams/AsyncStreams.h"
#include "../Common/Function.h"
#include "../Common/Result.h"
#include "../Common/Span.h"
#include "HttpExport.h"
#include "HttpParser.h"

namespace SC
{
struct AsyncEventLoop;
struct HttpAsyncClient;
struct HttpAsyncClientRequest;
struct HttpAsyncClientResponse;
struct HttpConnection;
struct HttpRequest;
struct HttpResponse;

//! @addtogroup group_http
//! @{

/// @brief WebSocket frame opcode as defined by RFC 6455
enum class HttpWebSocketOpcode : uint8_t
{
    Continuation = 0x0,

    Text   = 0x1,
    Binary = 0x2,
    Close  = 0x8,
    Ping   = 0x9,
    Pong   = 0xA,
};

/// @brief Identifies the local endpoint role to validate masking rules
enum class HttpWebSocketEndpointRole : uint8_t
{
    Client,
    Server,
};

/// @brief Parsed or to-be-written WebSocket frame header
struct SC_HTTP_EXPORT HttpWebSocketFrameHeaderView
{
    HttpWebSocketOpcode opcode = HttpWebSocketOpcode::Text;

    bool     fin           = true;
    bool     masked        = false;
    uint64_t payloadLength = 0;
    uint8_t  maskKey[4]    = {0, 0, 0, 0};

    [[nodiscard]] bool isControlFrame() const;
};

/// @brief Minimal transport handoff shape for later HTTP upgrade integration
struct SC_HTTP_EXPORT HttpWebSocketTransportView
{
    AsyncReadableStream* readableStream = nullptr;
    AsyncWritableStream* writableStream = nullptr;
    AsyncBuffersPool*    buffersPool    = nullptr;

    void reset();

    [[nodiscard]] bool isValid() const
    {
        return readableStream != nullptr and writableStream != nullptr and buffersPool != nullptr;
    }
};

/// @brief Normalized server-side WebSocket opening handshake data
struct SC_HTTP_EXPORT HttpWebSocketServerHandshakeRequestView
{
    HttpParser::Method method = HttpParser::Method::HttpGET;

    StringSpan version;
    StringSpan upgrade;
    StringSpan connection;
    StringSpan secWebSocketKey;
    StringSpan secWebSocketVersion;
};

/// @brief Normalized client-side WebSocket opening handshake response data
struct SC_HTTP_EXPORT HttpWebSocketClientHandshakeResponseView
{
    uint32_t statusCode = 0;

    StringSpan upgrade;
    StringSpan connection;
    StringSpan secWebSocketAccept;
};

/// @brief Outcome of validating a WebSocket opening handshake
struct SC_HTTP_EXPORT HttpWebSocketHandshakeResult
{
    enum class Status : uint8_t
    {
        Accepted,
        BadRequest,
        UnsupportedVersion,
    };

    Status status = Status::BadRequest;

    [[nodiscard]] bool accepted() const { return status == Status::Accepted; }
    [[nodiscard]] int  httpStatusCode() const;
};

/// @brief Dependency-free RFC 6455 opening handshake helpers
struct SC_HTTP_EXPORT HttpWebSocketHandshake
{
    static constexpr size_t ClientKeyLength = 24;
    static constexpr size_t AcceptKeyLength = 28;
    static constexpr size_t NonceLength     = 16;

    static Result createClientKey(Span<const uint8_t> nonce, Span<char> storage, StringSpan& key);
    static Result validateClientKey(StringSpan key);
    static Result computeAccept(StringSpan clientKey, Span<char> storage, StringSpan& accept);

    static bool headerContainsToken(StringSpan headerValue, StringSpan token);

    static HttpWebSocketHandshakeResult validateServerRequest(const HttpWebSocketServerHandshakeRequestView& request);
    static HttpWebSocketHandshakeResult validateServerRequest(const HttpRequest&                       request,
                                                              HttpWebSocketServerHandshakeRequestView* view = nullptr);

    static Result validateClientResponse(const HttpWebSocketClientHandshakeResponseView& response,
                                         StringSpan                                      expectedClientKey);
    static Result validateClientResponse(const HttpAsyncClientResponse& response, StringSpan expectedClientKey);

    static Result prepareClientRequest(HttpAsyncClientRequest& request, StringSpan clientKey);
    static Result writeServerAccept(HttpResponse& response, StringSpan clientKey, Span<char> acceptStorage,
                                    StringSpan& accept);
    static Result acceptServerConnection(HttpConnection& connection, HttpWebSocketTransportView& transport,
                                         Span<char> acceptStorage);
    static Result rejectServerConnection(HttpResponse& response, const HttpWebSocketHandshakeResult& result);
};

/// @brief Small client-side helper that upgrades an `HttpAsyncClient` connection to a WebSocket transport.
struct SC_HTTP_EXPORT HttpWebSocketClientHandshake
{
    Function<void(HttpWebSocketTransportView&)> onConnected;
    Function<void(Result)>                      onError;

    Result connect(HttpAsyncClient& client, AsyncEventLoop& loop, StringSpan url, StringSpan clientKey,
                   HttpWebSocketTransportView& transport);

  private:
    void onPrepareRequest(HttpAsyncClientRequest& request);
    void onResponse(HttpAsyncClientResponse& response);
    void onClientError(Result result);
    void fail(Result result);

    HttpAsyncClient*            client    = nullptr;
    HttpWebSocketTransportView* transport = nullptr;
    StringSpan                  clientKey;
};

/// @brief Incremental WebSocket frame reader operating on caller-owned mutable byte slices
struct SC_HTTP_EXPORT HttpWebSocketFrameReader
{
    Function<Result(const HttpWebSocketFrameHeaderView&)> onFrameHeader;
    Function<Result(Span<char>, bool)>                    onFramePayload;

    void   reset(HttpWebSocketEndpointRole endpointRole);
    Result parse(Span<char> data, size_t& consumedBytes);

  private:
    enum class State : uint8_t
    {
        HeaderByte0,
        HeaderByte1,
        ExtendedLength,
        MaskKey,
        Payload,
    };

    Result onHeaderReady();
    Result finishCurrentFrame();

    HttpWebSocketEndpointRole    endpointRole = HttpWebSocketEndpointRole::Client;
    HttpWebSocketFrameHeaderView currentFrame;

    State state = State::HeaderByte0;

    bool fragmentedMessageInProgress = false;

    uint8_t headerByte0 = 0;

    uint8_t extendedLengthBytesExpected = 0;
    uint8_t extendedLengthBytesRead     = 0;
    uint8_t maskBytesRead               = 0;

    uint64_t extendedLengthAccumulator = 0;
    uint64_t payloadBytesRemaining     = 0;
    uint64_t payloadBytesConsumed      = 0;
};

/// @brief Incremental WebSocket frame writer operating on caller-owned header storage and payload buffers
struct SC_HTTP_EXPORT HttpWebSocketFrameWriter
{
    void reset(HttpWebSocketEndpointRole endpointRole);

    Result beginFrame(const HttpWebSocketFrameHeaderView& frame, Span<char> storage, Span<const char>& encodedHeader);
    Result writePayload(Span<char> payload);
    Result finishFrame();

  private:
    HttpWebSocketEndpointRole    endpointRole = HttpWebSocketEndpointRole::Client;
    HttpWebSocketFrameHeaderView currentFrame;

    bool frameInProgress             = false;
    bool fragmentedMessageInProgress = false;

    uint64_t payloadBytesRemaining = 0;
    uint64_t payloadBytesWritten   = 0;
};

/// @brief Optional caller-buffered message assembly helper for applications that want complete messages
struct SC_HTTP_EXPORT HttpWebSocketMessageAssembler
{
    Function<Result(HttpWebSocketOpcode, Span<const char>)> onMessage;

    void reset(Span<char> messageStorage);

    Result onFrameHeader(const HttpWebSocketFrameHeaderView& header);
    Result onFramePayload(Span<char> payload, bool frameFinished);

    [[nodiscard]] size_t getCurrentMessageSize() const { return messageSize; }

  private:
    Span<char> messageStorage;

    HttpWebSocketFrameHeaderView currentFrame;
    HttpWebSocketOpcode          messageOpcode = HttpWebSocketOpcode::Text;

    size_t messageSize = 0;

    bool assemblingMessage = false;
    bool ignoringFrame     = false;
};

/// @brief Small frame-lifecycle helper for ping / pong, close, and explicit fixed-buffer send backpressure
struct SC_HTTP_EXPORT HttpWebSocketEndpoint
{
    Function<Result(const HttpWebSocketFrameHeaderView&)>   onFrameHeader;
    Function<Result(HttpWebSocketOpcode, Span<char>, bool)> onDataFramePayload;
    Function<Result(Span<char>)>                            onPing;
    Function<Result(Span<char>)>                            onPong;
    Function<Result(uint16_t, Span<char>)>                  onClose;

    void reset(HttpWebSocketEndpointRole endpointRole);
    void setAutomaticMaskKey(const uint8_t maskKey[4]);

    Result receive(Span<char> data, size_t& consumedBytes);

    Result sendFrame(const HttpWebSocketFrameHeaderView& header, Span<const char> payload, Span<char> storage,
                     Span<const char>& encodedFrame);
    Result sendData(HttpWebSocketOpcode opcode, Span<const char> payload, bool fin, const uint8_t* maskKey,
                    Span<char> storage, Span<const char>& encodedFrame);
    Result sendPing(Span<const char> payload, const uint8_t* maskKey, Span<char> storage,
                    Span<const char>& encodedFrame);
    Result sendPong(Span<const char> payload, const uint8_t* maskKey, Span<char> storage,
                    Span<const char>& encodedFrame);
    Result sendClose(uint16_t statusCode, Span<const char> reason, const uint8_t* maskKey, Span<char> storage,
                     Span<const char>& encodedFrame);

    [[nodiscard]] bool hasPendingControlFrame() const { return pendingControlFrame.sizeInBytes() > 0; }
    Result             getPendingControlFrame(Span<const char>& frame) const;
    void               clearPendingControlFrame();

    [[nodiscard]] bool hasCloseBeenSent() const { return closeSent; }
    [[nodiscard]] bool hasCloseBeenReceived() const { return closeReceived; }

  private:
    Result onReaderFrameHeader(const HttpWebSocketFrameHeaderView& header);
    Result onReaderPayload(Span<char> payload, bool frameFinished);
    Result handleControlFrame(Span<char> payload);
    Result queueAutomaticControl(HttpWebSocketOpcode opcode, Span<const char> payload);
    Result applyOutgoingMask(HttpWebSocketFrameHeaderView& header, const uint8_t* maskKey) const;

    HttpWebSocketEndpointRole    endpointRole = HttpWebSocketEndpointRole::Client;
    HttpWebSocketFrameReader     reader;
    HttpWebSocketFrameWriter     writer;
    HttpWebSocketFrameHeaderView currentFrame;

    char             controlPayload[125]                      = {0};
    size_t           controlPayloadSize                       = 0;
    char             automaticControlStorage[2 + 8 + 4 + 125] = {0};
    Span<const char> pendingControlFrame;
    uint8_t          automaticMaskKey[4] = {0, 0, 0, 0};

    bool automaticMaskKeySet = false;
    bool closeSent           = false;
    bool closeReceived       = false;
};

/// @brief Optional stream pump that binds a WebSocket transport to an endpoint lifecycle.
struct SC_HTTP_EXPORT HttpWebSocketConnectionPump
{
    Function<Result(HttpWebSocketOpcode, Span<char>, bool)> onDataFramePayload;
    Function<void()>                                        onEnd;
    Function<void(Result)>                                  onError;

    Result attach(const HttpWebSocketTransportView& transport, HttpWebSocketEndpointRole endpointRole);
    void   detach();

    Result writeFrame(Span<const char> frame);
    Result flushPendingControlFrame();

    [[nodiscard]] bool isAttached() const { return transport.isValid(); }

    HttpWebSocketEndpoint&       getEndpoint() { return endpoint; }
    const HttpWebSocketEndpoint& getEndpoint() const { return endpoint; }

    void onData(AsyncBufferView::ID bufferID);
    void onStreamEnd();

  private:
    Result onEndpointDataFrame(HttpWebSocketOpcode opcode, Span<char> payload, bool frameFinished);
    void   fail(Result result);

    HttpWebSocketTransportView transport;
    HttpWebSocketEndpoint      endpoint;

    bool dataListenerAdded  = false;
    bool endListenerAdded   = false;
    bool closeListenerAdded = false;
};

/// @brief Fixed-slot WebSocket hub client entry
struct SC_HTTP_EXPORT HttpWebSocketHubClient
{
    HttpWebSocketTransportView transport;
    bool                       active = false;

    void reset();
};

/// @brief Small fixed-capacity broadcast helper for server-side WebSocket fan-out
struct SC_HTTP_EXPORT HttpWebSocketSmallHub
{
    Function<Result(size_t, Span<const char>)> onBroadcastFrame;

    Result init(Span<HttpWebSocketHubClient> clientStorage);

    Result join(const HttpWebSocketTransportView& transport, size_t& clientIndex);
    Result leave(size_t clientIndex);

    Result broadcastFrame(Span<const char> encodedFrame);
    Result broadcastText(Span<const char> payload, Span<char> frameStorage);

    [[nodiscard]] size_t getNumClients() const { return numClients; }
    [[nodiscard]] size_t getCapacity() const { return clients.sizeInElements(); }
    [[nodiscard]] bool   isClientActive(size_t clientIndex) const;

  private:
    Span<HttpWebSocketHubClient> clients;
    size_t                       numClients = 0;
};

//! @}
} // namespace SC
