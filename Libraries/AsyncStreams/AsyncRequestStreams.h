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

    AsyncRequestType request; /// AsyncFileRead / AsyncFileWrite / AsyncSocketReceive / AsyncSocketSend

  protected:
    struct Internal;

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

    AsyncRequestType request; /// AsyncFileRead / AsyncFileWrite / AsyncSocketReceive / AsyncSocketSend

  protected:
    struct Internal;

    Function<void(AsyncBufferView::ID)> callback;

    Result write(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb);

    void onEndCloseDescriptor();
};

// clang-format off
/// @brief Uses an SC::AsyncFileRead to stream data from a file
struct ReadableFileStream : public AsyncRequestReadableStream<AsyncFileRead>{};
/// @brief Uses an SC::AsyncFileWrite to stream data to a file
struct WritableFileStream : public AsyncRequestWritableStream<AsyncFileWrite>{};
/// @brief Uses an SC::AsyncFileWrite to stream data from a socket
struct ReadableSocketStream : public AsyncRequestReadableStream<AsyncSocketReceive>{};
/// @brief Uses an SC::AsyncFileWrite to stream data to a socket
struct WritableSocketStream : public AsyncRequestWritableStream<AsyncSocketSend>{};
// clang-format on

} // namespace SC
//! @}
