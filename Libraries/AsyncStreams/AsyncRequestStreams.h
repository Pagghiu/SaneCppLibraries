// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Async/Async.h"
#include "AsyncStreams.h"

//! @addtogroup group_async_streams
//! @{
namespace SC
{
template <typename AsyncRequestType>
struct AsyncRequestReadableStream : public AsyncReadableStream
{
    AsyncRequestReadableStream();

    template <typename DescriptorType>
    Result init(AsyncBuffersPool& buffersPool, Span<Request> requests, AsyncEventLoop& loop,
                const DescriptorType& descriptor);

  private:
    struct Internal;
    AsyncRequestType request;

    Result read();
    void   afterRead(typename AsyncRequestType::Result& result, AsyncBufferView::ID bufferID);
};

template <typename AsyncRequestType>
struct AsyncRequestWritableStream : public AsyncWritableStream
{
    AsyncRequestWritableStream();

    template <typename DescriptorType>
    Result init(AsyncBuffersPool& buffersPool, Span<Request> requests, AsyncEventLoop& loop,
                const DescriptorType& descriptor);

  private:
    struct Internal;
    AsyncRequestType request;

    Function<void(AsyncBufferView::ID)> callback;

    Result write(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb);
};

using ReadableFileStream   = AsyncRequestReadableStream<AsyncFileRead>;
using WritableFileStream   = AsyncRequestWritableStream<AsyncFileWrite>;
using ReadableSocketStream = AsyncRequestReadableStream<AsyncSocketReceive>;
using WritableSocketStream = AsyncRequestWritableStream<AsyncSocketSend>;

} // namespace SC
//! @}
