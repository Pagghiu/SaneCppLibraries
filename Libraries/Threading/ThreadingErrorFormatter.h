// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Common/ResultErrorFormatter.h"
#include "Threading.h"

namespace SC
{
//! @addtogroup group_threading
//! @{

/// @brief Formats a canonical English Threading diagnostic into caller-owned storage.
/// @details This opt-in header owns the canonical strings. Applications can instead switch on ThreadingError and use
/// ResultErrorFormatter::formatMessage with their own translated text.
inline ResultErrorFormat formatThreadingError(ThreadingError error, Span<char> output)
{
    switch (error)
    {
    case ThreadingError::InvalidThreadFunction:
        return ResultErrorFormatter::formatMessage("Thread function is not valid", output);
    case ThreadingError::ThreadAlreadyStarted:
        return ResultErrorFormatter::formatMessage("Thread has already been started", output);
    case ThreadingError::ThreadNotStarted:
        return ResultErrorFormatter::formatMessage("Thread has not been started", output);
    case ThreadingError::ThreadCreationFailed:
        return ResultErrorFormatter::formatMessage("Failed to create thread", output);
    case ThreadingError::ThreadJoinFailed: return ResultErrorFormatter::formatMessage("Failed to join thread", output);
    case ThreadingError::ThreadDetachFailed:
        return ResultErrorFormatter::formatMessage("Failed to detach thread", output);
    case ThreadingError::ThreadPoolAlreadyCreated:
        return ResultErrorFormatter::formatMessage("Thread pool has already been created", output);
    case ThreadingError::InvalidWorkerThreadCount:
        return ResultErrorFormatter::formatMessage("Worker thread count must be greater than zero", output);
    case ThreadingError::ThreadPoolNotCreated:
        return ResultErrorFormatter::formatMessage("Thread pool has not been created", output);
    case ThreadingError::TaskAlreadyQueued:
        return ResultErrorFormatter::formatMessage("Task has already been queued in this thread pool", output);
    case ThreadingError::TaskInUseByAnotherThreadPool:
        return ResultErrorFormatter::formatMessage("Task is in use by another thread pool", output);
    case ThreadingError::ThreadPoolThreadCreationFailed:
        return ResultErrorFormatter::formatMessage("Failed to create thread pool worker", output);
    }
    return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
}

/// @brief Formats a plain Result when it contains a Threading error.
inline ResultErrorFormat formatThreadingError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != ThreadingResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatThreadingError(static_cast<ThreadingError>(result.errorValue()), output);
}

//! @}
} // namespace SC
