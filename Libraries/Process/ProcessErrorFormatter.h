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
inline bool appendProcessErrorDetail(ResultErrorFormatter& formatter, ProcessErrorDetail detail)
{
    switch (detail)
    {
    case ProcessErrorDetail::None: return true;
    case ProcessErrorDetail::PosixWaitPid: formatter.append("POSIX wait for child"); break;
    case ProcessErrorDetail::WindowsGetExitCodeProcess: formatter.append("Windows query process exit code"); break;
    case ProcessErrorDetail::PosixFork: formatter.append("POSIX create child process"); break;
    case ProcessErrorDetail::PosixExec: formatter.append("POSIX execute program"); break;
    case ProcessErrorDetail::PosixBuildResolveExecutablePath: formatter.append("POSIX build executable path"); break;
    case ProcessErrorDetail::PosixDuplicateDescriptor: formatter.append("POSIX duplicate file descriptor"); break;
    case ProcessErrorDetail::PosixResetSignalHandlers: formatter.append("POSIX reset signal handlers"); break;
    case ProcessErrorDetail::PosixChangeWorkingDirectory: formatter.append("POSIX change working directory"); break;
    case ProcessErrorDetail::WindowsSetHandleInformation: formatter.append("Windows prepare inherited handle"); break;
    case ProcessErrorDetail::WindowsCreateProcess: formatter.append("Windows create process"); break;
    case ProcessErrorDetail::WindowsRtlCloneUserProcess: formatter.append("Windows clone process"); break;
    case ProcessErrorDetail::WindowsNtWaitForSingleObject: formatter.append("Windows wait for child"); break;
    case ProcessErrorDetail::WindowsResolveWorkingDirectory:
        formatter.append("Windows resolve working directory");
        break;
    default: return false;
    }
    return true;
}

inline ResultErrorFormat formatProcessErrorWithDetails(ProcessError error, ProcessErrorDetail detail,
                                                       uint32_t nativeError, Span<char> output)
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
    case ProcessError::ArgumentCapacityExceeded: formatter.append("Process argument capacity exceeded"); break;
    case ProcessError::EnvironmentCapacityExceeded: formatter.append("Process environment capacity exceeded"); break;
    case ProcessError::WaitFailed: formatter.append("Failed to wait for process"); break;
    case ProcessError::LaunchFailed: formatter.append("Failed to launch process"); break;
    case ProcessError::CloneFailed: formatter.append("Failed to clone process"); break;
    case ProcessError::PathCapacityExceeded: formatter.append("Process path capacity exceeded"); break;
    case ProcessError::PathMustBeAbsolute: formatter.append("Process path must be absolute"); break;
    case ProcessError::InvalidPath: formatter.append("Process path is invalid"); break;
    case ProcessError::PathResolutionFailed: formatter.append("Failed to resolve process path"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }

    if (detail != ProcessErrorDetail::None)
    {
        formatter.append(" (detail: ");
        if (not appendProcessErrorDetail(formatter, detail))
            return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
        if (nativeError != 0)
        {
            formatter.append(", native error: ");
            formatter.append(static_cast<uint64_t>(nativeError));
        }
        formatter.append(")");
    }
    else if (nativeError != 0)
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
    return detail::formatProcessErrorWithDetails(error, ProcessErrorDetail::None, 0, output);
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

/// @brief Formats a Process result, including its backend detail and native error number when available.
inline ResultErrorFormat formatProcessError(ResultProcess result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.result.category() != ProcessResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return detail::formatProcessErrorWithDetails(static_cast<ProcessError>(result.result.errorValue()), result.detail,
                                                 result.nativeError, output);
}

//! @}
} // namespace SC
