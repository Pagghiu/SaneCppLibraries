// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncRequestStreams.h"
#include "../Foundation/Assert.h"
//-------------------------------------------------------------------------------------------------------
// AsyncRequestReadableStream
//-------------------------------------------------------------------------------------------------------
template <typename AsyncReadRequest>
struct SC::AsyncRequestReadableStream<AsyncReadRequest>::Internal
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
        self.request   = {};
        SC_TRY(descriptor.get(Internal::getDescriptor(self.request), Result::Error("Missing descriptor")));
        return self.AsyncReadableStream::init(buffersPool);
    }
};

template <typename AsyncReadRequest>
SC::AsyncRequestReadableStream<AsyncReadRequest>::AsyncRequestReadableStream()
{
    (void)AsyncReadableStream::eventClose
        .addListener<AsyncRequestReadableStream, &AsyncRequestReadableStream::onCloseStopRequest>(*this);
}

template <typename AsyncReadRequest>
void SC::AsyncRequestReadableStream<AsyncReadRequest>::onCloseStopRequest()
{
    if (not request.isFree())
    {
        SC::Result res = SC::Result::Error("Fake error to unref bufferID");

        typename AsyncReadRequest::Result result(*eventLoop, request, res, nullptr);

        justUnrefBuffer = true;
        request.callback(result); // will free bufferID
        if (not request.isCancelling())
        {
            SC_ASSERT_RELEASE(request.stop(*eventLoop));
        }
        justUnrefBuffer = false;
    }
    if (autoCloseDescriptor)
    {
        Result res = Internal::closeDescriptor(request);
        if (not res)
        {
            emitError(res);
        }
    }
}

template <typename AsyncReadRequest>
SC::Result SC::AsyncRequestReadableStream<AsyncReadRequest>::asyncRead()
{
    SC_ASSERT_RELEASE(request.isFree());
    AsyncBufferView::ID bufferID;
    if (getBufferOrPause(0, bufferID, request.buffer))
    {
        request.callback = [this, bufferID](typename AsyncReadRequest::Result& result) { afterRead(result, bufferID); };
        SC_TRY_MSG(eventLoop != nullptr, "AsyncRequestReadableStream eventLoop == nullptr");
        const Result startResult = request.start(*eventLoop);
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
    Span<char> data;
    if (result.get(data))
    {
        SC_ASSERT_RELEASE(request.isFree());
        if (Internal::isEnded(result))
        {
            getBuffersPool().unrefBuffer(bufferID);
            AsyncReadableStream::pushEnd();
        }
        else
        {
            const bool continuePushing = AsyncReadableStream::push(bufferID, data.sizeInBytes());
            SC_ASSERT_RELEASE(result.getAsync().isFree());
            getBuffersPool().unrefBuffer(bufferID);
            // Check if we're still pushing (so not, paused, destroyed or errored etc.)
            if (continuePushing)
            {
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
    }
    else
    {
        getBuffersPool().unrefBuffer(bufferID);
        if (justUnrefBuffer)
            return;
        AsyncReadableStream::emitError(result.isValid());
    }
}

//-------------------------------------------------------------------------------------------------------
// AsyncRequestWritableStream
//-------------------------------------------------------------------------------------------------------

template <typename AsyncWriteRequest>
struct SC::AsyncRequestWritableStream<AsyncWriteRequest>::Internal
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
        self.request   = {};
        SC_TRY(descriptor.get(Internal::getDescriptor(self.request), Result::Error("Missing descriptor")));
        return self.AsyncWritableStream::init(buffersPool);
    }
};

template <typename AsyncWriteRequest>
SC::AsyncRequestWritableStream<AsyncWriteRequest>::AsyncRequestWritableStream()
{
    (void)AsyncWritableStream::eventFinish
        .addListener<AsyncRequestWritableStream, &AsyncRequestWritableStream::onFinishStopRequest>(*this);
}

template <typename AsyncWriteRequest>
void SC::AsyncRequestWritableStream<AsyncWriteRequest>::onFinishStopRequest()
{
    if (not request.isFree())
    {
        SC::Result res = SC::Result::Error("Fake error to unref bufferID");

        typename AsyncWriteRequest::Result result(*eventLoop, request, res, nullptr);

        justUnrefBuffer = true;
        request.callback(result); // will free bufferID
        if (not request.isCancelling())
        {
            SC_ASSERT_RELEASE(request.stop(*eventLoop));
        }
        justUnrefBuffer = false;
    }
    if (autoCloseDescriptor)
    {
        Result res = Internal::closeDescriptor(request);
        if (not res)
        {
            emitError(res);
        }
    }
}

template <typename AsyncWriteRequest>
SC::Result SC::AsyncRequestWritableStream<AsyncWriteRequest>::asyncWrite(AsyncBufferView::ID                 bufferID,
                                                                         Function<void(AsyncBufferView::ID)> cb)
{
    SC_ASSERT_RELEASE(not callback.isValid());
    callback = move(cb);
    SC_TRY(getBuffersPool().getReadableData(bufferID, request.buffer));
    request.callback = [this, bufferID](typename AsyncWriteRequest::Result& result)
    {
        getBuffersPool().unrefBuffer(bufferID);
        if (justUnrefBuffer)
            return;
        auto callbackCopy = move(callback);
        callback          = {};
        AsyncWritableStream::finishedWriting(bufferID, move(callbackCopy), result.isValid());
    };
    SC_TRY_MSG(eventLoop != nullptr, "AsyncRequestReadableStream eventLoop == nullptr");
    const Result res = request.start(*eventLoop);
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
