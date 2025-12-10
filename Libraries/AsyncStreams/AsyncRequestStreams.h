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
struct SC_COMPILER_EXPORT AsyncRequestReadableStream : public AsyncReadableStream
{
    AsyncRequestReadableStream();

    /// @brief Automatically closes descriptor during read stream close event
    void setAutoCloseDescriptor(bool value) { autoCloseDescriptor = value; }

    AsyncRequestType request; /// AsyncFileRead / AsyncFileWrite / AsyncSocketReceive / AsyncSocketSend
  protected:
    struct Internal;
    AsyncEventLoop* eventLoop = nullptr;

    bool autoCloseDescriptor = false;
    bool justUnrefBuffer     = false;

    Result read();

    void afterRead(typename AsyncRequestType::Result& result, AsyncBufferView::ID bufferID);
    void onCloseStopRequest();
};

template <typename AsyncRequestType>
struct SC_COMPILER_EXPORT AsyncRequestWritableStream : public AsyncWritableStream
{
    AsyncRequestWritableStream();

    template <typename DescriptorType>
    Result init(AsyncBuffersPool& buffersPool, Span<Request> requests, AsyncEventLoop& eventLoop,
                const DescriptorType& descriptor);

    /// @brief Automatically closes descriptor during write stream finish event
    void setAutoCloseDescriptor(bool value) { autoCloseDescriptor = value; }

    AsyncRequestType request; /// AsyncFileRead / AsyncFileWrite / AsyncSocketReceive / AsyncSocketSend

  protected:
    struct Internal;
    AsyncEventLoop* eventLoop = nullptr;

    bool autoCloseDescriptor = false;
    bool justUnrefBuffer     = false;

    Function<void(AsyncBufferView::ID)> callback;

    Result write(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb);

    void onFinishStopRequest();
};

/// @brief Uses an SC::AsyncFileRead to stream data from a file
struct SC_COMPILER_EXPORT ReadableFileStream : public AsyncRequestReadableStream<AsyncFileRead>
{
    Result init(AsyncBuffersPool& buffersPool, Span<Request> requests, AsyncEventLoop& eventLoop,
                const FileDescriptor& descriptor);
};

/// @brief Uses an SC::AsyncFileWrite to stream data to a file
struct SC_COMPILER_EXPORT WritableFileStream : public AsyncRequestWritableStream<AsyncFileWrite>
{
    Result init(AsyncBuffersPool& buffersPool, Span<Request> requests, AsyncEventLoop& eventLoop,
                const FileDescriptor& descriptor);
};

/// @brief Uses an SC::AsyncFileWrite to stream data from a socket
struct SC_COMPILER_EXPORT ReadableSocketStream : public AsyncRequestReadableStream<AsyncSocketReceive>
{
    Result init(AsyncBuffersPool& buffersPool, Span<Request> requests, AsyncEventLoop& eventLoop,
                const SocketDescriptor& descriptor);
};

/// @brief Uses an SC::AsyncFileWrite to stream data to a socket
struct SC_COMPILER_EXPORT WritableSocketStream : public AsyncRequestWritableStream<AsyncSocketSend>
{
    Result init(AsyncBuffersPool& buffersPool, Span<Request> requests, AsyncEventLoop& eventLoop,
                const SocketDescriptor& descriptor);
};

} // namespace SC
//! @}
