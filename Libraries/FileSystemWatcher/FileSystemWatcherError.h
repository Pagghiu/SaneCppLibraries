// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/Result.h"

namespace SC
{
//! @addtogroup group_file_system_watcher
//! @{

/// @brief Stable portable failures returned by the FileSystemWatcher library.
enum class FileSystemWatcherError : uint32_t
{
    NotInitialized = 1,
    AlreadyWatching,
    NotWatching,
    UnsupportedPathEncoding,
    PathPreparationFailed,
    WatchLimitExceeded,
    BufferTooSmall,
    InitializationFailed,
    WatchSetupFailed,
    StopWatchingFailed,
    CloseFailed,
};

/// @brief Stable backend stages retained as context for a FileSystemWatcher failure.
enum class FileSystemWatcherErrorDetail : uint32_t
{
    None = 0,
    AssignWatchPath,
    BuildSubfolderPath,
    BuildNotificationPath,
    BuildFullPath,
    WatchPathCapacity,
    WorkerAlreadyStarted,
    PosixPthreadCreate,
    PosixPthreadJoin,
    WindowsCreateThread,
    WindowsWaitForSingleObject,
    LinuxCreateShutdownPipe,
    LinuxInitializeInotify,
    LinuxWriteShutdownPipe,
    LinuxInotifyDescriptor,
    LinuxRemoveWatch,
    LinuxAddRootWatch,
    LinuxAddSubdirectoryWatch,
    LinuxOpenRootDirectory,
    LinuxWatchHandleCapacity,
    LinuxPendingDirectoryCapacity,
    LinuxRelativePathStorage,
    AppleRunLoop,
    AppleCreateRunLoopSource,
    AppleAllocateWatchPaths,
    AppleCreateWatchPathString,
    AppleCreateWatchPathsArray,
    AppleCreateEventStream,
    AppleStartEventStream,
    WindowsOpenDirectory,
    WindowsSubmitDirectoryChanges,
};

/// @brief Stable category assigned to errors owned by FileSystemWatcher.
static constexpr ResultCategory FileSystemWatcherResultCategory = ResultCategory(6);

/// @brief FileSystemWatcher result retaining backend-stage and native-error context.
/// @details The composed Result is authoritative. Context is retained only for this category and is cleared when a
/// plain or foreign result is converted into this type. Converting to Result preserves category and error identity and
/// deliberately discards context.
struct [[nodiscard]] ResultFileSystemWatcher
{
    Result                       result;
    FileSystemWatcherErrorDetail detail      = FileSystemWatcherErrorDetail::None;
    uint32_t                     nativeError = 0;

    explicit constexpr ResultFileSystemWatcher(bool valid = true) : result(valid) {}
    constexpr ResultFileSystemWatcher(FileSystemWatcherError       error,
                                      FileSystemWatcherErrorDetail detail      = FileSystemWatcherErrorDetail::None,
                                      uint32_t                     nativeError = 0)
        : result(Result::Error(FileSystemWatcherResultCategory, error)), detail(detail), nativeError(nativeError)
    {}
    constexpr ResultFileSystemWatcher(Result result) : result(result) {}

    template <typename ResultLike>
    constexpr ResultFileSystemWatcher(const ResultLike& other) : result(other.toResult())
    {}

    explicit constexpr operator bool() const { return static_cast<bool>(result); }

    constexpr operator Result() const { return result; }

    constexpr Result toResult() const { return result; }
    constexpr bool   isError(FileSystemWatcherError error) const
    {
        return result.isError(FileSystemWatcherResultCategory, error);
    }
};

//! @}
} // namespace SC
