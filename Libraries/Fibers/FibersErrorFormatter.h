// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/ResultErrorFormatter.h"
#include "FibersError.h"

namespace SC
{
/// @brief Format a Fibers-owned error into caller-provided UTF-8 storage.
/// @details Include this optional header only where canonical English text is needed.
inline ResultErrorFormat formatFibersError(FibersError error, Span<char> output)
{
    ResultErrorFormatter formatter(output);
    switch (error)
    {
    case FibersError::InvalidState: formatter.append("Operation is invalid in the current state"); break;
    case FibersError::WrongOwner: formatter.append("Resource belongs to another owner"); break;
    case FibersError::WrongScheduler: formatter.append("Work belongs to another scheduler"); break;
    case FibersError::WrongExecutionContext:
        formatter.append("Operation requires a different execution context");
        break;
    case FibersError::InvalidConfiguration: formatter.append("Configuration is invalid"); break;
    case FibersError::CapacityExceeded: formatter.append("Capacity limit exceeded"); break;
    case FibersError::StorageTooSmall: formatter.append("Storage is too small"); break;
    case FibersError::GroupStorageTooSmall: formatter.append("Group error storage is too small"); break;
    case FibersError::SlotUnavailable: formatter.append("No slot is available"); break;
    case FibersError::QueueUnavailable: formatter.append("Queue cannot accept more work"); break;
    case FibersError::InvalidProcedure: formatter.append("Procedure is invalid"); break;
    case FibersError::OperationUnsupported: formatter.append("Operation is unsupported"); break;
    case FibersError::Cancelled: formatter.append("Work was cancelled"); break;
    case FibersError::NoProgress: formatter.append("Scheduler made no progress"); break;
    case FibersError::CounterUnderflow: formatter.append("Counter is already zero"); break;
    case FibersError::SynchronizationViolation: formatter.append("Synchronization precondition was violated"); break;
    case FibersError::GroupNotReset: formatter.append("Group must be reset before another work wave"); break;
    case FibersError::UnexpectedWorkerState: formatter.append("Worker has work in an unexpected state"); break;
    case FibersError::AllocationFailed: formatter.append("Allocation failed"); break;
    case FibersError::MemoryReservationFailed: formatter.append("Memory reservation failed"); break;
    case FibersError::MemoryCommitFailed: formatter.append("Memory commitment failed"); break;
    case FibersError::MemoryDecommitFailed: formatter.append("Memory decommitment failed"); break;
    case FibersError::MemoryReleaseFailed: formatter.append("Memory release failed"); break;
    case FibersError::MemoryProtectionFailed: formatter.append("Memory protection failed"); break;
    case FibersError::StackGrowthPreparationFailed: formatter.append("Stack growth preparation failed"); break;
    case FibersError::StackGuardPreparationFailed: formatter.append("Stack guard preparation failed"); break;
    case FibersError::SignalHandlerInstallFailed:
        formatter.append("Stack growth signal handler installation failed");
        break;
    case FibersError::SignalHandlerRestoreFailed:
        formatter.append("Stack growth signal handler restoration failed");
        break;
    case FibersError::SignalStackInstallFailed: formatter.append("Signal stack installation failed"); break;
    case FibersError::SignalStackRestoreFailed: formatter.append("Signal stack restoration failed"); break;
    case FibersError::ThreadStartFailed: formatter.append("Worker thread could not start"); break;
    case FibersError::ThreadJoinFailed: formatter.append("Worker thread could not be joined"); break;
    case FibersError::ThreadPriorityApplyFailed: formatter.append("Worker thread priority could not be applied"); break;
    case FibersError::ThreadAffinityApplyFailed: formatter.append("Worker thread affinity could not be applied"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

inline ResultErrorFormat formatFibersError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != FibersResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatFibersError(static_cast<FibersError>(result.errorValue()), output);
}
} // namespace SC
