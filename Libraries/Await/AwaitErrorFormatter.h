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
