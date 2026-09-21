// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/ResultErrorFormatter.h"
#include "AwaitError.h"

namespace SC
{
/// @brief Format an Await-owned error into caller-provided UTF-8 storage.
/// @details Include this optional header only where canonical English text is needed.
inline ResultErrorFormat formatAwaitError(AwaitError error, Span<char> output)
{
    ResultErrorFormatter formatter(output);
    switch (error)
    {
    case AwaitError::Cancelled: formatter.append("Awaited operation was cancelled"); break;
    case AwaitError::WrongEventLoop: formatter.append("Task belongs to another event loop"); break;
    case AwaitError::AllocatorAlreadyOpen: formatter.append("Allocator is already open"); break;
    case AwaitError::AllocatorStorageEmpty: formatter.append("Allocator storage is empty"); break;
    case AwaitError::AllocatorReservationEmpty: formatter.append("Allocator reservation size is zero"); break;
    case AwaitError::AllocatorReservationFailed: formatter.append("Allocator memory could not be reserved"); break;
    case AwaitError::AllocatorCommitFailed: formatter.append("Allocator memory could not be committed"); break;
    case AwaitError::AllocatorHasLiveAllocations: formatter.append("Allocator still has live allocations"); break;
    case AwaitError::InvalidTask: formatter.append("Task is invalid"); break;
    case AwaitError::TaskNotCompleted: formatter.append("Task has not completed"); break;
    case AwaitError::TaskNotStarted: formatter.append("Task has not started"); break;
    case AwaitError::TaskAlreadyStarted: formatter.append("Task has already started"); break;
    case AwaitError::TaskCancellationUnavailable: formatter.append("Task cannot be cancelled now"); break;
    case AwaitError::UnhandledException: formatter.append("Task encountered an unhandled exception"); break;
    case AwaitError::SocketSendNoProgress: formatter.append("Socket send made no progress"); break;
    case AwaitError::SocketReceiveIncomplete:
        formatter.append("Socket disconnected before the requested data arrived");
        break;
    case AwaitError::SocketReceiveNoProgress: formatter.append("Socket receive made no progress"); break;
    case AwaitError::EmptyReceiveBuffer: formatter.append("Receive buffer is empty"); break;
    case AwaitError::ReceiveLineBufferExhausted:
        formatter.append("Receive buffer filled before a newline arrived");
        break;
    case AwaitError::FileReadNoProgress: formatter.append("File read made no progress"); break;
    case AwaitError::OperationUnsupported: formatter.append("Operation is unsupported"); break;
    case AwaitError::InvalidFileHandle: formatter.append("File handle is invalid"); break;
    case AwaitError::MissingOutputFile: formatter.append("Output file is missing"); break;
    case AwaitError::MissingFile: formatter.append("File is missing"); break;
    case AwaitError::MissingReadResult: formatter.append("File read result is missing"); break;
    case AwaitError::InvalidFileSystemOperation: formatter.append("File-system operation is invalid"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

inline ResultErrorFormat formatAwaitError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != AwaitResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatAwaitError(static_cast<AwaitError>(result.errorValue()), output);
}
} // namespace SC
