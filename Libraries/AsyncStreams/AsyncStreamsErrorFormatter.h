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
