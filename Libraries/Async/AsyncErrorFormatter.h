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
