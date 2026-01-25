// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncRequestStreams.h"
#include "../Foundation/Assert.h"

namespace SC
{
//-------------------------------------------------------------------------------------------------------
// AsyncRequestReadableStream
//-------------------------------------------------------------------------------------------------------
template <typename AsyncReadRequest>
struct AsyncRequestReadableStream<AsyncReadRequest>::Internal
{
    static bool isEnded(AsyncFileRead::Result& result) { return result.completionData.endOfFile; }
    static bool isEnded(AsyncSocketReceive::Result& result) { return result.completionData.disconnected; }
    template <typename T>
    static auto& getDescriptor(T& async)
    {
        return async.handle;
    }

    static Result closeDescriptor(AsyncFileRead& async)
    {
        return detail::FileDescriptorDefinition::releaseHandle(async.handle);
    }

    static Result closeDescriptor(AsyncSocketReceive& async)
    {
        return detail::SocketDescriptorDefinition::releaseHandle(async.handle);
    }

    template <typename DescriptorType>
    static Result init(AsyncRequestReadableStream& self, AsyncBuffersPool& buffersPool, AsyncEventLoop& eventLoop,
                       const DescriptorType& descriptor)
    {
        SC_TRY_MSG(not self.request.isCancelling(), "AsyncRequestReadableStream - Destroy in progress");
        self.eventLoop = &eventLoop;
        SC_TRY(descriptor.get(Internal::getDescriptor(self.request), Result::Error("Missing descriptor")));
        return self.AsyncReadableStream::init(buffersPool);
    }
};

template <typename AsyncReadRequest>
AsyncRequestReadableStream<AsyncReadRequest>::AsyncRequestReadableStream()
{}

template <typename AsyncReadRequest>
Result AsyncRequestReadableStream<AsyncReadRequest>::asyncDestroyReadable()
{
    if (request.isFree())
    {
        finalizeReadableDestruction();
        return Result(true);
    }
    else
    {
        return request.stop(*eventLoop, &onStopCallback);
    }
}

template <typename AsyncReadRequest>
Function<void(AsyncResult&)> AsyncRequestReadableStream<AsyncReadRequest>::onStopCallback =
    &AsyncRequestReadableStream<AsyncReadRequest>::stopRedableCallback;

template <typename AsyncReadRequest>
void AsyncRequestReadableStream<AsyncReadRequest>::stopRedableCallback(AsyncResult& result)
{
    SC_COMPILER_WARNING_PUSH_OFFSETOF;
    AsyncReadRequest&           asyncRequest = static_cast<AsyncReadRequest&>(result.async);
    AsyncRequestReadableStream& stream = SC_COMPILER_FIELD_OFFSET(AsyncRequestReadableStream, request, asyncRequest);
    stream.finalizeReadableDestruction();
    SC_COMPILER_WARNING_POP;
}

template <typename AsyncReadRequest>
void AsyncRequestReadableStream<AsyncReadRequest>::finalizeReadableDestruction()
{
    if (bufferID.isValid())
    {
        getBuffersPool().unrefBuffer(bufferID);
        bufferID = {};
    }
    if (autoCloseDescriptor)
    {
        SC_ASSERT_RELEASE(Internal::closeDescriptor(request));
    }
    SC_ASSERT_RELEASE(AsyncReadableStream::finishedDestroyingReadable());
    request = {};
}

template <typename AsyncReadRequest>
Result AsyncRequestReadableStream<AsyncReadRequest>::asyncRead()
{
    SC_ASSERT_RELEASE(request.isFree());
    if (getBufferOrPause(0, bufferID, request.buffer))
    {
        request.callback.template bind<AsyncRequestReadableStream, &AsyncRequestReadableStream::afterRead>(*this);
        SC_TRY_MSG(eventLoop != nullptr, "AsyncRequestReadableStream eventLoop == nullptr");
        const Result startResult = request.start(*eventLoop);
        if (not startResult)
        {
            getBuffersPool().unrefBuffer(bufferID);
            bufferID = {};
            return startResult; // Error occurred during request start
        }
    }
    return Result(true);
}

template <typename AsyncReadRequest>
void AsyncRequestReadableStream<AsyncReadRequest>::afterRead(typename AsyncReadRequest::Result& result)
{
    Span<char> data;
    if (result.get(data))
    {
        SC_ASSERT_RELEASE(request.isFree());
        if (Internal::isEnded(result))
        {
            getBuffersPool().unrefBuffer(bufferID);
            bufferID = {};
            AsyncReadableStream::pushEnd();
        }
        else
        {
            const bool continuePushing = AsyncReadableStream::push(bufferID, data.sizeInBytes());
            SC_ASSERT_RELEASE(result.getAsync().isFree());
            // Only unref if destroy() wasn't called during push callback (which would have already unref'd)
            if (not AsyncReadableStream::hasBeenDestroyed())
            {
                getBuffersPool().unrefBuffer(bufferID);
                bufferID = {};
            }
            // Check if we're still pushing (so not, paused, destroyed or errored etc.)
            if (continuePushing)
            {
                if (getBufferOrPause(0, bufferID, result.getAsync().buffer))
                {
                    request.callback.template bind<AsyncRequestReadableStream, &AsyncRequestReadableStream::afterRead>(
                        *this);
                    result.reactivateRequest(true);
                    // Stream is in AsyncPushing mode and AsyncResult::reactivateRequest(true) will cause more
                    // data to be delivered here, so it's not necessary calling AsyncReadableStream::reactivate(true).
                }
            }
        }
    }
    else
    {
        getBuffersPool().unrefBuffer(bufferID);
        bufferID = {};
        AsyncReadableStream::emitError(result.isValid());
    }
}

//-------------------------------------------------------------------------------------------------------
// AsyncRequestWritableStream
//-------------------------------------------------------------------------------------------------------

template <typename AsyncWriteRequest>
struct AsyncRequestWritableStream<AsyncWriteRequest>::Internal
{
    template <typename T>
    static auto& getDescriptor(T& async)
    {
        return async.handle;
    }

    static Result closeDescriptor(AsyncFileWrite& async)
    {
        return detail::FileDescriptorDefinition::releaseHandle(async.handle);
    }

    static Result closeDescriptor(AsyncSocketSend& async)
    {
        return detail::SocketDescriptorDefinition::releaseHandle(async.handle);
    }

    template <typename DescriptorType>
    static Result init(AsyncRequestWritableStream& self, AsyncBuffersPool& buffersPool, AsyncEventLoop& eventLoop,
                       const DescriptorType& descriptor)
    {
        self.eventLoop = &eventLoop;
        SC_TRY(descriptor.get(Internal::getDescriptor(self.request), Result::Error("Missing descriptor")));
        return self.AsyncWritableStream::init(buffersPool);
    }
};

template <typename AsyncWriteRequest>
AsyncRequestWritableStream<AsyncWriteRequest>::AsyncRequestWritableStream()
{}

template <typename AsyncWriteRequest>
Result AsyncRequestWritableStream<AsyncWriteRequest>::asyncDestroyWritable()
{
    if (request.isFree())
    {
        finalizeWritableDestruction();
        return Result(true);
    }
    else
    {
        return request.stop(*eventLoop, &onStopCallback);
    }
}

template <typename AsyncWriteRequest>
bool AsyncRequestWritableStream<AsyncWriteRequest>::canEndWritable()
{
    return request.isFree();
}

template <typename AsyncWriteRequest>
Function<void(AsyncResult&)> AsyncRequestWritableStream<AsyncWriteRequest>::onStopCallback =
    &AsyncRequestWritableStream<AsyncWriteRequest>::stopWritableCallback;

template <typename AsyncWriteRequest>
void AsyncRequestWritableStream<AsyncWriteRequest>::stopWritableCallback(AsyncResult& result)
{
    SC_COMPILER_WARNING_PUSH_OFFSETOF;
    AsyncWriteRequest&          asyncRequest = static_cast<AsyncWriteRequest&>(result.async);
    AsyncRequestWritableStream& stream = SC_COMPILER_FIELD_OFFSET(AsyncRequestWritableStream, request, asyncRequest);
    stream.finalizeWritableDestruction();
    SC_COMPILER_WARNING_POP;
}

template <typename AsyncWriteRequest>
void AsyncRequestWritableStream<AsyncWriteRequest>::finalizeWritableDestruction()
{
    if (autoCloseDescriptor)
    {
        Result res = Internal::closeDescriptor(request);
        if (not res)
        {
            emitError(res);
        }
    }
    request = {};
    AsyncWritableStream::finishedDestroyingWritable();
}

template <typename AsyncWriteRequest>
Result AsyncRequestWritableStream<AsyncWriteRequest>::asyncWrite(AsyncBufferView::ID                 newBufferID,
                                                                 Function<void(AsyncBufferView::ID)> cb)
{
    bufferID = newBufferID;
    SC_ASSERT_RELEASE(not callback.isValid());
    callback = move(cb);
    SC_TRY(getBuffersPool().getReadableData(bufferID, request.buffer));
    request.callback.template bind<AsyncRequestWritableStream, &AsyncRequestWritableStream::afterWrite>(*this);
    SC_TRY_MSG(eventLoop != nullptr, "AsyncRequestReadableStream eventLoop == nullptr");
    const Result res = request.start(*eventLoop);
    if (res)
    {
        getBuffersPool().refBuffer(bufferID);
    }
    return res;
}

template <typename AsyncWriteRequest>
void AsyncRequestWritableStream<AsyncWriteRequest>::afterWrite(typename AsyncWriteRequest::Result& result)
{
    AsyncBufferView::ID savedBufferID = bufferID;
    getBuffersPool().unrefBuffer(bufferID);
    bufferID = {};
    auto cb  = move(callback);
    callback = {};
    AsyncWritableStream::finishedWriting(savedBufferID, move(cb), result.isValid());
}

SC_COMPILER_EXTERN template struct AsyncRequestReadableStream<AsyncSocketReceive>;
SC_COMPILER_EXTERN template struct AsyncRequestReadableStream<AsyncFileRead>;
SC_COMPILER_EXTERN template struct AsyncRequestWritableStream<AsyncFileWrite>;
SC_COMPILER_EXTERN template struct AsyncRequestWritableStream<AsyncSocketSend>;

Result ReadableSocketStream::init(AsyncBuffersPool& buffersPool, AsyncEventLoop& loop,
                                  const SocketDescriptor& descriptor)
{
    return Internal::init(*this, buffersPool, loop, descriptor);
}
Result WritableSocketStream::init(AsyncBuffersPool& buffersPool, AsyncEventLoop& loop,
                                  const SocketDescriptor& descriptor)
{
    return Internal::init(*this, buffersPool, loop, descriptor);
}
Result ReadableFileStream::init(AsyncBuffersPool& buffersPool, AsyncEventLoop& loop, const FileDescriptor& descriptor)
{
    return Internal::init(*this, buffersPool, loop, descriptor);
}
Result WritableFileStream::init(AsyncBuffersPool& buffersPool, AsyncEventLoop& loop, const FileDescriptor& descriptor)
{
    return Internal::init(*this, buffersPool, loop, descriptor);
}

} // namespace SC
