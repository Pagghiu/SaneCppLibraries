// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/Result.h"

namespace SC
{
//! @addtogroup group_async_fibers
//! @{

/// @brief Stable portable failures owned by the AsyncFibers bridge. Values are append-only.
/// @details Errors returned by Async, File, Socket, Fibers, or caller work keep their original identity.
enum class AsyncFibersError : uint32_t
{
    Cancelled = 1,
    SocketSendNoProgress,
    InvalidFileHandle,
    UnexpectedEndOfFile,
    WrongOwnerThread,
    FiberContextRequired,
    CommandStorageEmpty,
    InvalidCommand,
    CommandQueueFull,
};

/// @brief Stable category assigned to AsyncFibers-owned errors.
static constexpr ResultCategory AsyncFibersResultCategory = ResultCategory(14);

//! @}
} // namespace SC
