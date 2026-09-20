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
    case AsyncError::InvalidFileHandle: formatter.append("File handle is invalid"); break;
    case AsyncError::EmptyTransfer: formatter.append("Transfer length is zero"); break;
    case AsyncError::OperationNotSet: formatter.append("File-system operation has not been set"); break;
    case AsyncError::InvalidPath: formatter.append("Path is invalid"); break;
    case AsyncError::InvalidSourcePath: formatter.append("Source path is invalid"); break;
    case AsyncError::InvalidDestinationPath: formatter.append("Destination path is invalid"); break;
    case AsyncError::EventLoopCreationFailed: formatter.append("Event loop could not be created"); break;
    case AsyncError::WakeUpInitializationFailed: formatter.append("Event-loop wake-up could not be initialized"); break;
    case AsyncError::WakeUpFailed: formatter.append("Event loop could not be woken"); break;
    case AsyncError::InvalidEventLoopHandle: formatter.append("Event-loop handle is invalid"); break;
    case AsyncError::InvalidWakeUpHandle: formatter.append("Event-loop wake-up handle is invalid"); break;
    case AsyncError::WatcherRegistrationFailed: formatter.append("Event watcher could not be registered"); break;
    case AsyncError::WatcherRemovalFailed: formatter.append("Event watcher could not be removed"); break;
    case AsyncError::EventLoopFlushFailed: formatter.append("Event-loop changes could not be submitted"); break;
    case AsyncError::EventLoopPollFailed: formatter.append("Event loop could not poll for events"); break;
    case AsyncError::EventCompletionFailed: formatter.append("Event could not be completed"); break;
    case AsyncError::SubmissionCapacityExhausted:
        formatter.append("Event-loop submission capacity is exhausted");
        break;
    case AsyncError::SubmissionFailed: formatter.append("Event-loop submission failed"); break;
    case AsyncError::ProcessWatcherCreationFailed: formatter.append("Process watcher could not be created"); break;
    case AsyncError::ProcessWatcherRemovalFailed: formatter.append("Process watcher could not be removed"); break;
    case AsyncError::ProcessWaitFailed: formatter.append("Process exit could not be observed"); break;
    case AsyncError::SignalWatcherCreationFailed: formatter.append("Signal watcher could not be created"); break;
    case AsyncError::SignalWatcherRemovalFailed: formatter.append("Signal watcher could not be removed"); break;
    case AsyncError::SignalReadFailed: formatter.append("Signal event could not be read"); break;
    case AsyncError::SignalSubscriberLimitReached: formatter.append("Signal subscriber limit was reached"); break;
    case AsyncError::InvalidEventIndex: formatter.append("Event index is invalid"); break;
    case AsyncError::CancellationFailed: formatter.append("Operation could not be cancelled"); break;
    case AsyncError::SocketConnectFailed: formatter.append("Socket could not connect"); break;
    case AsyncError::SocketSendFailed: formatter.append("Socket data could not be sent"); break;
    case AsyncError::SocketReceiveFailed: formatter.append("Socket data could not be received"); break;
    case AsyncError::SocketSendIncomplete: formatter.append("Socket data was only partially sent"); break;
    case AsyncError::FileWriteFailed: formatter.append("File data could not be written"); break;
    case AsyncError::FileWriteIncomplete: formatter.append("File data was only partially written"); break;
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
