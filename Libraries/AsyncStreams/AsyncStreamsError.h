// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/Result.h"

namespace SC
{
//! @addtogroup group_async_streams
//! @{

/// @brief Stable portable failures owned by AsyncStreams. Values are append-only.
/// @details Errors propagated from another library retain their original category and value.
enum class AsyncStreamsError : uint32_t
{
    InvalidBufferID = 1,
    InvalidParentBufferID,
    BufferNotWritable,
    NoReusableBuffer,
    BufferPoolFull,
    InsufficientSliceStorage,
    InvalidSliceCount,
    ChildViewOutOfBounds,
    InvalidRootBufferType,
    ReadableNotReady,
    ReadQueueMissing,
    EmptyReadBuffer,
    ReadQueueFull,
    InvalidReadableState,
    ReadReactivationMissing,
    ReadableDestroying,
    ReadableEnded,
    ReadableNotInitialized,
    ReadableErrored,
    InvalidWritableState,
    WriteQueueMissing,
    WriteAfterEnd,
    WriteQueueFull,
    WritableDestroying,
    WritableEndAlreadyCalled,
};

/// @brief Stable category assigned to AsyncStreams-owned errors.
static constexpr ResultCategory AsyncStreamsResultCategory = ResultCategory(16);

//! @}
} // namespace SC
