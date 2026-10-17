// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Common/Result.h"
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
};
static constexpr ResultCategory AsyncExecutionResultCategory = ResultCategory(23);
} // namespace SC
