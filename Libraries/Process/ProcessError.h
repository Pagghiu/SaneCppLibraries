// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/Result.h"

namespace SC
{
//! @addtogroup group_process
//! @{

/// @brief Stable error codes returned by the Process library.
enum class ProcessError : uint32_t
{
    ProcessChainEmpty = 1,
    ProcessAlreadyInChain,
    InvalidInputRedirection,
    InvalidOutputRedirection,
    UnsupportedInputRedirection,
    UnsupportedOutputRedirection,
    ArgumentCapacityExceeded,
    EnvironmentCapacityExceeded,
    WaitFailed,
    LaunchFailed,
    CloneFailed,
    PathCapacityExceeded,
    PathMustBeAbsolute,
    InvalidPath,
    PathResolutionFailed,
};

/// @brief Stable diagnostic stages for Process failures.
/// @details Details are only meaningful on a failing ResultProcess and are deliberately not encoded in Result.
enum class ProcessErrorDetail : uint32_t
{
    None = 0,
    PosixWaitPid,
    WindowsGetExitCodeProcess,
    PosixFork,
    PosixExec,
    PosixBuildResolveExecutablePath,
    PosixDuplicateDescriptor,
    PosixResetSignalHandlers,
    PosixChangeWorkingDirectory,
    WindowsSetHandleInformation,
    WindowsCreateProcess,
    WindowsRtlCloneUserProcess,
    WindowsNtWaitForSingleObject,
    WindowsResolveWorkingDirectory,
    WindowsPrepareExecutableTransportPath,
    WindowsPrepareWorkingDirectoryTransportPath,
    WindowsNormalizeExecutablePath,
};

/// @brief Stable category assigned to errors owned by the Process library.
static constexpr ResultCategory ProcessResultCategory = ResultCategory(5);

/// @brief Process result retaining optional backend and native platform details.
/// @details detail and nativeError are zero when unavailable or irrelevant. Converting to Result preserves the
/// portable error identity and deliberately discards both diagnostic details.
struct [[nodiscard]] ResultProcess
{
    Result             result;
    ProcessErrorDetail detail      = ProcessErrorDetail::None;
    uint32_t           nativeError = 0;

    explicit constexpr ResultProcess(bool valid = true) : result(valid) {}
    constexpr ResultProcess(ProcessError error, ProcessErrorDetail detail = ProcessErrorDetail::None,
                            uint32_t nativeError = 0)
        : result(Result::Error(ProcessResultCategory, error)), detail(detail), nativeError(nativeError)
    {}
    constexpr ResultProcess(Result result) : result(result) {}

    template <typename ResultLike>
    constexpr ResultProcess(const ResultLike& other) : result(other.toResult())
    {}

    explicit constexpr operator bool() const { return static_cast<bool>(result); }

    constexpr operator Result() const { return result; }

    constexpr Result toResult() const { return result; }
    constexpr bool   isError(ProcessError error) const { return result.isError(ProcessResultCategory, error); }
};

//! @}
} // namespace SC
