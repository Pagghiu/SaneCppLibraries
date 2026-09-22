// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/ResultErrorFormatter.h"
#include "AsyncStreamsError.h"

namespace SC
{
/// @brief Format an AsyncStreams-owned error into caller-provided UTF-8 storage.
/// @details Include this optional header only where canonical English text is needed.
inline ResultErrorFormat formatAsyncStreamsError(AsyncStreamsError error, Span<char> output)
{
    ResultErrorFormatter formatter(output);
    switch (error)
    {
    case AsyncStreamsError::InvalidBufferID: formatter.append("Buffer identifier is invalid"); break;
    case AsyncStreamsError::InvalidParentBufferID: formatter.append("Parent buffer identifier is invalid"); break;
    case AsyncStreamsError::BufferNotWritable: formatter.append("Buffer is not writable"); break;
    case AsyncStreamsError::NoReusableBuffer: formatter.append("No reusable buffer can satisfy the request"); break;
    case AsyncStreamsError::BufferPoolFull: formatter.append("Buffer pool is full"); break;
    case AsyncStreamsError::InsufficientSliceStorage: formatter.append("Slice storage is too small"); break;
    case AsyncStreamsError::InvalidSliceCount: formatter.append("Slice count must be greater than zero"); break;
    case AsyncStreamsError::ChildViewOutOfBounds: formatter.append("Child view exceeds its parent buffer"); break;
    case AsyncStreamsError::InvalidRootBufferType: formatter.append("Root buffer type is invalid"); break;
    case AsyncStreamsError::ReadableNotReady: formatter.append("Readable stream is not ready to start"); break;
    case AsyncStreamsError::ReadQueueMissing: formatter.append("Readable stream has no read queue"); break;
    case AsyncStreamsError::EmptyReadBuffer: formatter.append("Readable stream cannot push an empty buffer"); break;
    case AsyncStreamsError::ReadQueueFull: formatter.append("Readable stream queue is full"); break;
    case AsyncStreamsError::InvalidReadableState:
        formatter.append("Readable stream operation is invalid in this state");
        break;
    case AsyncStreamsError::ReadReactivationMissing:
        formatter.append("Readable stream did not reactivate after pushing data");
        break;
    case AsyncStreamsError::ReadableDestroying: formatter.append("Readable stream is being destroyed"); break;
    case AsyncStreamsError::ReadableEnded: formatter.append("Readable stream has ended"); break;
    case AsyncStreamsError::ReadableNotInitialized: formatter.append("Readable stream is not initialized"); break;
    case AsyncStreamsError::ReadableErrored: formatter.append("Readable stream is in an error state"); break;
    case AsyncStreamsError::InvalidWritableState:
        formatter.append("Writable stream operation is invalid in this state");
        break;
    case AsyncStreamsError::WriteQueueMissing: formatter.append("Writable stream has no write queue"); break;
    case AsyncStreamsError::WriteAfterEnd: formatter.append("Writable stream cannot accept writes after end"); break;
    case AsyncStreamsError::WriteQueueFull: formatter.append("Writable stream queue is full"); break;
    case AsyncStreamsError::WritableDestroying: formatter.append("Writable stream is being destroyed"); break;
    case AsyncStreamsError::WritableEndAlreadyCalled: formatter.append("Writable stream end was already called"); break;
    case AsyncStreamsError::PipelineSourceMissing: formatter.append("Pipeline source is missing"); break;
    case AsyncStreamsError::PipelineSinkMissing: formatter.append("Pipeline has no sink"); break;
    case AsyncStreamsError::PipelineBufferPoolMismatch:
        formatter.append("Pipeline streams must share the same buffer pool");
        break;
    case AsyncStreamsError::PipelineListenerStorageFull: formatter.append("Pipeline listener storage is full"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

inline ResultErrorFormat formatAsyncStreamsError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != AsyncStreamsResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatAsyncStreamsError(static_cast<AsyncStreamsError>(result.errorValue()), output);
}
} // namespace SC
