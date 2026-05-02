// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpClient.h"

#include <string.h>

#if SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
#include <errno.h>
#include <pthread.h>
#include <time.h>
#endif

#if SC_PLATFORM_APPLE
#include "Internal/HttpClientApple.inl"
#elif SC_PLATFORM_WINDOWS
#include "Internal/HttpClientWindows.inl"
#elif SC_PLATFORM_LINUX
#include "Internal/HttpClientLinux.inl"
#else
SC::HttpClient::HttpClient() {}
SC::HttpClient::~HttpClient() {}
SC::Result SC::HttpClient::platformInit() { return Result::Error("HttpClient: unsupported platform"); }
SC::Result SC::HttpClient::platformClose() { return Result(true); }

SC::HttpClientOperation::HttpClientOperation() {}
SC::HttpClientOperation::~HttpClientOperation() {}
SC::Result SC::HttpClientOperation::platformInit() { return Result::Error("HttpClient: unsupported platform"); }
SC::Result SC::HttpClientOperation::platformClose() { return Result(true); }
SC::Result SC::HttpClientOperation::platformStart() { return Result::Error("HttpClient: unsupported platform"); }
SC::Result SC::HttpClientOperation::platformCancel() { return Result(true); }
#endif

namespace
{
static char asciiLower(char c)
{
    if (c >= 'A' and c <= 'Z')
    {
        return static_cast<char>(c - 'A' + 'a');
    }
    return c;
}

static bool asciiEqualsIgnoreCase(SC::StringSpan left, SC::StringSpan right)
{
    if (left.sizeInBytes() != right.sizeInBytes())
    {
        return false;
    }
    const SC::Span<const char> leftBytes  = left.toCharSpan();
    const SC::Span<const char> rightBytes = right.toCharSpan();
    for (size_t idx = 0; idx < leftBytes.sizeInBytes(); ++idx)
    {
        if (asciiLower(leftBytes[idx]) != asciiLower(rightBytes[idx]))
        {
            return false;
        }
    }
    return true;
}

static bool isAsciiWhiteSpace(char c) { return c == ' ' or c == '\t' or c == '\r' or c == '\n'; }

static SC::StringSpan trimAsciiWhiteSpace(SC::Span<const char> span)
{
    size_t start = 0;
    size_t end   = span.sizeInBytes();
    while (start < end and isAsciiWhiteSpace(span[start]))
    {
        start += 1;
    }
    while (end > start and isAsciiWhiteSpace(span[end - 1]))
    {
        end -= 1;
    }
    return {{span.data() + start, end - start}, false, SC::StringEncoding::Ascii};
}

static SC::Result sliceResponseBuffers(SC::Span<SC::HttpClientResponseBuffer> buffers, SC::Span<char> memory)
{
    SC_TRY_MSG(not buffers.empty(), "HttpClientOperation: response buffers missing");
    SC_TRY_MSG(memory.sizeInBytes() >= buffers.sizeInElements(),
               "HttpClientOperation: response buffer memory too small for slicing");

    const size_t sliceSize = memory.sizeInBytes() / buffers.sizeInElements();
    SC_TRY_MSG(sliceSize > 0, "HttpClientOperation: response buffer slices would be empty");

    for (size_t idx = 0; idx < buffers.sizeInElements(); ++idx)
    {
        buffers[idx].data = {memory.data() + (idx * sliceSize),
                             idx + 1 == buffers.sizeInElements() ? memory.sizeInBytes() - idx * sliceSize : sliceSize};
    }
    return SC::Result(true);
}
} // namespace

namespace
{
#if SC_PLATFORM_WINDOWS
using HttpClientLocalMutexStorage             = CRITICAL_SECTION;
using HttpClientLocalConditionVariableStorage = CONDITION_VARIABLE;
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
using HttpClientLocalMutexStorage             = pthread_mutex_t;
using HttpClientLocalConditionVariableStorage = pthread_cond_t;

static timespec httpClientMakeAbsoluteTimeout(SC::uint32_t timeoutMilliseconds)
{
    timespec deadline = {};
    (void)clock_gettime(CLOCK_REALTIME, &deadline);

    deadline.tv_sec += static_cast<time_t>(timeoutMilliseconds / 1000);
    deadline.tv_nsec += static_cast<long>((timeoutMilliseconds % 1000) * 1000000u);
    if (deadline.tv_nsec >= 1000000000L)
    {
        deadline.tv_sec += 1;
        deadline.tv_nsec -= 1000000000L;
    }
    return deadline;
}
#else
struct HttpClientLocalMutexStorage
{
    char value;
};

struct HttpClientLocalConditionVariableStorage
{
    char value;
};
#endif
} // namespace

SC::HttpClientLocalMutex::HttpClientLocalMutex()
{
    static_assert(sizeof(storage) >= sizeof(HttpClientLocalMutexStorage), "HttpClient mutex storage too small");
    static_assert(alignof(HttpClientLocalMutex) >= alignof(HttpClientLocalMutexStorage),
                  "HttpClient mutex alignment mismatch");

#if SC_PLATFORM_WINDOWS
    InitializeCriticalSection(reinterpret_cast<HttpClientLocalMutexStorage*>(storage));
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    (void)pthread_mutex_init(reinterpret_cast<HttpClientLocalMutexStorage*>(storage), nullptr);
#endif
}

SC::HttpClientLocalMutex::~HttpClientLocalMutex()
{
#if SC_PLATFORM_WINDOWS
    DeleteCriticalSection(reinterpret_cast<HttpClientLocalMutexStorage*>(storage));
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    (void)pthread_mutex_destroy(reinterpret_cast<HttpClientLocalMutexStorage*>(storage));
#endif
}

void SC::HttpClientLocalMutex::lock()
{
#if SC_PLATFORM_WINDOWS
    EnterCriticalSection(reinterpret_cast<HttpClientLocalMutexStorage*>(storage));
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    (void)pthread_mutex_lock(reinterpret_cast<HttpClientLocalMutexStorage*>(storage));
#endif
}

void SC::HttpClientLocalMutex::unlock()
{
#if SC_PLATFORM_WINDOWS
    LeaveCriticalSection(reinterpret_cast<HttpClientLocalMutexStorage*>(storage));
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    (void)pthread_mutex_unlock(reinterpret_cast<HttpClientLocalMutexStorage*>(storage));
#endif
}

SC::HttpClientLocalConditionVariable::HttpClientLocalConditionVariable()
{
    static_assert(sizeof(storage) >= sizeof(HttpClientLocalConditionVariableStorage),
                  "HttpClient condition variable storage too small");
    static_assert(alignof(HttpClientLocalConditionVariable) >= alignof(HttpClientLocalConditionVariableStorage),
                  "HttpClient condition variable alignment mismatch");

#if SC_PLATFORM_WINDOWS
    InitializeConditionVariable(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage));
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    (void)pthread_cond_init(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage), nullptr);
#endif
}

SC::HttpClientLocalConditionVariable::~HttpClientLocalConditionVariable()
{
#if SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    (void)pthread_cond_destroy(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage));
#endif
}

void SC::HttpClientLocalConditionVariable::wait(HttpClientLocalMutex& mutex)
{
#if SC_PLATFORM_WINDOWS
    (void)SleepConditionVariableCS(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage),
                                   reinterpret_cast<HttpClientLocalMutexStorage*>(mutex.storage), INFINITE);
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    (void)pthread_cond_wait(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage),
                            reinterpret_cast<HttpClientLocalMutexStorage*>(mutex.storage));
#endif
}

bool SC::HttpClientLocalConditionVariable::wait(HttpClientLocalMutex& mutex, uint32_t timeoutMilliseconds)
{
#if SC_PLATFORM_WINDOWS
    return SleepConditionVariableCS(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage),
                                    reinterpret_cast<HttpClientLocalMutexStorage*>(mutex.storage),
                                    timeoutMilliseconds) != 0;
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    const timespec deadline = httpClientMakeAbsoluteTimeout(timeoutMilliseconds);
    return pthread_cond_timedwait(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage),
                                  reinterpret_cast<HttpClientLocalMutexStorage*>(mutex.storage),
                                  &deadline) != ETIMEDOUT;
#else
    SC_COMPILER_UNUSED(mutex);
    SC_COMPILER_UNUSED(timeoutMilliseconds);
    return true;
#endif
}

void SC::HttpClientLocalConditionVariable::signal()
{
#if SC_PLATFORM_WINDOWS
    WakeConditionVariable(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage));
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    (void)pthread_cond_signal(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage));
#endif
}

void SC::HttpClientLocalConditionVariable::broadcast()
{
#if SC_PLATFORM_WINDOWS
    WakeAllConditionVariable(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage));
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    (void)pthread_cond_broadcast(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage));
#endif
}

bool SC::HttpClientResponse::getHeader(StringSpan name, StringSpan& value) const
{
    const Span<const char> allHeaders = {headers.data(), headersLength};

    value = {};

    size_t offset = 0;
    while (offset < allHeaders.sizeInBytes())
    {
        size_t lineEnd = offset;
        while (lineEnd < allHeaders.sizeInBytes())
        {
            if (allHeaders[lineEnd] == '\r' and (lineEnd + 1) < allHeaders.sizeInBytes() and
                allHeaders[lineEnd + 1] == '\n')
            {
                break;
            }
            lineEnd += 1;
        }

        const Span<const char> line = {allHeaders.data() + offset, lineEnd - offset};
        offset                      = lineEnd;
        if ((offset + 1) < allHeaders.sizeInBytes() and allHeaders[offset] == '\r' and allHeaders[offset + 1] == '\n')
        {
            offset += 2;
        }

        if (line.sizeInBytes() == 0)
        {
            continue;
        }

        size_t separator = 0;
        while (separator < line.sizeInBytes() and line[separator] != ':')
        {
            separator += 1;
        }
        if (separator < line.sizeInBytes())
        {
            const StringSpan headerName = trimAsciiWhiteSpace({line.data(), separator});
            const StringSpan headerValue =
                trimAsciiWhiteSpace({line.data() + separator + 1, line.sizeInBytes() - separator - 1});
            if (asciiEqualsIgnoreCase(headerName, name))
            {
                value = headerValue;
                return true;
            }
        }
    }
    return false;
}

SC::Result SC::HttpClient::init()
{
    SC_TRY_MSG(not initialized, "HttpClient: already initialized");
    SC_TRY(platformInit());
    initialized = true;
    return Result(true);
}

SC::Result SC::HttpClient::close()
{
    if (not initialized)
    {
        return Result(true);
    }
    SC_TRY(platformClose());
    initialized = false;
    return Result(true);
}

SC::Result SC::HttpClientOperation::init(HttpClient& clientValue, const HttpClientOperationMemory& memory)
{
    SC_TRY_MSG(clientValue.isInitialized(), "HttpClientOperation: client not initialized");
    SC_TRY_MSG(not initialized, "HttpClientOperation: already initialized");
    SC_TRY_MSG(memory.responseBuffers.sizeInElements() > 0, "HttpClientOperation: response buffers missing");
    SC_TRY_MSG(memory.eventQueue.sizeInElements() > 0, "HttpClientOperation: event queue missing");
    SC_TRY_MSG(memory.responseHeaders.sizeInBytes() > 0, "HttpClientOperation: response headers buffer missing");
    SC_TRY_MSG(memory.responseMetadata.sizeInBytes() > 0, "HttpClientOperation: response metadata buffer missing");

    client           = &clientValue;
    responseBuffers  = memory.responseBuffers;
    eventQueue       = memory.eventQueue;
    responseHeaders  = memory.responseHeaders;
    responseMetadata = memory.responseMetadata;
    backendScratch   = memory.backendScratch;

    if (memory.responseBufferMemory.sizeInBytes() > 0)
    {
        SC_TRY(sliceResponseBuffers(responseBuffers, memory.responseBufferMemory));
    }
    else
    {
        for (HttpClientResponseBuffer& buffer : responseBuffers)
        {
            SC_TRY_MSG(buffer.data.sizeInBytes() > 0, "HttpClientOperation: response buffer is empty");
            buffer.inUse = false;
        }
    }

    eventHead            = 0;
    eventTail            = 0;
    eventCount           = 0;
    requestBodyError     = Result(true);
    requestBodyBytesRead = 0;
    requestBodyFinished  = false;

    SC_TRY(platformInit());
    initialized = true;
    return Result(true);
}

SC::Result SC::HttpClientOperation::close()
{
    if (not initialized)
    {
        return Result(true);
    }

    if (requestInFlight)
    {
        (void)platformCancel();
    }

    SC_TRY(platformClose());

    client          = nullptr;
    currentResponse = nullptr;
    currentListener = nullptr;
    currentRequest  = {};
    notifier        = nullptr;

    eventMutex.lock();
    eventHead  = 0;
    eventTail  = 0;
    eventCount = 0;
    for (HttpClientResponseBuffer& buffer : responseBuffers)
    {
        buffer.inUse = false;
    }
    eventMutex.unlock();

    requestInFlight      = false;
    requestBodyFinished  = false;
    requestBodyError     = Result(true);
    requestBodyBytesRead = 0;
    initialized          = false;
    return Result(true);
}

SC::Result SC::HttpClientOperation::cancel()
{
    SC_TRY_MSG(initialized, "HttpClientOperation: not initialized");
    if (not requestInFlight)
    {
        return Result(true);
    }
    return platformCancel();
}

SC::Result SC::HttpClientOperation::start(const HttpClientRequest& request, HttpClientResponse& response,
                                          HttpClientOperationListener* listener)
{
    SC_TRY_MSG(initialized, "HttpClientOperation: not initialized");
    SC_TRY_MSG(not requestInFlight, "HttpClientOperation: request already in flight");
    SC_TRY_MSG(request.url.sizeInBytes() > 0, "HttpClientOperation: URL is empty");
    SC_TRY_MSG(not(request.body.isStreamed() and request.body.bytes.sizeInBytes() > 0),
               "HttpClientOperation: inline and streamed request body both set");
    if (not request.body.isStreamed())
    {
        SC_TRY_MSG(request.body.sizeInBytes == 0 or request.body.sizeInBytes == request.body.bytes.sizeInBytes(),
                   "HttpClientOperation: inline request body size mismatch");
    }
    else
    {
        SC_TRY_MSG(request.body.provider != nullptr, "HttpClientOperation: streamed request body requires provider");
    }

    if (request.options.redirect.mode == HttpClientRequestRedirectOptions::FollowGetHead)
    {
        SC_TRY_MSG(request.method == HttpClientRequest::HttpGET or request.method == HttpClientRequest::HttpHEAD,
                   "HttpClientOperation: FollowGetHead requires GET or HEAD");
    }
    if (request.options.redirect.mode != HttpClientRequestRedirectOptions::NoRedirects)
    {
        SC_TRY_MSG((not request.body.isStreamed()) or request.body.canReplay,
                   "HttpClientOperation: automatic redirects require a replayable request body");
    }
    SC_TRY_MSG(request.options.protocol.preference == HttpClientRequestProtocolOptions::Default,
               "HttpClientOperation: protocol preference not yet supported");

    resetResponseState(response);
    resetRequestBodyState();

    currentRequest  = request;
    currentResponse = &response;
    currentListener = listener;
    requestInFlight = true;
    SC_TRY(copyResponseEffectiveUrl(currentRequest.url));

    return platformStart();
}

SC::Result SC::HttpClientOperation::enqueueEvent(const HttpClientOperationEvent& event)
{
    eventMutex.lock();
    while (eventCount >= eventQueue.sizeInElements())
    {
        eventCV.wait(eventMutex);
    }
    eventQueue[eventTail] = event;
    eventTail             = (eventTail + 1) % eventQueue.sizeInElements();
    eventCount += 1;
    eventCV.signal();
    eventMutex.unlock();

    if (notifier != nullptr)
    {
        notifier->notifyHttpClientOperation(*this);
    }
    return Result(true);
}

bool SC::HttpClientOperation::dequeueEvent(HttpClientOperationEvent& event)
{
    eventMutex.lock();
    if (eventCount == 0)
    {
        eventMutex.unlock();
        return false;
    }
    event     = eventQueue[eventHead];
    eventHead = (eventHead + 1) % eventQueue.sizeInElements();
    eventCount -= 1;
    eventCV.signal();
    eventMutex.unlock();
    return true;
}

bool SC::HttpClientOperation::hasPendingEvents() const
{
    eventMutex.lock();
    const bool pending = eventCount > 0;
    eventMutex.unlock();
    return pending;
}

SC::Result SC::HttpClientOperation::allocateResponseBuffer(size_t minimumSizeInBytes, size_t& bufferIndex,
                                                           Span<char>& data)
{
    eventMutex.lock();
    bool supportsSize = false;
    for (const HttpClientResponseBuffer& buffer : responseBuffers)
    {
        if (buffer.data.sizeInBytes() >= minimumSizeInBytes)
        {
            supportsSize = true;
            break;
        }
    }
    if (not supportsSize)
    {
        eventMutex.unlock();
        return Result::Error("HttpClient: response buffer exhausted");
    }

    while (requestInFlight)
    {
        for (size_t idx = 0; idx < responseBuffers.sizeInElements(); ++idx)
        {
            HttpClientResponseBuffer& buffer = responseBuffers[idx];
            if (not buffer.inUse and buffer.data.sizeInBytes() >= minimumSizeInBytes)
            {
                buffer.inUse = true;
                bufferIndex  = idx;
                data         = buffer.data;
                eventMutex.unlock();
                return Result(true);
            }
        }
        eventCV.wait(eventMutex);
    }

    eventMutex.unlock();
    return Result::Error("HttpClient: request cancelled");
}

void SC::HttpClientOperation::releaseResponseBuffer(size_t bufferIndex)
{
    eventMutex.lock();
    if (bufferIndex < responseBuffers.sizeInElements())
    {
        responseBuffers[bufferIndex].inUse = false;
        eventCV.signal();
    }
    eventMutex.unlock();
}

SC::Result SC::HttpClientOperation::enqueueResponseDataCopy(Span<const char> data)
{
    size_t offset = 0;
    while (offset < data.sizeInBytes())
    {
        size_t     bufferIndex = 0;
        Span<char> writable;
        SC_TRY(allocateResponseBuffer(1, bufferIndex, writable));

        const size_t remaining = data.sizeInBytes() - offset;
        const size_t toCopy    = writable.sizeInBytes() < remaining ? writable.sizeInBytes() : remaining;
        memcpy(writable.data(), data.data() + offset, toCopy);
        enqueueResponseBuffer(bufferIndex, toCopy);
        offset += toCopy;
    }
    return Result(true);
}

void SC::HttpClientOperation::enqueueResponseHead()
{
    HttpClientOperationEvent event;
    event.type = HttpClientOperationEvent::Type::ResponseHead;
    (void)enqueueEvent(event);
}

void SC::HttpClientOperation::enqueueResponseBuffer(size_t bufferIndex, size_t size)
{
    HttpClientOperationEvent event;
    event.type        = HttpClientOperationEvent::Type::ResponseData;
    event.bufferIndex = bufferIndex;
    event.size        = size;
    (void)enqueueEvent(event);
}

void SC::HttpClientOperation::enqueueResponseComplete()
{
    HttpClientOperationEvent event;
    event.type = HttpClientOperationEvent::Type::ResponseComplete;
    (void)enqueueEvent(event);
}

void SC::HttpClientOperation::enqueueError(Result error)
{
    HttpClientOperationEvent event;
    event.type  = HttpClientOperationEvent::Type::Error;
    event.error = error;
    (void)enqueueEvent(event);
}

SC::Result SC::HttpClientOperation::processPendingEvents()
{
    HttpClientOperationEvent event;
    while (dequeueEvent(event))
    {
        switch (event.type)
        {
        case HttpClientOperationEvent::Type::ResponseHead:
            if (currentResponse != nullptr and currentListener != nullptr)
            {
                currentListener->onResponseHead(*currentResponse);
            }
            break;
        case HttpClientOperationEvent::Type::ResponseData: {
            if (event.bufferIndex >= responseBuffers.sizeInElements())
            {
                return Result::Error("HttpClient: invalid response buffer index");
            }
            const Span<char>       writable = responseBuffers[event.bufferIndex].data;
            const Span<const char> readable = {writable.data(), event.size};
            if (currentListener != nullptr)
            {
                currentListener->onResponseBody(readable);
            }
            releaseResponseBuffer(event.bufferIndex);
            break;
        }
        case HttpClientOperationEvent::Type::ResponseComplete:
            requestInFlight = false;
            eventMutex.lock();
            eventCV.signal();
            eventMutex.unlock();
            if (currentListener != nullptr)
            {
                currentListener->onResponseComplete();
            }
            resetRequestBodyState();
            break;
        case HttpClientOperationEvent::Type::Error:
            requestInFlight = false;
            eventMutex.lock();
            eventCV.signal();
            eventMutex.unlock();
            if (currentListener != nullptr)
            {
                currentListener->onError(event.error);
            }
            resetRequestBodyState();
            break;
        }
    }
    return Result(true);
}

SC::Result SC::HttpClientOperation::poll(uint32_t timeoutMilliseconds)
{
    SC_TRY_MSG(initialized, "HttpClientOperation: not initialized");

    SC_TRY(processPendingEvents());
    if (timeoutMilliseconds == 0 or not requestInFlight)
    {
        return Result(true);
    }

    eventMutex.lock();
    while (requestInFlight and eventCount == 0)
    {
        if (not eventCV.wait(eventMutex, timeoutMilliseconds))
        {
            eventMutex.unlock();
            return Result(true);
        }
    }
    eventMutex.unlock();

    return processPendingEvents();
}

void SC::HttpClientOperation::resetResponseState(HttpClientResponse& response)
{
    response.statusCode         = 0;
    response.headers            = responseHeaders;
    response.headersLength      = 0;
    response.negotiatedProtocol = HttpClientResponse::Protocol::Unknown;
    response.effectiveUrl       = {};
    response.redirectCount      = 0;
}

void SC::HttpClientOperation::resetRequestBodyState()
{
    requestBodyFinished  = false;
    requestBodyError     = Result(true);
    requestBodyBytesRead = 0;
}

bool SC::HttpClientOperation::isAutomaticRedirectEnabled() const
{
    return currentRequest.options.redirect.mode != HttpClientRequestRedirectOptions::NoRedirects;
}

bool SC::HttpClientOperation::canAutomaticRedirectRequestReplay() const
{
    if (currentRequest.body.isStreamed())
    {
        return currentRequest.body.canReplay;
    }
    return true;
}

SC::Result SC::HttpClientOperation::copyResponseEffectiveUrl(StringSpan url)
{
    SC_TRY_MSG(currentResponse != nullptr, "HttpClientOperation: missing response");
    SC_TRY_MSG(responseMetadata.sizeInBytes() >= url.sizeInBytes(), "HttpClient: response metadata buffer too small");

    if (url.sizeInBytes() > 0)
    {
        memcpy(responseMetadata.data(), url.toCharSpan().data(), url.sizeInBytes());
    }
    currentResponse->effectiveUrl = StringSpan({responseMetadata.data(), url.sizeInBytes()}, false, url.getEncoding());
    return Result(true);
}

size_t SC::HttpClientOperation::readRequestBodyChunk(Span<char> dest, Result& outError, bool& outEnd)
{
    outError = Result(true);
    outEnd   = false;

    if (not currentRequest.body.isStreamed())
    {
        outEnd = true;
        return 0;
    }

    if (requestBodyFinished)
    {
        outEnd = true;
        return 0;
    }

    if (currentRequest.body.provider == nullptr)
    {
        outError = Result::Error("HttpClientOperation: missing request body provider");
        return 0;
    }

    size_t bytesWritten = 0;
    bool   endReached   = false;

    SC_TRY_MSG(dest.sizeInBytes() > 0, "HttpClientOperation: request body destination is empty");

    outError = currentRequest.body.provider->pullRequestBody(dest, bytesWritten, endReached);
    if (not outError)
    {
        return 0;
    }

    if (bytesWritten > dest.sizeInBytes())
    {
        outError = Result::Error("HttpClientOperation: request body provider overflowed destination span");
        return 0;
    }

    if (bytesWritten == 0 and not endReached)
    {
        outError = Result::Error("HttpClientOperation: request body provider returned zero bytes without ending");
        return 0;
    }

    requestBodyBytesRead += bytesWritten;
    if (requestBodyBytesRead > currentRequest.body.sizeInBytes)
    {
        outError = Result::Error("HttpClientOperation: streamed body exceeded declared size");
        return 0;
    }

    if (endReached)
    {
        requestBodyFinished = true;
        if (requestBodyBytesRead != currentRequest.body.sizeInBytes)
        {
            outError = Result::Error("HttpClientOperation: streamed body ended before expected size");
            return 0;
        }
        if (bytesWritten == 0)
        {
            outEnd = true;
        }
    }

    return bytesWritten;
}

SC::Result SC::HttpClient::executeBlocking(const HttpClientRequest& request, HttpClientResponse& response,
                                           Span<char> bodyBuffer, size_t& bodyLength,
                                           const HttpClientOperationMemory& memory)
{
    bodyLength = 0;

    HttpClient client;
    SC_TRY(client.init());

    struct BlockingListener final : public HttpClientOperationListener
    {
        Span<char> bodyBuffer;
        size_t*    bodyLength = nullptr;
        bool*      completed  = nullptr;
        Result*    finalRes   = nullptr;

        virtual void onResponseBody(Span<const char> data) override
        {
            const size_t remaining = bodyBuffer.sizeInBytes() - *bodyLength;
            const size_t toCopy    = data.sizeInBytes() < remaining ? data.sizeInBytes() : remaining;
            if (toCopy > 0)
            {
                memcpy(bodyBuffer.data() + *bodyLength, data.data(), toCopy);
                *bodyLength += toCopy;
            }
        }

        virtual void onResponseComplete() override { *completed = true; }

        virtual void onError(Result error) override
        {
            *completed = true;
            *finalRes  = error;
        }
    };

    HttpClientOperation operation;
    SC_TRY(operation.init(client, memory));

    bool   completed = false;
    Result finalRes(true);

    BlockingListener listener;
    listener.bodyBuffer = bodyBuffer;
    listener.bodyLength = &bodyLength;
    listener.completed  = &completed;
    listener.finalRes   = &finalRes;

    SC_TRY(operation.start(request, response, &listener));

    while (not completed and operation.isRequestInFlight())
    {
        SC_TRY(operation.poll(50));
    }

    SC_TRY(operation.poll());
    SC_TRY(operation.close());
    SC_TRY(client.close());
    return finalRes;
}
