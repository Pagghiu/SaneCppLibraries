// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Common/ResultErrorFormatter.h"
#include "ThreadingError.h"

namespace SC
{
//! @addtogroup group_threading
//! @{

namespace detail
{
inline ResultErrorFormat formatThreadingErrorWithContext(ThreadingError error, ThreadingErrorDetail errorDetail,
                                                         uint32_t nativeError, Span<char> output)
{
    ResultErrorFormatter formatter(output);
    switch (error)
    {
    case ThreadingError::InvalidThreadFunction: formatter.append("Thread function is not valid"); break;
    case ThreadingError::ThreadAlreadyStarted: formatter.append("Thread has already been started"); break;
    case ThreadingError::ThreadNotStarted: formatter.append("Thread has not been started"); break;
    case ThreadingError::ThreadCreationFailed: formatter.append("Failed to create thread"); break;
    case ThreadingError::ThreadJoinFailed: formatter.append("Failed to join thread"); break;
    case ThreadingError::ThreadDetachFailed: formatter.append("Failed to detach thread"); break;
    case ThreadingError::ThreadPoolAlreadyCreated: formatter.append("Thread pool has already been created"); break;
    case ThreadingError::InvalidWorkerThreadCount:
        formatter.append("Worker thread count must be greater than zero");
        break;
    case ThreadingError::ThreadPoolNotCreated: formatter.append("Thread pool has not been created"); break;
    case ThreadingError::TaskAlreadyQueued: formatter.append("Task has already been queued in this thread pool"); break;
    case ThreadingError::TaskInUseByAnotherThreadPool: formatter.append("Task is in use by another thread pool"); break;
    case ThreadingError::ThreadPoolThreadCreationFailed: formatter.append("Failed to create thread pool worker"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }

    if (errorDetail != ThreadingErrorDetail::None)
    {
        formatter.append(" (detail: ");
        switch (errorDetail)
        {
        case ThreadingErrorDetail::PosixPthreadCreate: formatter.append("POSIX create thread"); break;
        case ThreadingErrorDetail::PosixPthreadJoin: formatter.append("POSIX join thread"); break;
        case ThreadingErrorDetail::PosixPthreadDetach: formatter.append("POSIX detach thread"); break;
        case ThreadingErrorDetail::WindowsCreateThread: formatter.append("Windows create thread"); break;
        case ThreadingErrorDetail::WindowsWaitForSingleObject: formatter.append("Windows wait for thread"); break;
        case ThreadingErrorDetail::WindowsCloseHandle: formatter.append("Windows close thread handle"); break;
        default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
        }
        if (nativeError != 0)
        {
            formatter.append(", native error: ");
            formatter.append(static_cast<uint64_t>(nativeError));
        }
        formatter.append(")");
    }
    else if (nativeError != 0 and
             (error == ThreadingError::ThreadCreationFailed or error == ThreadingError::ThreadJoinFailed or
              error == ThreadingError::ThreadDetachFailed or error == ThreadingError::ThreadPoolThreadCreationFailed))
    {
        formatter.append(" (native error: ");
        formatter.append(static_cast<uint64_t>(nativeError));
        formatter.append(")");
    }
    return formatter.finish();
}
} // namespace detail

/// @brief Formats a canonical English Threading diagnostic into caller-owned storage.
/// @details This opt-in header owns the canonical strings. Applications can instead switch on ThreadingError and use
/// ResultErrorFormatter with their own translated text.
inline ResultErrorFormat formatThreadingError(ThreadingError error, Span<char> output)
{
    return detail::formatThreadingErrorWithContext(error, ThreadingErrorDetail::None, 0, output);
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

/// @brief Formats a Threading result, including its native error number when available.
inline ResultErrorFormat formatThreadingError(ResultThreading result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.result.category() != ThreadingResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return detail::formatThreadingErrorWithContext(static_cast<ThreadingError>(result.result.errorValue()),
                                                   result.detail, result.nativeError, output);
}

//! @}
} // namespace SC
