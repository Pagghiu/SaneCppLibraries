// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/Result.h"

namespace SC
{
//! @addtogroup group_async
//! @{

/// @brief Stable portable failures owned by Async. Values are append-only.
/// @details Results returned by other libraries or caller work retain their original identity.
enum class AsyncError : uint32_t
{
    AlreadyInitialized = 1,
    NotInitialized,
    RequestInUse,
    RequestNotActive,
    SequencedTimeout,
    TimeoutTransitionInProgress,
    InvalidState,
    InvalidCallback,
    MissingThreadPool,
    InvalidHandle,
    InvalidSignal,
    UnsupportedSignal,
    OperationUnsupported,
    SubmissionAlreadyPending,
    NoPendingSubmission,
    ManualCompletionRequired,
    CompletionAlreadyPosted,
    InvalidSocketHandle,
    InvalidAddress,
    EmptyBuffer,
    MissingAcceptData,
    InvalidFileHandle,
    EmptyTransfer,
    OperationNotSet,
    InvalidPath,
    InvalidSourcePath,
    InvalidDestinationPath,
};

/// @brief Stable category assigned to Async-owned errors.
static constexpr ResultCategory AsyncResultCategory = ResultCategory(13);

//! @}
} // namespace SC
