// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/ResultErrorFormatter.h"
#include "AsyncError.h"

namespace SC
{
/// @brief Format an Async-owned error into caller-provided UTF-8 storage.
/// @details Include this optional header only where canonical English text is needed.
inline ResultErrorFormat formatAsyncError(AsyncError error, Span<char> output)
{
    ResultErrorFormatter formatter(output);
    switch (error)
    {
    case AsyncError::AlreadyInitialized: formatter.append("Event loop is already initialized"); break;
    case AsyncError::NotInitialized: formatter.append("Event loop is not initialized"); break;
    case AsyncError::RequestInUse: formatter.append("Request is already in use"); break;
    case AsyncError::RequestNotActive: formatter.append("Request is not active"); break;
    case AsyncError::SequencedTimeout: formatter.append("Sequenced timeout cannot be unscheduled"); break;
    case AsyncError::TimeoutTransitionInProgress: formatter.append("Timeout is changing state"); break;
    case AsyncError::InvalidState: formatter.append("Operation is invalid in the current state"); break;
    case AsyncError::InvalidCallback: formatter.append("Work callback is invalid"); break;
    case AsyncError::MissingThreadPool: formatter.append("Thread pool has not been set"); break;
    case AsyncError::InvalidHandle: formatter.append("Handle is invalid"); break;
    case AsyncError::InvalidSignal: formatter.append("Signal number is invalid"); break;
    case AsyncError::UnsupportedSignal: formatter.append("Signal cannot be watched"); break;
    case AsyncError::OperationUnsupported: formatter.append("Operation is unsupported"); break;
    case AsyncError::SubmissionAlreadyPending: formatter.append("Submission is already pending"); break;
    case AsyncError::NoPendingSubmission: formatter.append("No submission is pending"); break;
    case AsyncError::ManualCompletionRequired: formatter.append("Manual completion mode is required"); break;
    case AsyncError::CompletionAlreadyPosted: formatter.append("Completion has already been posted"); break;
    case AsyncError::InvalidSocketHandle: formatter.append("Socket handle is invalid"); break;
    case AsyncError::InvalidAddress: formatter.append("Socket address is invalid"); break;
    case AsyncError::EmptyBuffer: formatter.append("Buffer is empty"); break;
    case AsyncError::MissingAcceptData: formatter.append("Accept request data is missing"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

inline ResultErrorFormat formatAsyncError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != AsyncResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatAsyncError(static_cast<AsyncError>(result.errorValue()), output);
}
} // namespace SC
