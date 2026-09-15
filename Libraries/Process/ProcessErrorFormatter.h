// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Common/ResultErrorFormatter.h"
#include "Process.h"

namespace SC
{
//! @addtogroup group_process
//! @{

namespace detail
{
inline ResultErrorFormat formatProcessErrorWithNativeCode(ProcessError error, uint32_t nativeError, Span<char> output)
{
    ResultErrorFormatter formatter(output);
    switch (error)
    {
    case ProcessError::ProcessChainEmpty: formatter.append("Process chain has no processes"); break;
    case ProcessError::ProcessAlreadyInChain: formatter.append("Process already belongs to a process chain"); break;
    case ProcessError::InvalidInputRedirection: formatter.append("Process input redirection is invalid"); break;
    case ProcessError::InvalidOutputRedirection: formatter.append("Process output redirection is invalid"); break;
    case ProcessError::UnsupportedInputRedirection: formatter.append("Process input redirection is unsupported"); break;
    case ProcessError::UnsupportedOutputRedirection:
        formatter.append("Process output redirection is unsupported");
        break;
    case ProcessError::StringCountCapacityExceeded: formatter.append("Process string count capacity exceeded"); break;
    case ProcessError::StringDestinationCapacityExceeded:
        formatter.append("Process string destination capacity exceeded");
        break;
    case ProcessError::EnvironmentCapacityExceeded: formatter.append("Process environment capacity exceeded"); break;
    case ProcessError::WaitFailed: formatter.append("Failed to wait for process"); break;
    case ProcessError::ForkFailed: formatter.append("Failed to fork process"); break;
    case ProcessError::ExecFailed: formatter.append("Failed to execute process"); break;
    case ProcessError::DuplicateDescriptorFailed: formatter.append("Failed to duplicate process descriptor"); break;
    case ProcessError::ResetSignalHandlersFailed: formatter.append("Failed to reset child signal handlers"); break;
    case ProcessError::ChangeWorkingDirectoryFailed:
        formatter.append("Failed to change process working directory");
        break;
    case ProcessError::SetHandleInformationFailed: formatter.append("Failed to set process handle information"); break;
    case ProcessError::CreateProcessFailed: formatter.append("Failed to create process"); break;
    case ProcessError::ForkWaitFailed: formatter.append("Failed to wait for forked process"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }

    if (nativeError != 0)
    {
        formatter.append(" (native error: ");
        formatter.append(static_cast<uint64_t>(nativeError));
        formatter.append(")");
    }
    return formatter.finish();
}
} // namespace detail

/// @brief Formats a canonical English Process diagnostic into caller-owned storage.
inline ResultErrorFormat formatProcessError(ProcessError error, Span<char> output)
{
    return detail::formatProcessErrorWithNativeCode(error, 0, output);
}

/// @brief Formats a plain Result when it contains a Process error.
inline ResultErrorFormat formatProcessError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != ProcessResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatProcessError(static_cast<ProcessError>(result.errorValue()), output);
}

/// @brief Formats a Process result, including its native error number when available.
inline ResultErrorFormat formatProcessError(ResultProcess result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.result.category() != ProcessResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return detail::formatProcessErrorWithNativeCode(static_cast<ProcessError>(result.result.errorValue()),
                                                    result.nativeError, output);
}

//! @}
} // namespace SC
