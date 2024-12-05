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

    /// @brief Registers or unregisters a listener to AsyncReadableStream::eventEnd to close descriptor
    Result registerAutoCloseDescriptor(bool value);

  private:
    struct Internal;
    AsyncRequestType request;

    Result read();

    void afterRead(typename AsyncRequestType::Result& result, AsyncBufferView::ID bufferID);
    void onEndCloseDescriptor();
};

template <typename AsyncRequestType>
struct AsyncRequestWritableStream : public AsyncWritableStream
{
    AsyncRequestWritableStream();

    template <typename DescriptorType>
    Result init(AsyncBuffersPool& buffersPool, Span<Request> requests, AsyncEventLoop& loop,
                const DescriptorType& descriptor);

    /// @brief Registers or unregisters a listener to AsyncWritableStream::eventFinish to close descriptor
    Result registerAutoCloseDescriptor(bool value);

  private:
    struct Internal;
    AsyncRequestType request;

    Function<void(AsyncBufferView::ID)> callback;

    Result write(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb);

    void onEndCloseDescriptor();
};

using ReadableFileStream   = AsyncRequestReadableStream<AsyncFileRead>;
using WritableFileStream   = AsyncRequestWritableStream<AsyncFileWrite>;
using ReadableSocketStream = AsyncRequestReadableStream<AsyncSocketReceive>;
using WritableSocketStream = AsyncRequestWritableStream<AsyncSocketSend>;

} // namespace SC
//! @}
