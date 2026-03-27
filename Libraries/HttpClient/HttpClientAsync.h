// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../HttpClient/HttpClient.h"
#include "../HttpClient/Internal/HttpClientEvent.h"
#include "../HttpClient/Internal/HttpClientThreading.h"

#include <string.h>

namespace SC
{
//! @addtogroup group_http_client
//! @{

/// @brief Caller-owned memory for one HttpClientAsyncT operation.
/// @tparam T_AsyncStreams Traits exposing the AsyncStreams types used by the adapter.
template <typename T_AsyncStreams>
struct SC_HTTP_CLIENT_EXPORT HttpClientAsyncOperationMemoryT
{
    using T_AsyncBufferView      = typename T_AsyncStreams::BufferView;
    using T_AsyncReadableStream  = typename T_AsyncStreams::ReadableStream;
    using T_AsyncWritableStream  = typename T_AsyncStreams::WritableStream;
    using T_AsyncReadableRequest = typename T_AsyncReadableStream::Request;
    using T_AsyncWritableRequest = typename T_AsyncWritableStream::Request;

    Span<T_AsyncBufferView>      responseBuffers;
    Span<char>                   responseBufferMemory;
    Span<T_AsyncReadableRequest> responseReadQueue;
    Span<T_AsyncWritableRequest> requestWriteQueue;
};

template <typename T_AsyncEventLoop, typename T_AsyncStreams>
struct HttpClientAsyncT final : private HttpClientOperationListener,
                                private HttpClientRequestBodyProvider,
                                private HttpClientOperationNotifier
{
    using T_AsyncLoopWakeUp      = typename T_AsyncEventLoop::LoopWakeUp;
    using T_AsyncBufferView      = typename T_AsyncStreams::BufferView;
    using T_AsyncBuffersPool     = typename T_AsyncStreams::BuffersPool;
    using T_AsyncReadableStream  = typename T_AsyncStreams::ReadableStream;
    using T_AsyncWritableStream  = typename T_AsyncStreams::WritableStream;
    using T_AsyncReadableRequest = typename T_AsyncReadableStream::Request;
    using T_AsyncWritableRequest = typename T_AsyncWritableStream::Request;

    static constexpr int MaxListeners = 8;

    struct ResponseBodyStream final : public T_AsyncReadableStream
    {
        virtual Result asyncRead() override { return Result(true); }
    };

    struct RequestBodySink final : public T_AsyncWritableStream
    {
        Result init(T_AsyncBuffersPool& buffersPool, Span<T_AsyncWritableRequest> writeQueueSpan,
                    HttpClientAsyncT& ownerValue)
        {
            owner = &ownerValue;
            this->setWriteQueue(writeQueueSpan);
            return T_AsyncWritableStream::init(buffersPool);
        }

      private:
        virtual Result asyncWrite(typename T_AsyncBufferView::ID                 bufferID,
                                  Function<void(typename T_AsyncBufferView::ID)> cb) override
        {
            SC_TRY_MSG(owner != nullptr, "HttpClientAsyncT::RequestBodySink missing owner");
            owner->onRequestBodyWritableBuffer(bufferID, move(cb));
            return Result(true);
        }

        virtual Result asyncDestroyWritable() override
        {
            if (owner != nullptr and owner->operation.isRequestInFlight() and not owner->requestBodyFinished)
            {
                owner->onRequestBodyWritableError(Result::Error("HttpClientAsyncT: request body sink destroyed"));
            }
            T_AsyncWritableStream::finishedDestroyingWritable();
            return Result(true);
        }

        virtual bool canEndWritable() override
        {
            if (owner != nullptr)
            {
                owner->onRequestBodyWritableFinished();
            }
            return true;
        }

        HttpClientAsyncT* owner = nullptr;
    };

    HttpClientEvent<MaxListeners, HttpClientResponse&> eventResponseHead;

    Result init(HttpClient& client, T_AsyncEventLoop& loop, const HttpClientOperationMemory& operationMemory,
                const HttpClientAsyncOperationMemoryT<T_AsyncStreams>& asyncMemory)
    {
        SC_TRY_MSG(eventLoop == nullptr, "HttpClientAsyncT: already initialized");
        SC_TRY_MSG(asyncMemory.responseBuffers.sizeInElements() > 0, "HttpClientAsyncT: response buffers missing");
        SC_TRY_MSG(asyncMemory.responseReadQueue.sizeInElements() > 0, "HttpClientAsyncT: response read queue missing");
        SC_TRY_MSG(asyncMemory.requestWriteQueue.sizeInElements() > 0, "HttpClientAsyncT: request write queue missing");

        eventLoop         = &loop;
        responseReadQueue = asyncMemory.responseReadQueue;
        requestWriteQueue = asyncMemory.requestWriteQueue;

        responseBuffersPool.setBuffers(asyncMemory.responseBuffers);
        if (asyncMemory.responseBufferMemory.sizeInBytes() > 0)
        {
            SC_TRY(T_AsyncBuffersPool::sliceInEqualParts(asyncMemory.responseBuffers, asyncMemory.responseBufferMemory,
                                                         asyncMemory.responseBuffers.sizeInElements()));
        }

        responseBodyStream.setReadQueue(responseReadQueue);
        responseBodyStream.setAutoDestroy(false);
        requestBodySink.setAutoDestroy(false);
        wakeUp.callback = [this](typename T_AsyncLoopWakeUp::Result& result) { onWakeUp(result); };

        SC_TRY(operation.init(client, operationMemory));
        operation.setNotifier(this);
        initialized = true;
        return Result(true);
    }

    Result close()
    {
        if (not initialized)
        {
            return Result(true);
        }

        if (wakeUpStarted and eventLoop != nullptr)
        {
            (void)wakeUp.stop(*eventLoop);
            wakeUpStarted = false;
        }

        if (requestBodySink.isStillWriting() or not requestBodySink.hasBeenDestroyed())
        {
            requestBodySink.destroy();
        }
        responseBodyStream.destroy();
        resetRequestBodyState();

        SC_TRY(operation.close());
        operation.setNotifier(nullptr);

        eventLoop              = nullptr;
        requestBodyBuffersPool = nullptr;
        initialized            = false;
        return Result(true);
    }

    Result cancel() { return operation.cancel(); }

    Result start(const HttpClientRequest& request, HttpClientResponse& response,
                 T_AsyncBuffersPool* requestBodyPool = nullptr)
    {
        SC_TRY_MSG(initialized, "HttpClientAsyncT: not initialized");
        if (request.streamedBodySize > 0)
        {
            SC_TRY_MSG(requestBodyPool != nullptr, "HttpClientAsyncT: streamed request body requires buffers pool");
        }

        requestBodyBuffersPool = requestBodyPool;
        resetRequestBodyState();

        responseBodyStream.setReadQueue(responseReadQueue);
        SC_TRY(responseBodyStream.init(responseBuffersPool));
        SC_TRY(responseBodyStream.start());

        if (request.streamedBodySize > 0)
        {
            SC_TRY(requestBodySink.init(*requestBodyBuffersPool, requestWriteQueue, *this));
        }

        if (not wakeUpStarted)
        {
            SC_TRY(wakeUp.start(*eventLoop));
            wakeUpStarted = true;
        }

        const Result res = operation.start(request, response, this, request.streamedBodySize > 0 ? this : nullptr);
        if (not res)
        {
            if (request.streamedBodySize > 0)
            {
                requestBodySink.destroy();
            }
            responseBodyStream.destroy();
            return res;
        }
        return Result(true);
    }

    [[nodiscard]] T_AsyncReadableStream& getResponseBodyStream() { return responseBodyStream; }
    [[nodiscard]] T_AsyncWritableStream& getRequestBodySink() { return requestBodySink; }

    [[nodiscard]] bool isInitialized() const { return initialized; }
    [[nodiscard]] bool isRequestInFlight() const { return operation.isRequestInFlight(); }

  private:
    virtual void onResponseHead(HttpClientResponse& response) override { eventResponseHead.emit(response); }

    virtual void onResponseBody(Span<const char> data) override
    {
        typename T_AsyncBufferView::ID bufferID;
        Span<char>                     writable;
        if (not responseBuffersPool.requestNewBuffer(data.sizeInBytes(), bufferID, writable))
        {
            responseBodyStream.emitError(Result::Error("HttpClientAsyncT: response buffer exhausted"));
            (void)operation.cancel();
            return;
        }
        memcpy(writable.data(), data.data(), data.sizeInBytes());
        const bool continuePushing = responseBodyStream.push(bufferID, data.sizeInBytes());
        responseBuffersPool.unrefBuffer(bufferID);
        if (not continuePushing)
        {
            (void)operation.cancel();
        }
    }

    virtual void onResponseComplete() override
    {
        responseBodyStream.pushEnd();
        resetRequestBodyState();
    }

    virtual void onError(Result error) override
    {
        responseBodyStream.emitError(error);
        if (requestBodyBuffersPool != nullptr)
        {
            requestBodySink.destroy();
        }
        resetRequestBodyState();
    }

    virtual Result pullRequestBody(Span<char> dest, size_t& bytesWritten, bool& endReached) override
    {
        bytesWritten = 0;
        endReached   = false;

        requestBodyMutex.lock();
        while (not requestBodyHasActive and not requestBodyFinished and not requestBodyErrorSet)
        {
            requestBodyCV.wait(requestBodyMutex);
        }

        if (requestBodyErrorSet)
        {
            const Result error = requestBodyError;
            requestBodyMutex.unlock();
            return error;
        }

        if (not requestBodyHasActive and requestBodyFinished)
        {
            requestBodyMutex.unlock();
            endReached = true;
            return Result(true);
        }

        const typename T_AsyncBufferView::ID activeBuffer = requestBodyActiveBuffer;
        const size_t                         activeOffset = requestBodyActiveOffset;
        requestBodyMutex.unlock();

        SC_TRY_MSG(requestBodyBuffersPool != nullptr, "HttpClientAsyncT: missing request body buffers pool");

        Span<const char> readable;
        SC_TRY(requestBodyBuffersPool->getReadableData(activeBuffer, readable));

        const size_t remaining = readable.sizeInBytes() - activeOffset;
        bytesWritten           = remaining < dest.sizeInBytes() ? remaining : dest.sizeInBytes();
        if (bytesWritten > 0)
        {
            memcpy(dest.data(), readable.data() + activeOffset, bytesWritten);
        }

        requestBodyMutex.lock();
        requestBodyActiveOffset += bytesWritten;
        const bool consumedBuffer = requestBodyActiveOffset >= readable.sizeInBytes();
        requestBodyMutex.unlock();

        if (consumedBuffer)
        {
            finishRequestBodyActive(Result(true));
        }

        return Result(true);
    }

    virtual void notifyHttpClientOperation(HttpClientOperation&) override
    {
        if (wakeUpStarted and eventLoop != nullptr)
        {
            (void)wakeUp.wakeUp(*eventLoop);
        }
    }

    void onWakeUp(typename T_AsyncLoopWakeUp::Result& result)
    {
        const Result pollResult = operation.poll();
        if (not pollResult)
        {
            responseBodyStream.emitError(pollResult);
        }
        result.reactivateRequest(true);
    }

    void resetRequestBodyState()
    {
        requestBodyMutex.lock();
        if (requestBodyHasActive and requestBodyBuffersPool != nullptr)
        {
            requestBodyBuffersPool->unrefBuffer(requestBodyActiveBuffer);
        }
        requestBodyActiveBuffer   = {};
        requestBodyActiveCallback = {};
        requestBodyActiveOffset   = 0;
        requestBodyHasActive      = false;
        requestBodyFinished       = false;
        requestBodyErrorSet       = false;
        requestBodyError          = Result(true);
        requestBodyCV.broadcast();
        requestBodyMutex.unlock();
    }

    void finishRequestBodyActive(Result res)
    {
        requestBodyMutex.lock();
        if (requestBodyHasActive and requestBodyBuffersPool != nullptr)
        {
            typename T_AsyncBufferView::ID bufferID = requestBodyActiveBuffer;

            Function<void(typename T_AsyncBufferView::ID)> callback = move(requestBodyActiveCallback);

            requestBodyHasActive    = false;
            requestBodyActiveBuffer = {};
            requestBodyActiveOffset = 0;
            requestBodyBuffersPool->unrefBuffer(bufferID);
            requestBodyMutex.unlock();
            requestBodySink.finishedWriting(bufferID, move(callback), res);
        }
        else
        {
            requestBodyMutex.unlock();
        }
    }

    void onRequestBodyWritableBuffer(typename T_AsyncBufferView::ID                   bufferID,
                                     Function<void(typename T_AsyncBufferView::ID)>&& cb)
    {
        requestBodyMutex.lock();
        if (requestBodyBuffersPool != nullptr)
        {
            requestBodyBuffersPool->refBuffer(bufferID);
        }
        requestBodyActiveBuffer   = bufferID;
        requestBodyActiveCallback = move(cb);
        requestBodyActiveOffset   = 0;
        requestBodyHasActive      = true;
        requestBodyCV.broadcast();
        requestBodyMutex.unlock();
    }

    void onRequestBodyWritableFinished()
    {
        requestBodyMutex.lock();
        requestBodyFinished = true;
        requestBodyCV.broadcast();
        requestBodyMutex.unlock();
    }

    void onRequestBodyWritableError(Result error)
    {
        requestBodyMutex.lock();
        requestBodyErrorSet = true;
        requestBodyError    = error;
        requestBodyCV.broadcast();
        requestBodyMutex.unlock();
    }

    HttpClientOperation operation;
    T_AsyncEventLoop*   eventLoop = nullptr;

    ResponseBodyStream responseBodyStream;
    RequestBodySink    requestBodySink;
    T_AsyncBuffersPool responseBuffersPool;

    Span<T_AsyncReadableRequest> responseReadQueue;
    Span<T_AsyncWritableRequest> requestWriteQueue;

    T_AsyncBuffersPool* requestBodyBuffersPool = nullptr;

    T_AsyncLoopWakeUp wakeUp;
    bool              wakeUpStarted = false;

    HttpClientLocalMutex             requestBodyMutex;
    HttpClientLocalConditionVariable requestBodyCV;

    typename T_AsyncBufferView::ID                 requestBodyActiveBuffer = {};
    Function<void(typename T_AsyncBufferView::ID)> requestBodyActiveCallback;

    size_t requestBodyActiveOffset = 0;
    bool   requestBodyHasActive    = false;
    bool   requestBodyFinished     = false;
    bool   requestBodyErrorSet     = false;
    Result requestBodyError        = Result(true);

    bool initialized = false;
};

//! @}
} // namespace SC
