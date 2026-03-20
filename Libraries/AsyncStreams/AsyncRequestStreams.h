// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Assert.h"
#include "AsyncStreams.h"
//! @addtogroup group_async_streams
//! @{
namespace SC
{
struct AsyncResult;

template <typename AsyncRequestType, typename AsyncEventLoopType>
struct AsyncRequestReadableStream : public AsyncReadableStream
{
    AsyncRequestReadableStream() {}

    /// @brief Automatically closes descriptor during read stream close event
    void setAutoCloseDescriptor(bool value) { autoCloseDescriptor = value; }

    AsyncRequestType request; /// AsyncFileRead / AsyncFileWrite / AsyncSocketReceive / AsyncSocketSend

  protected:
    using BufferViewID = AsyncBufferView::ID;
    using Self         = AsyncRequestReadableStream;

    AsyncEventLoopType* eventLoop = nullptr;
    BufferViewID        bufferID;
    bool                autoCloseDescriptor = false;

    virtual Result asyncRead() override
    {
        SC_ASSERT_RELEASE(request.isFree());
        if (this->getBufferOrPause(0, bufferID, request.buffer))
        {
            request.callback.template bind<Self, &Self::afterRead>(*this);
            SC_TRY_MSG(eventLoop != nullptr, "AsyncRequestReadableStream eventLoop == nullptr");
            const Result startResult = request.start(*eventLoop);
            if (not startResult)
            {
                this->getBuffersPool().unrefBuffer(bufferID);
                bufferID = {};
                return startResult; // Error occurred during request start
            }
        }
        return Result(true);
    }

    virtual Result asyncDestroyReadable() override
    {
        if (request.isFree())
        {
            finalizeReadableDestruction();
            return Result(true);
        }
        else
        {
            return request.stop(*eventLoop, &getStopCallback());
        }
    }

    void afterRead(typename AsyncRequestType::Result& result)
    {
        Span<char> data;
        if (result.get(data))
        {
            SC_ASSERT_RELEASE(request.isFree());
            if (result.isEnded())
            {
                this->getBuffersPool().unrefBuffer(bufferID);
                bufferID = {};
                this->pushEnd();
            }
            else
            {
                const bool continuePushing = this->push(bufferID, data.sizeInBytes());
                SC_ASSERT_RELEASE(result.getAsync().isFree());
                // Only unref if destroy() wasn't called during push callback (which would have already unref'd)
                if (not this->hasBeenDestroyed())
                {
                    this->getBuffersPool().unrefBuffer(bufferID);
                    bufferID = {};
                }
                // Check if we're still pushing (so not, paused, destroyed or errored etc.)
                if (continuePushing)
                {
                    if (this->getBufferOrPause(0, bufferID, result.getAsync().buffer))
                    {
                        request.callback.template bind<Self, &Self::afterRead>(*this);
                        result.reactivateRequest(true);
                    }
                }
            }
        }
        else
        {
            this->getBuffersPool().unrefBuffer(bufferID);
            bufferID = {};
            this->emitError(result.isValid());
        }
    }

    void finalizeReadableDestruction()
    {
        if (bufferID.isValid())
        {
            this->getBuffersPool().unrefBuffer(bufferID);
            bufferID = {};
        }
        if (autoCloseDescriptor)
        {
            SC_ASSERT_RELEASE(request.closeHandle());
        }
        SC_ASSERT_RELEASE(this->finishedDestroyingReadable());
        request = {};
    }

    template <typename T_AsyncResult>
    static void stopReadableCallback(T_AsyncResult& result)
    {
        SC_COMPILER_WARNING_PUSH_OFFSETOF;
        Self& stream = SC_COMPILER_FIELD_OFFSET(Self, request, static_cast<AsyncRequestType&>(result.async));
        stream.finalizeReadableDestruction();
        SC_COMPILER_WARNING_POP;
    }

  private:
    // clang-format off
    static Function<void(AsyncResult&)>& getStopCallback() { static Function<void(AsyncResult&)> cb = &stopReadableCallback<AsyncResult>; return cb; }
    // clang-format on

  public:
    template <typename DescriptorType>
    Result init(AsyncBuffersPool& buffersPool, AsyncEventLoopType& loop, const DescriptorType& descriptor)
    {
        SC_TRY_MSG(not request.isCancelling(), "AsyncRequestReadableStream - Destroy in progress");
        this->eventLoop = &loop;
        SC_TRY(descriptor.get(this->request.handle, Result::Error("Missing descriptor")));
        return AsyncReadableStream::init(buffersPool);
    }
};

template <typename AsyncRequestType, typename AsyncEventLoopType>
struct AsyncRequestWritableStream : public AsyncWritableStream
{
    AsyncRequestWritableStream() {}

    /// @brief Automatically closes descriptor during write stream finish event
    void setAutoCloseDescriptor(bool value) { autoCloseDescriptor = value; }

    AsyncRequestType request; /// AsyncFileRead / AsyncFileWrite / AsyncSocketReceive / AsyncSocketSend

  protected:
    using BufferViewID = AsyncBufferView::ID;
    using Self         = AsyncRequestWritableStream;

    AsyncEventLoopType* eventLoop = nullptr;
    BufferViewID        bufferID;
    bool                autoCloseDescriptor = false;

    Function<void(BufferViewID)> callback;

    virtual Result asyncWrite(BufferViewID newBufferID, Function<void(BufferViewID)> cb) override
    {
        bufferID = newBufferID;
        SC_ASSERT_RELEASE(not callback.isValid());
        callback = move(cb);
        SC_TRY(this->getBuffersPool().getReadableData(bufferID, request.buffer));
        request.callback.template bind<Self, &Self::afterWrite>(*this);
        SC_TRY_MSG(eventLoop != nullptr, "AsyncRequestWritableStream eventLoop == nullptr");
        const Result res = request.start(*eventLoop);
        if (res)
        {
            this->getBuffersPool().refBuffer(bufferID);
        }
        return res;
    }

    virtual Result asyncDestroyWritable() override
    {
        if (request.isFree())
        {
            finalizeWritableDestruction();
            return Result(true);
        }
        else
        {
            return request.stop(*eventLoop, &getStopCallback());
        }
    }

    virtual bool canEndWritable() override { return request.isFree(); }

    void afterWrite(typename AsyncRequestType::Result& result)
    {
        BufferViewID savedBufferID = bufferID;
        this->getBuffersPool().unrefBuffer(bufferID);
        bufferID = {};
        auto cb  = move(callback);
        callback = {};
        this->finishedWriting(savedBufferID, move(cb), result.isValid());
    }

    void finalizeWritableDestruction()
    {
        if (autoCloseDescriptor)
        {
            SC_ASSERT_RELEASE(request.closeHandle());
        }
        request = {};
        this->finishedDestroyingWritable();
    }

    template <typename T_AsyncResult>
    static void stopWritableCallback(T_AsyncResult& result)
    {
        SC_COMPILER_WARNING_PUSH_OFFSETOF;
        Self& stream = SC_COMPILER_FIELD_OFFSET(Self, request, static_cast<AsyncRequestType&>(result.async));
        stream.finalizeWritableDestruction();
        SC_COMPILER_WARNING_POP;
    }

  private:
    // clang-format off
    static Function<void(AsyncResult&)>& getStopCallback() { static Function<void(AsyncResult&)> cb = &stopWritableCallback<AsyncResult>; return cb; }
    // clang-format on
  public:
    template <typename DescriptorType>
    Result init(AsyncBuffersPool& buffersPool, AsyncEventLoopType& loop, const DescriptorType& descriptor)
    {
        this->eventLoop = &loop;
        SC_TRY(descriptor.get(this->request.handle, Result::Error("Missing descriptor")));
        return AsyncWritableStream::init(buffersPool);
    }
};

// clang-format off
/// @brief Uses an SC::AsyncFileRead to stream data from a file
template <typename AsyncEventLoopType> struct SC_COMPILER_EXPORT AsyncReadableFileStream : public AsyncRequestReadableStream<typename AsyncEventLoopType::FileRead, AsyncEventLoopType>{};
/// @brief Uses an SC::AsyncFileWrite to stream data to a file
template <typename AsyncEventLoopType> struct SC_COMPILER_EXPORT AsyncWritableFileStream : public AsyncRequestWritableStream<typename AsyncEventLoopType::FileWrite, AsyncEventLoopType>{};
/// @brief Uses an SC::AsyncSocketReceive to stream data from a socket
template <typename AsyncEventLoopType> struct SC_COMPILER_EXPORT AsyncReadableSocketStream : public AsyncRequestReadableStream<typename AsyncEventLoopType::SocketReceive, AsyncEventLoopType>{};
/// @brief Uses an SC::AsyncSocketSend to stream data to a socket
template <typename AsyncEventLoopType> struct SC_COMPILER_EXPORT AsyncWritableSocketStream : public AsyncRequestWritableStream<typename AsyncEventLoopType::SocketSend, AsyncEventLoopType>{};
// clang-format on

} // namespace SC
//! @}
