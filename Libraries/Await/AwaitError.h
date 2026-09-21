// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/Result.h"

namespace SC
{
//! @addtogroup group_await
//! @{

/// @brief Stable portable failures owned by Await. Values are append-only.
/// @details Results from Async and other libraries retain their original identity.
enum class AwaitError : uint32_t
{
    Cancelled = 1,
    WrongEventLoop,
    AllocatorAlreadyOpen,
    AllocatorStorageEmpty,
    AllocatorReservationEmpty,
    AllocatorReservationFailed,
    AllocatorCommitFailed,
    AllocatorHasLiveAllocations,
    InvalidTask,
    TaskNotCompleted,
    TaskNotStarted,
    TaskAlreadyStarted,
    TaskCancellationUnavailable,
    UnhandledException,
    SocketSendNoProgress,
    SocketReceiveIncomplete,
    SocketReceiveNoProgress,
    EmptyReceiveBuffer,
    ReceiveLineBufferExhausted,
    FileReadNoProgress,
    OperationUnsupported,
    InvalidFileHandle,
    MissingOutputFile,
    MissingFile,
    MissingReadResult,
    InvalidFileSystemOperation,
    TaskGroupStorageFull,
    TaskGroupInvalidTask,
    TaskGroupResultStorageTooSmall,
    TaskGroupInactiveTask,
    TaskGroupEmpty,
    TaskAlreadyAwaited,
};

/// @brief Stable category assigned to Await-owned errors.
static constexpr ResultCategory AwaitResultCategory = ResultCategory(15);

//! @}
} // namespace SC
