// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Foundation/Compiler.h"
#ifndef SC_EXPORT_LIBRARY_HTTP_CLIENT
#define SC_EXPORT_LIBRARY_HTTP_CLIENT 0
#endif
#define SC_HTTP_CLIENT_EXPORT SC_COMPILER_LIBRARY_EXPORT(SC_EXPORT_LIBRARY_HTTP_CLIENT)

#include "../Foundation/Result.h"
#include "../Foundation/Span.h"
#include "../Foundation/StringSpan.h"
#include "Internal/HttpClientThreading.h"

namespace SC
{
//! @addtogroup group_http_client
//! @{

struct SC_HTTP_CLIENT_EXPORT HttpClientRequestBodyProvider;
struct SC_HTTP_CLIENT_EXPORT HttpClientRequestOptions;

/// @brief HTTP header name/value view
struct SC_HTTP_CLIENT_EXPORT HttpClientHeader
{
    StringSpan name;
    StringSpan value;
};

/// @brief Caller-owned cursor for iterating response headers
struct SC_HTTP_CLIENT_EXPORT HttpClientResponseHeaderIterator
{
    size_t offset = 0;
};

/// @brief Parsed response content-coding token view
struct SC_HTTP_CLIENT_EXPORT HttpClientContentCoding
{
    enum Type : uint8_t
    {
        Unknown,
        Identity,
        GZip,
        Deflate,
        Compress,
        Brotli,
    };

    Type       type = Unknown;
    StringSpan name;

    [[nodiscard]] bool               isIdentity() const { return type == Identity; }
    [[nodiscard]] static Type        parseName(StringSpan name);
    [[nodiscard]] static const char* getName(Type type);
    [[nodiscard]] static Result writeAcceptEncoding(Span<const Type> types, Span<char> destination, StringSpan& value);
};

/// @brief Caller-owned cursor for iterating comma-separated Content-Encoding values.
struct SC_HTTP_CLIENT_EXPORT HttpClientContentCodingIterator
{
    HttpClientResponseHeaderIterator headerIterator;
    StringSpan                       headerValue;
    size_t                           valueOffset    = 0;
    bool                             hasHeaderValue = false;
};

/// @brief Parsed response transfer-coding token view.
struct SC_HTTP_CLIENT_EXPORT HttpClientTransferCoding
{
    enum Type : uint8_t
    {
        Unknown,
        Chunked,
        Compress,
        Deflate,
        GZip,
    };

    Type       type = Unknown;
    StringSpan name;

    [[nodiscard]] bool               isChunked() const { return type == Chunked; }
    [[nodiscard]] static Type        parseName(StringSpan name);
    [[nodiscard]] static const char* getName(Type type);
};

/// @brief Caller-owned cursor for iterating comma-separated Transfer-Encoding values.
struct SC_HTTP_CLIENT_EXPORT HttpClientTransferCodingIterator
{
    HttpClientResponseHeaderIterator headerIterator;
    StringSpan                       headerValue;
    size_t                           valueOffset    = 0;
    bool                             hasHeaderValue = false;
};

/// @brief Compile-time backend capability report for the active HttpClient backend
struct SC_HTTP_CLIENT_EXPORT HttpClientCapabilities
{
    enum Backend : uint8_t
    {
        Unsupported,
        AppleURLSession,
        LibCurl,
        WinHttp,
    };

    enum Feature : uint8_t
    {
        MultipleOperationsPerClient,
        FixedRequestBody,
        SizedStreamRequestBody,
        ChunkedStreamRequestBody,
        RedirectPolicy,
        ProtocolHttp11Only,
        ProtocolHttp2Preferred,
        ProtocolHttp2Required,
        TlsDisablePeerVerification,
        TlsCustomCaPath,
        ProxyNoProxy,
        ProxyHttp,
        ProxyAuthorization,
        ProxyBypassList,
        ContentCodingPolicy,
    };

    Backend backend = Unsupported;

    bool multipleOperationsPerClient = false;
    bool fixedRequestBody            = false;
    bool sizedStreamRequestBody      = false;
    bool chunkedStreamRequestBody    = false;
    bool redirectPolicy              = false;
    bool protocolHttp11Only          = false;
    bool protocolHttp2Preferred      = false;
    bool protocolHttp2Required       = false;
    bool tlsDisablePeerVerification  = false;
    bool tlsCustomCaPath             = false;
    bool proxyNoProxy                = false;
    bool proxyHttp                   = false;
    bool proxyAuthorization          = false;
    bool proxyBypassList             = false;
    bool contentCodingPolicy         = false;

    [[nodiscard]] bool               hasBackend(Backend requiredBackend) const;
    [[nodiscard]] bool               supports(Feature feature) const;
    [[nodiscard]] bool               supportsRequestOptions(const HttpClientRequestOptions& options) const;
    [[nodiscard]] bool               supportsAll(Span<const Feature> features) const;
    [[nodiscard]] Result             requireBackend(Backend requiredBackend) const;
    [[nodiscard]] Result             requireFeatures(Span<const Feature> features) const;
    [[nodiscard]] Result             requireRequestOptions(const HttpClientRequestOptions& options) const;
    [[nodiscard]] const char*        getBackendName() const;
    [[nodiscard]] static const char* getBackendName(Backend backend);
    [[nodiscard]] static const char* getFeatureName(Feature feature);
};

/// @brief Outgoing request body description
struct SC_HTTP_CLIENT_EXPORT HttpClientRequestBody
{
    /// @brief Transfer framing requested for the outgoing body.
    ///
    /// `FixedSize` uses `bytes`, `SizedStream` uses `provider` plus `sizeInBytes`, and `ChunkedStream`
    /// uses `provider` without a declared size.
    enum Framing : uint8_t
    {
        FixedSize,
        SizedStream,
        ChunkedStream,
    };

    Span<const char>               bytes;
    HttpClientRequestBodyProvider* provider = nullptr;

    uint64_t sizeInBytes = 0; ///< Required for SizedStream, must be zero for ChunkedStream
    bool     canReplay   = false;

    Framing framing = FixedSize;

    [[nodiscard]] bool               isStreamed() const { return framing == SizedStream or framing == ChunkedStream; }
    [[nodiscard]] bool               isChunkedStream() const { return framing == ChunkedStream; }
    [[nodiscard]] const char*        getFramingName() const { return getFramingName(framing); }
    [[nodiscard]] static const char* getFramingName(Framing framing);
    [[nodiscard]] uint64_t           getDeclaredSizeInBytes() const
    {
        if (framing == FixedSize)
        {
            return static_cast<uint64_t>(bytes.sizeInBytes());
        }
        return framing == SizedStream ? sizeInBytes : 0;
    }
};

/// @brief Redirect handling policy for one request
struct SC_HTTP_CLIENT_EXPORT HttpClientRequestRedirectOptions
{
    enum Mode : uint8_t
    {
        NoRedirects,
        FollowGetHead,
        FollowAll,
    };

    Mode    mode         = NoRedirects;
    uint8_t maxRedirects = 10;

    [[nodiscard]] const char*        getModeName() const { return getModeName(mode); }
    [[nodiscard]] static const char* getModeName(Mode mode);
};

/// @brief Timeout policy for one request
struct SC_HTTP_CLIENT_EXPORT HttpClientRequestTimeoutOptions
{
    uint32_t requestTimeoutMs = 30000; ///< Request timeout in milliseconds (0 = no timeout)
};

/// @brief TLS policy for one request
struct SC_HTTP_CLIENT_EXPORT HttpClientRequestTlsOptions
{
    bool       verifyPeer = true;
    StringSpan caCertificatesPath;
};

/// @brief HTTP protocol preference for one request
struct SC_HTTP_CLIENT_EXPORT HttpClientRequestProtocolOptions
{
    enum Preference : uint8_t
    {
        Default,
        Http11Only,
        Http2Preferred,
        Http2Required,
    };

    Preference preference = Default;

    [[nodiscard]] const char*        getPreferenceName() const { return getPreferenceName(preference); }
    [[nodiscard]] static const char* getPreferenceName(Preference preference);
};

/// @brief Proxy policy for one request
struct SC_HTTP_CLIENT_EXPORT HttpClientRequestProxyOptions
{
    enum Mode : uint8_t
    {
        Default, ///< Use the backend default proxy configuration
        NoProxy, ///< Bypass proxies for this request
        Http,    ///< Use `url` as an explicit HTTP proxy URL
    };

    Mode       mode = Default;
    StringSpan url;           ///< Required for `Http`, must use the `http://` scheme
    StringSpan authorization; ///< Optional exact `Proxy-Authorization` header value for `Http`
    StringSpan bypassList;    ///< Optional comma-separated proxy bypass list for `Http`

    [[nodiscard]] const char*        getModeName() const { return getModeName(mode); }
    [[nodiscard]] static const char* getModeName(Mode mode);
};

/// @brief Extended request options grouped by transport concern
struct SC_HTTP_CLIENT_EXPORT HttpClientRequestOptions
{
    HttpClientRequestRedirectOptions redirect;
    HttpClientRequestTimeoutOptions  timeouts;
    HttpClientRequestTlsOptions      tls;
    HttpClientRequestProtocolOptions protocol;
    HttpClientRequestProxyOptions    proxy;
};

/// @brief Configuration for an outgoing HTTP request.
///
/// All views must remain valid until the operation completes or is cancelled. The request object is
/// copied by value, but referenced header/body/provider storage remains caller-owned.
struct SC_HTTP_CLIENT_EXPORT HttpClientRequest
{
    enum Method : uint8_t
    {
        HttpGET,
        HttpPOST,
        HttpPUT,
        HttpHEAD,
        HttpDELETE,
        HttpPATCH,
        HttpOPTIONS,
    };

    Method method = HttpGET;

    StringSpan url; ///< Full URL including scheme (e.g. "https://example.com/path")

    Span<const HttpClientHeader> headers;
    HttpClientRequestBody        body;
    HttpClientRequestOptions     options;

    [[nodiscard]] const char*        getMethodName() const { return getMethodName(method); }
    [[nodiscard]] static const char* getMethodName(Method method);
    [[nodiscard]] Result             validate() const;
};

/// @brief Parsed response metadata filled when headers arrive.
///
/// `headers` and `effectiveUrl` are views into caller-owned `HttpClientOperationMemory` buffers.
struct SC_HTTP_CLIENT_EXPORT HttpClientResponse
{
    enum class Protocol : uint8_t
    {
        Unknown,
        Http11,
        Http2,
    };

    int statusCode = 0; ///< HTTP status code (e.g. 200, 404)

    Span<const char> headers;

    size_t     headersLength      = 0;
    Protocol   negotiatedProtocol = Protocol::Unknown;
    StringSpan effectiveUrl;
    uint32_t   redirectCount = 0;

    [[nodiscard]] bool        getHeader(StringSpan name, StringSpan& value) const;
    [[nodiscard]] bool        hasHeader(StringSpan name) const;
    [[nodiscard]] bool        findNextHeader(StringSpan name, HttpClientResponseHeaderIterator& iterator,
                                             StringSpan& value) const;
    [[nodiscard]] bool        getNextHeader(HttpClientResponseHeaderIterator& iterator, HttpClientHeader& header) const;
    [[nodiscard]] bool        getContentLength(uint64_t& value) const;
    [[nodiscard]] bool        getContentType(StringSpan& value) const;
    [[nodiscard]] bool        getContentEncoding(StringSpan& value) const;
    [[nodiscard]] bool        getTransferEncoding(StringSpan& value) const;
    [[nodiscard]] bool        getLocation(StringSpan& value) const;
    [[nodiscard]] bool        getWwwAuthenticate(StringSpan& value) const;
    [[nodiscard]] bool        getProxyAuthenticate(StringSpan& value) const;
    [[nodiscard]] bool        getNextContentCoding(HttpClientContentCodingIterator& iterator,
                                                   HttpClientContentCoding&         contentCoding) const;
    [[nodiscard]] bool        getNextTransferCoding(HttpClientTransferCodingIterator& iterator,
                                                    HttpClientTransferCoding&         transferCoding) const;
    [[nodiscard]] bool        hasContentCoding(HttpClientContentCoding::Type type) const;
    [[nodiscard]] bool        hasTransferCoding(HttpClientTransferCoding::Type type) const;
    [[nodiscard]] bool        isHttp11() const { return negotiatedProtocol == Protocol::Http11; }
    [[nodiscard]] bool        isHttp2() const { return negotiatedProtocol == Protocol::Http2; }
    [[nodiscard]] bool        isInformationalStatus() const { return statusCode >= 100 and statusCode < 200; }
    [[nodiscard]] bool        isSuccessfulStatus() const { return statusCode >= 200 and statusCode < 300; }
    [[nodiscard]] bool        isRedirectStatus() const { return statusCode >= 300 and statusCode < 400; }
    [[nodiscard]] bool        isClientErrorStatus() const { return statusCode >= 400 and statusCode < 500; }
    [[nodiscard]] bool        isServerErrorStatus() const { return statusCode >= 500 and statusCode < 600; }
    [[nodiscard]] bool        isErrorStatus() const { return statusCode >= 400; }
    [[nodiscard]] const char* getProtocolName() const { return getProtocolName(negotiatedProtocol); }
    [[nodiscard]] static const char* getProtocolName(Protocol protocol);
};

/// @brief Pull-based provider for streamed request bodies
struct SC_HTTP_CLIENT_EXPORT HttpClientRequestBodyProvider
{
    virtual ~HttpClientRequestBodyProvider() {}

    /// @brief Writes the next request body chunk in dest
    /// @param dest Destination span to fill with request body bytes
    /// @param bytesWritten Number of bytes written in dest
    /// @param endReached Set to true once the provider has no more bytes to return
    /// @return `Result(true)` on success, otherwise the error to propagate to the request
    virtual Result pullRequestBody(Span<char> dest, size_t& bytesWritten, bool& endReached) = 0;
};

/// @brief Listener receiving response notifications during HttpClientOperation::poll
struct SC_HTTP_CLIENT_EXPORT HttpClientOperationListener
{
    virtual ~HttpClientOperationListener() {}

    /// @brief Called once the response status code and headers are available
    /// @param response Parsed response metadata owned by the current operation
    virtual void onResponseHead(HttpClientResponse& response) { SC_COMPILER_UNUSED(response); }

    /// @brief Called for each response body chunk delivered by poll()
    /// @param data Body bytes valid for the duration of the callback
    virtual void onResponseBody(Span<const char> data) { SC_COMPILER_UNUSED(data); }

    /// @brief Called when the response body completed successfully
    virtual void onResponseComplete() {}

    /// @brief Called when the request fails
    /// @param error Operation failure
    virtual void onError(Result error) { SC_COMPILER_UNUSED(error); }
};

struct SC_HTTP_CLIENT_EXPORT HttpClientOperation;

/// @brief Optional notifier used by external adapters to wake up their own event loop
struct SC_HTTP_CLIENT_EXPORT HttpClientOperationNotifier
{
    virtual ~HttpClientOperationNotifier() {}

    /// @brief Notifies an external adapter that the operation has queued new events
    /// @param operation Operation that should be polled
    virtual void notifyHttpClientOperation(HttpClientOperation& operation) = 0;
};

/// @brief Caller-owned response buffer descriptor for one HttpClientOperation
struct SC_HTTP_CLIENT_EXPORT HttpClientResponseBuffer
{
    Span<char> data;

  private:
    friend struct HttpClientOperation;
    bool inUse = false;
};

/// @brief Event slot storage used by HttpClientOperation to hand off backend notifications
struct SC_HTTP_CLIENT_EXPORT HttpClientOperationEvent
{
    enum class Type : uint8_t
    {
        ResponseHead,
        ResponseData,
        ResponseComplete,
        Error,
    };

    Type   type        = Type::ResponseHead;
    size_t size        = 0;
    size_t bufferIndex = 0;
    Result error       = Result(true);
};

/// @brief Caller-owned memory for one HttpClientOperation.
///
/// `responseBuffers` and `eventQueue` are required. Either provide non-empty `data` for each
/// response buffer, or provide `responseBufferMemory` to be split equally across them during
/// `HttpClientOperation::init()`. `responseHeaders` stores raw response headers, `responseMetadata`
/// stores transport metadata such as the effective URL, and `backendScratch` is temporary
/// backend-specific conversion/header workspace.
struct SC_HTTP_CLIENT_EXPORT HttpClientOperationMemory
{
    Span<HttpClientResponseBuffer> responseBuffers;
    Span<HttpClientOperationEvent> eventQueue;

    Span<char> responseBufferMemory; ///< Optional; split equally into responseBuffers if non-empty
    Span<char> responseHeaders;
    Span<char> responseMetadata;
    Span<char> backendScratch;
};

/// @brief Reusable HTTP backend/session owner
#if SC_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
struct SC_HTTP_CLIENT_EXPORT HttpClient
{
    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&)            = delete;
    HttpClient(HttpClient&&)                 = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient& operator=(HttpClient&&)      = delete;

    [[nodiscard]] Result init();
    [[nodiscard]] Result init(HttpClientCapabilities::Backend requiredBackend);
    [[nodiscard]] Result init(Span<const HttpClientCapabilities::Feature> requiredFeatures);
    [[nodiscard]] Result init(HttpClientCapabilities::Backend             requiredBackend,
                              Span<const HttpClientCapabilities::Feature> requiredFeatures);
    [[nodiscard]] Result close();

    [[nodiscard]] static HttpClientCapabilities getCapabilities();
    [[nodiscard]] bool                          isInitialized() const { return initialized; }

    /// @brief Convenience helper executing a request synchronously on top of HttpClientOperation::poll
    /// @param request Request metadata
    /// @param response Parsed response metadata
    /// @param bodyBuffer Destination for the response body; exhaustion fails the request instead of truncating silently
    /// @param bodyLength Number of body bytes copied into bodyBuffer, including bytes copied before an overflow error
    /// @param memory Caller-owned operation memory used for the blocking request
    /// @return `Result(true)` on success, otherwise the request error
    [[nodiscard]] static Result executeBlocking(const HttpClientRequest& request, HttpClientResponse& response,
                                                Span<char> bodyBuffer, size_t& bodyLength,
                                                const HttpClientOperationMemory& memory);

    friend struct HttpClientOperation;

  private:
    friend struct Internal;
    friend struct HttpClientLinuxCallbacks;
    struct Internal;

    Result platformInit();
    Result platformClose();

    bool initialized = false;

#if SC_PLATFORM_APPLE
    alignas(uint64_t) char storage[128];
#elif SC_PLATFORM_WINDOWS
    alignas(uint64_t) char storage[128];
#elif SC_PLATFORM_LINUX
    alignas(uint64_t) char storage[512];
#else
    alignas(uint64_t) char storage[8];
#endif
};

/// @brief One in-flight HTTP request/response operation
struct SC_HTTP_CLIENT_EXPORT HttpClientOperation
{
    HttpClientOperation();
    ~HttpClientOperation();

    HttpClientOperation(const HttpClientOperation&)            = delete;
    HttpClientOperation(HttpClientOperation&&)                 = delete;
    HttpClientOperation& operator=(const HttpClientOperation&) = delete;
    HttpClientOperation& operator=(HttpClientOperation&&)      = delete;

    [[nodiscard]] Result init(HttpClient& client, const HttpClientOperationMemory& memory);
    [[nodiscard]] Result close();
    [[nodiscard]] Result cancel();

    /// @brief Starts a new request on this operation
    /// @param request Request metadata
    /// @param response Parsed response metadata storage updated as the request progresses
    /// @param listener Optional poll-driven response listener
    /// @return `Result(true)` on success, otherwise the start error
    [[nodiscard]] Result start(const HttpClientRequest& request, HttpClientResponse& response,
                               HttpClientOperationListener* listener = nullptr);

    /// @brief Processes queued backend events and optionally waits for more work
    /// @param timeoutMilliseconds Maximum time to wait for new events, `0` for non-blocking polling
    /// @return `Result(true)` on success, otherwise the processing error
    [[nodiscard]] Result poll(uint32_t timeoutMilliseconds = 0);

    /// @brief Registers an optional notifier used by adapters such as HttpClientAsyncT
    /// @param notifierValue Notifier receiving wake-up notifications, or `nullptr`
    void setNotifier(HttpClientOperationNotifier* notifierValue) { notifier = notifierValue; }

    [[nodiscard]] bool isInitialized() const { return initialized; }
    [[nodiscard]] bool isRequestInFlight() const { return requestInFlight; }

    friend struct Internal;
    struct Internal;

  private:
    friend struct HttpClientAppleCallbacks;
    friend struct HttpClientLinuxCallbacks;
    friend struct HttpClientWindowsCallbacks;

    Result platformInit();
    Result platformClose();
    Result platformStart();
    Result platformCancel();

    Result enqueueEvent(const HttpClientOperationEvent& event);
    bool   dequeueEvent(HttpClientOperationEvent& event);

    Result allocateResponseBuffer(size_t minimumSizeInBytes, size_t& bufferIndex, Span<char>& data);
    void   releaseResponseBuffer(size_t bufferIndex);
    Result enqueueResponseDataCopy(Span<const char> data);

    void enqueueResponseHead();
    void enqueueResponseBuffer(size_t bufferIndex, size_t size);
    void enqueueResponseComplete();
    void enqueueError(Result error);

    void   resetResponseState(HttpClientResponse& response);
    void   resetRequestBodyState();
    bool   isAutomaticRedirectEnabled() const;
    bool   canAutomaticRedirectRequestReplay() const;
    Result copyResponseEffectiveUrl(StringSpan url);

    size_t readRequestBodyChunk(Span<char> dest, Result& outError, bool& outEnd);
    bool   hasPendingEvents() const;
    Result processPendingEvents();

    HttpClient*                  client          = nullptr;
    HttpClientResponse*          currentResponse = nullptr;
    HttpClientOperationListener* currentListener = nullptr;
    HttpClientOperationNotifier* notifier        = nullptr;
    HttpClientRequest            currentRequest;

    Span<HttpClientResponseBuffer> responseBuffers;
    Span<HttpClientOperationEvent> eventQueue;

    Span<char> responseHeaders;
    Span<char> responseMetadata;
    Span<char> backendScratch;

    mutable HttpClientLocalMutex     eventMutex;
    HttpClientLocalConditionVariable eventCV;
    HttpClientOperationEvent         dequeuedEvent;

    size_t   eventHead            = 0;
    size_t   eventTail            = 0;
    size_t   eventCount           = 0;
    bool     requestBodyFinished  = false;
    Result   requestBodyError     = Result(true);
    uint64_t requestBodyBytesRead = 0;
    bool     initialized          = false;
    bool     requestInFlight      = false;

#if SC_PLATFORM_APPLE
    alignas(uint64_t) char storage[512];
#elif SC_PLATFORM_WINDOWS
    alignas(uint64_t) char storage[256];
#elif SC_PLATFORM_LINUX
    alignas(uint64_t) char storage[512];
#else
    alignas(uint64_t) char storage[8];
#endif
};
#if SC_COMPILER_MSVC
#pragma warning(pop)
#endif

//! @}
} // namespace SC
