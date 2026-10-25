// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../../Libraries/Common/Result.h"
namespace SC
{
enum class AsyncExecutionError : uint32_t
{
    OutOfMemory = 1,
    InvalidPath,
    AlreadyRunning,
    Stopped,
    FileTooLarge,
    SocketReceiveIncomplete,
    SocketReceiveNoProgress,
    ReceiveLineBufferExhausted,
    EmptyReceiveBuffer,
    TransferSizeOverflow,
    InvalidFileOffset,
    UnexpectedEndOfFile,
    FileReadNoProgress,
    ScopeClosed,
    InvalidAllocator,
    TimedOut,
    NotificationAlreadyWaiting,
    NotificationContextMismatch,
    ExternalCompletionAlreadyWaiting,
    ExternalCompletionContextMismatch,
    ExternalCompletionConsumed,
    ScopeStorageInUse,
    ScopeCapacityExhausted,
};
static constexpr ResultCategory AsyncExecutionResultCategory = ResultCategory(23);
} // namespace SC
