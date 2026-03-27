// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Foundation/Result.h"
#include "../Foundation/Span.h"
#include "../Foundation/StringSpan.h"
#include "Internal/HttpClientThreading.h"

namespace SC
{
//! @addtogroup group_http_client
//! @{

/// @brief Configuration for an outgoing HTTP request
struct SC_COMPILER_EXPORT HttpClientRequest
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

    Span<const StringSpan> headerNames;
    Span<const StringSpan> headerValues;

    Span<const char> body; ///< Fixed request body

    uint64_t streamedBodySize = 0; ///< If > 0, caller must provide a HttpClientRequestBodyProvider

    uint32_t timeoutMs      = 30000; ///< Request timeout in milliseconds (0 = no timeout)
    bool     allowRedirects = false;
};

/// @brief Parsed response metadata filled when headers arrive
struct SC_COMPILER_EXPORT HttpClientResponse
{
    enum class Protocol : uint8_t
    {
        Unknown,
        Http11,
        Http2,
    };

    int statusCode = 0; ///< HTTP status code (e.g. 200, 404)

    Span<const char> headers;

    size_t   headersLength      = 0;
    Protocol negotiatedProtocol = Protocol::Unknown;

    [[nodiscard]] bool getHeader(StringSpan name, StringSpan& value) const;
};

/// @brief Pull-based provider for streamed request bodies
struct SC_COMPILER_EXPORT HttpClientRequestBodyProvider
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
struct SC_COMPILER_EXPORT HttpClientOperationListener{
    virtual ~HttpClientOperationListener(){}

    /// @brief Called once the response status code and headers are available
    /// @param response Parsed response metadata owned by the current operation
    virtual void onResponseHead(HttpClientResponse & response){SC_COMPILER_UNUSED(response);
} // namespace SC

/// @brief Called for each response body chunk delivered by poll()
/// @param data Body bytes valid for the duration of the callback
virtual void onResponseBody(Span<const char> data) { SC_COMPILER_UNUSED(data); }

/// @brief Called when the response body completed successfully
virtual void onResponseComplete() {}

/// @brief Called when the request fails
/// @param error Operation failure
virtual void onError(Result error) { SC_COMPILER_UNUSED(error); }
}
;

struct SC_COMPILER_EXPORT HttpClientOperation;

/// @brief Optional notifier used by external adapters to wake up their own event loop
struct SC_COMPILER_EXPORT HttpClientOperationNotifier
{
    virtual ~HttpClientOperationNotifier() {}

    /// @brief Notifies an external adapter that the operation has queued new events
    /// @param operation Operation that should be polled
    virtual void notifyHttpClientOperation(HttpClientOperation& operation) = 0;
};

/// @brief Caller-owned response buffer descriptor for one HttpClientOperation
struct SC_COMPILER_EXPORT HttpClientResponseBuffer
{
    Span<char> data;

  private:
    friend struct HttpClientOperation;
    bool inUse = false;
};

/// @brief Event slot storage used by HttpClientOperation to hand off backend notifications
struct SC_COMPILER_EXPORT HttpClientOperationEvent
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

/// @brief Caller-owned memory for one HttpClientOperation
struct SC_COMPILER_EXPORT HttpClientOperationMemory
{
    Span<HttpClientResponseBuffer> responseBuffers;
    Span<HttpClientOperationEvent> eventQueue;

    Span<char> responseBufferMemory; ///< Optional; split equally into responseBuffers if non-empty
    Span<char> responseHeaders;
    Span<char> backendScratch;
};

/// @brief Reusable HTTP backend/session owner
#if SC_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
struct SC_COMPILER_EXPORT HttpClient
{
    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&)            = delete;
    HttpClient(HttpClient&&)                 = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient& operator=(HttpClient&&)      = delete;

    [[nodiscard]] Result init();
    [[nodiscard]] Result close();

    [[nodiscard]] bool isInitialized() const { return initialized; }

    /// @brief Convenience helper executing a request synchronously on top of HttpClientOperation::poll
    /// @param request Request metadata
    /// @param response Parsed response metadata
    /// @param bodyBuffer Destination for the response body
    /// @param bodyLength Number of body bytes copied into bodyBuffer
    /// @param memory Caller-owned operation memory used for the blocking request
    /// @return `Result(true)` on success, otherwise the request error
    [[nodiscard]] static Result executeBlocking(const HttpClientRequest& request, HttpClientResponse& response,
                                                Span<char> bodyBuffer, size_t& bodyLength,
                                                const HttpClientOperationMemory& memory);

    friend struct HttpClientOperation;

  private:
    friend struct Internal;
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
struct SC_COMPILER_EXPORT HttpClientOperation
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
    /// @param bodyProvider Optional request body provider for streamed uploads
    /// @return `Result(true)` on success, otherwise the start error
    [[nodiscard]] Result start(const HttpClientRequest& request, HttpClientResponse& response,
                               HttpClientOperationListener*   listener     = nullptr,
                               HttpClientRequestBodyProvider* bodyProvider = nullptr);

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

    void resetResponseState(HttpClientResponse& response);
    void resetRequestBodyState();

    size_t readRequestBodyChunk(Span<char> dest, Result& outError, bool& outEnd);
    bool   hasPendingEvents() const;
    Result processPendingEvents();

    HttpClient*                    client              = nullptr;
    HttpClientResponse*            currentResponse     = nullptr;
    HttpClientOperationListener*   currentListener     = nullptr;
    HttpClientRequestBodyProvider* currentBodyProvider = nullptr;
    HttpClientOperationNotifier*   notifier            = nullptr;
    HttpClientRequest              currentRequest;

    Span<HttpClientResponseBuffer> responseBuffers;
    Span<HttpClientOperationEvent> eventQueue;

    Span<char> responseHeaders;
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
