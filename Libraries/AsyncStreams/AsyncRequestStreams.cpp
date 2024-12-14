// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncRequestStreams.h"

//-------------------------------------------------------------------------------------------------------
// AsyncRequestReadableStream
//-------------------------------------------------------------------------------------------------------
template <typename AsyncReadRequest>
struct SC::AsyncRequestReadableStream<AsyncReadRequest>::Internal
{
    static bool isEnded(AsyncFileRead::Result& result) { return result.completionData.endOfFile; }
    static bool isEnded(AsyncSocketReceive::Result& result) { return result.completionData.disconnected; }
    static SocketDescriptor::Handle& getDescriptor(AsyncSocketReceive& async) { return async.handle; }
    static FileDescriptor::Handle&   getDescriptor(AsyncFileRead& async) { return async.fileDescriptor; }

    static Result closeDescriptor(AsyncFileRead& async)
    {
        return detail::FileDescriptorDefinition::releaseHandle(async.fileDescriptor);
    }

    static Result closeDescriptor(AsyncSocketReceive& async)
    {
        return detail::SocketDescriptorDefinition::releaseHandle(async.handle);
    }
};

template <typename AsyncReadRequest>
SC::AsyncRequestReadableStream<AsyncReadRequest>::AsyncRequestReadableStream()
{
    AsyncReadableStream::asyncRead.bind<AsyncRequestReadableStream, &AsyncRequestReadableStream::read>(*this);
}

template <typename AsyncReadRequest>
template <typename DescriptorType>
SC::Result SC::AsyncRequestReadableStream<AsyncReadRequest>::init(AsyncBuffersPool& buffersPool, Span<Request> requests,
                                                                  AsyncEventLoop&       loop,
                                                                  const DescriptorType& descriptor)
{
    request.cacheInternalEventLoop(loop);
    SC_TRY(descriptor.get(Internal::getDescriptor(request), Result::Error("Missing descriptor")));
    return AsyncReadableStream::init(buffersPool, requests);
}

template <typename AsyncReadRequest>
void SC::AsyncRequestReadableStream<AsyncReadRequest>::onEndCloseDescriptor()
{
    Result res = Internal::closeDescriptor(request);
    if (not res)
    {
        emitError(res);
    }
}

template <typename AsyncReadRequest>
SC::Result SC::AsyncRequestReadableStream<AsyncReadRequest>::registerAutoCloseDescriptor(bool value)
{
    using Self = AsyncRequestReadableStream;
    if (value)
    {
        return Result(eventEnd.addListener<Self, &Self::onEndCloseDescriptor>(*this));
    }
    else
    {
        return Result(eventEnd.removeListener<Self, &Self::onEndCloseDescriptor>(*this));
    }
}

template <typename AsyncReadRequest>
SC::Result SC::AsyncRequestReadableStream<AsyncReadRequest>::read()
{
    SC_ASSERT_RELEASE(request.isFree());
    AsyncBufferView::ID bufferID;
    if (getBufferOrPause(0, bufferID, request.buffer))
    {
        request.callback = [this, bufferID](typename AsyncReadRequest::Result& result) { afterRead(result, bufferID); };
        const Result startResult = request.start(*request.getEventLoop());
        if (not startResult)
        {
            getBuffersPool().unrefBuffer(bufferID);
            return startResult; // Error occurred during request start
        }
    }
    return Result(true);
}

template <typename AsyncReadRequest>
void SC::AsyncRequestReadableStream<AsyncReadRequest>::afterRead(typename AsyncReadRequest::Result& result,
                                                                 AsyncBufferView::ID                bufferID)
{
    SC_ASSERT_RELEASE(request.isFree());
    Span<char> data;
    if (result.get(data))
    {
        if (Internal::isEnded(result))
        {
            getBuffersPool().unrefBuffer(bufferID);
            AsyncReadableStream::pushEnd();
        }
        else
        {
            AsyncReadableStream::push(bufferID, data.sizeInBytes());
            SC_ASSERT_RELEASE(result.getAsync().isFree());
            getBuffersPool().unrefBuffer(bufferID);
            if (getBufferOrPause(0, bufferID, result.getAsync().buffer))
            {
                request.callback = [this, bufferID](typename AsyncReadRequest::Result& result)
                { afterRead(result, bufferID); };
                result.reactivateRequest(true);
                // Stream is in AsyncPushing mode and SC::AsyncResult::reactivateRequest(true) will cause more
                // data to be delivered here, so it's not necessary calling AsyncReadableStream::reactivate(true).
            }
        }
    }
    else
    {
        getBuffersPool().unrefBuffer(bufferID);
        AsyncReadableStream::emitError(result.isValid());
    }
}

//-------------------------------------------------------------------------------------------------------
// AsyncRequestWritableStream
//-------------------------------------------------------------------------------------------------------

template <typename AsyncWriteRequest>
struct SC::AsyncRequestWritableStream<AsyncWriteRequest>::Internal
{
    static SocketDescriptor::Handle& getDescriptor(AsyncSocketSend& async) { return async.handle; }
    static FileDescriptor::Handle&   getDescriptor(AsyncFileWrite& async) { return async.fileDescriptor; }

    static Result closeDescriptor(AsyncFileWrite& async)
    {
        return detail::FileDescriptorDefinition::releaseHandle(async.fileDescriptor);
    }

    static Result closeDescriptor(AsyncSocketSend& async)
    {
        return detail::SocketDescriptorDefinition::releaseHandle(async.handle);
    }
};

template <typename AsyncWriteRequest>
SC::AsyncRequestWritableStream<AsyncWriteRequest>::AsyncRequestWritableStream()
{
    AsyncWritableStream::asyncWrite.bind<AsyncRequestWritableStream, &AsyncRequestWritableStream::write>(*this);
}

template <typename AsyncWriteRequest>
template <typename DescriptorType>
SC::Result SC::AsyncRequestWritableStream<AsyncWriteRequest>::init(AsyncBuffersPool& buffersPool,
                                                                   Span<Request> requests, AsyncEventLoop& loop,
                                                                   const DescriptorType& descriptor)
{
    request.cacheInternalEventLoop(loop);
    SC_TRY(descriptor.get(Internal::getDescriptor(request), Result::Error("Missing descriptor")));
    return AsyncWritableStream::init(buffersPool, requests);
}

template <typename AsyncWriteRequest>
void SC::AsyncRequestWritableStream<AsyncWriteRequest>::onEndCloseDescriptor()
{
    Result res = Internal::closeDescriptor(request);
    if (not res)
    {
        emitError(res);
    }
}

template <typename AsyncWriteRequest>
SC::Result SC::AsyncRequestWritableStream<AsyncWriteRequest>::registerAutoCloseDescriptor(bool value)
{
    using Self = AsyncRequestWritableStream;
    if (value)
    {
        return Result(eventFinish.addListener<Self, &Self::onEndCloseDescriptor>(*this));
    }
    else
    {
        return Result(eventFinish.removeListener<Self, &Self::onEndCloseDescriptor>(*this));
    }
}

template <typename AsyncWriteRequest>
SC::Result SC::AsyncRequestWritableStream<AsyncWriteRequest>::write(AsyncBufferView::ID                 bufferID,
                                                                    Function<void(AsyncBufferView::ID)> cb)
{
    SC_ASSERT_RELEASE(not callback.isValid());
    callback = move(cb);
    SC_TRY(getBuffersPool().getData(bufferID, request.buffer));
    request.callback = [this, bufferID](typename AsyncWriteRequest::Result& result)
    {
        getBuffersPool().unrefBuffer(bufferID);
        auto callbackCopy = move(callback);
        callback          = {};
        AsyncWritableStream::finishedWriting(bufferID, move(callbackCopy), result.isValid());
    };
    const Result res = request.start(*request.getEventLoop());
    if (res)
    {
        getBuffersPool().refBuffer(bufferID);
    }
    return res;
}
namespace SC
{
SC_COMPILER_EXTERN template struct AsyncRequestReadableStream<AsyncSocketReceive>;
SC_COMPILER_EXTERN template struct AsyncRequestReadableStream<AsyncFileRead>;
SC_COMPILER_EXTERN template struct AsyncRequestWritableStream<AsyncFileWrite>;
SC_COMPILER_EXTERN template struct AsyncRequestWritableStream<AsyncSocketSend>;

SC_COMPILER_EXTERN template SC::Result SC::AsyncRequestReadableStream<AsyncSocketReceive>::init(
    AsyncBuffersPool& buffersPool, Span<Request> requests, AsyncEventLoop& loop, const SocketDescriptor& descriptor);
SC_COMPILER_EXTERN template SC::Result SC::AsyncRequestWritableStream<AsyncSocketSend>::init(
    AsyncBuffersPool& buffersPool, Span<Request> requests, AsyncEventLoop& loop, const SocketDescriptor& descriptor);
SC_COMPILER_EXTERN template SC::Result SC::AsyncRequestReadableStream<AsyncFileRead>::init(
    AsyncBuffersPool& buffersPool, Span<Request> requests, AsyncEventLoop& loop, const FileDescriptor& descriptor);
SC_COMPILER_EXTERN template SC::Result SC::AsyncRequestWritableStream<AsyncFileWrite>::init(
    AsyncBuffersPool& buffersPool, Span<Request> requests, AsyncEventLoop& loop, const FileDescriptor& descriptor);

} // namespace SC
