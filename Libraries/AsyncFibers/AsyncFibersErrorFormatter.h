// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/ResultErrorFormatter.h"
#include "AsyncFibersError.h"

namespace SC
{
/// @brief Format an AsyncFibers-owned error into caller-provided UTF-8 storage.
/// @details Include this optional header only where canonical English text is needed.
inline ResultErrorFormat formatAsyncFibersError(AsyncFibersError error, Span<char> output)
{
    ResultErrorFormatter formatter(output);
    switch (error)
    {
    case AsyncFibersError::Cancelled: formatter.append("Fiber I/O operation was cancelled"); break;
    case AsyncFibersError::SocketSendNoProgress: formatter.append("Socket send made no progress"); break;
    case AsyncFibersError::InvalidFileHandle: formatter.append("File handle is invalid"); break;
    case AsyncFibersError::UnexpectedEndOfFile:
        formatter.append("File ended before the requested data was read");
        break;
    case AsyncFibersError::WrongOwnerThread: formatter.append("Fiber I/O must run on its owner thread"); break;
    case AsyncFibersError::FiberContextRequired: formatter.append("Operation requires a running fiber"); break;
    case AsyncFibersError::CommandStorageEmpty: formatter.append("Command queue has no storage"); break;
    case AsyncFibersError::InvalidCommand: formatter.append("Command is invalid"); break;
    case AsyncFibersError::CommandQueueFull: formatter.append("Command queue is full"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

inline ResultErrorFormat formatAsyncFibersError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != AsyncFibersResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatAsyncFibersError(static_cast<AsyncFibersError>(result.errorValue()), output);
}
} // namespace SC
