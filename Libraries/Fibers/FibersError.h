// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/Result.h"

namespace SC
{
//! @addtogroup group_fibers
//! @{

/// @brief Stable portable failures owned by Fibers. Values are append-only.
/// @details Procedure results from callers are not translated into this domain.
enum class FibersError : uint32_t
{
    InvalidState = 1,
    WrongOwner,
    WrongScheduler,
    WrongExecutionContext,
    InvalidConfiguration,
    CapacityExceeded,
    StorageTooSmall,
    GroupStorageTooSmall,
    SlotUnavailable,
    QueueUnavailable,
    InvalidProcedure,
    OperationUnsupported,
    Cancelled,
    NoProgress,
    CounterUnderflow,
    SynchronizationViolation,
    GroupNotReset,
    UnexpectedWorkerState,
    AllocationFailed,
    MemoryReservationFailed,
    MemoryCommitFailed,
    MemoryDecommitFailed,
    MemoryReleaseFailed,
    MemoryProtectionFailed,
    StackGrowthPreparationFailed,
    StackGuardPreparationFailed,
    SignalHandlerInstallFailed,
    SignalHandlerRestoreFailed,
    SignalStackInstallFailed,
    SignalStackRestoreFailed,
    ThreadStartFailed,
    ThreadJoinFailed,
    ThreadPriorityApplyFailed,
    ThreadAffinityApplyFailed,
};

/// @brief Stable category assigned to Fibers-owned errors.
static constexpr ResultCategory FibersResultCategory = ResultCategory(12);

//! @}
} // namespace SC
