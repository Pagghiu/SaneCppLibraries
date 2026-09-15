// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/ResultErrorFormatter.h"
#include "FileSystemWatcherError.h"

namespace SC
{
//! @addtogroup group_file_system_watcher
//! @{

namespace detail
{
inline bool appendFileSystemWatcherErrorDetail(ResultErrorFormatter& formatter, FileSystemWatcherErrorDetail detail)
{
    switch (detail)
    {
    case FileSystemWatcherErrorDetail::None: return true;
    case FileSystemWatcherErrorDetail::AssignWatchPath: formatter.append("assign watch path"); break;
    case FileSystemWatcherErrorDetail::BuildSubfolderPath: formatter.append("build subfolder path"); break;
    case FileSystemWatcherErrorDetail::BuildNotificationPath: formatter.append("build notification path"); break;
    case FileSystemWatcherErrorDetail::BuildFullPath: formatter.append("build full path"); break;
    case FileSystemWatcherErrorDetail::WatchPathCapacity: formatter.append("watch path capacity"); break;
    case FileSystemWatcherErrorDetail::WorkerAlreadyStarted: formatter.append("worker already started"); break;
    case FileSystemWatcherErrorDetail::PosixPthreadCreate: formatter.append("POSIX create thread"); break;
    case FileSystemWatcherErrorDetail::PosixPthreadJoin: formatter.append("POSIX join thread"); break;
    case FileSystemWatcherErrorDetail::WindowsCreateThread: formatter.append("Windows create thread"); break;
    case FileSystemWatcherErrorDetail::WindowsWaitForSingleObject: formatter.append("Windows wait for thread"); break;
    case FileSystemWatcherErrorDetail::LinuxCreateShutdownPipe: formatter.append("Linux create shutdown pipe"); break;
    case FileSystemWatcherErrorDetail::LinuxInitializeInotify: formatter.append("Linux initialize inotify"); break;
    case FileSystemWatcherErrorDetail::LinuxWriteShutdownPipe: formatter.append("Linux write shutdown pipe"); break;
    case FileSystemWatcherErrorDetail::LinuxInotifyDescriptor: formatter.append("Linux inotify descriptor"); break;
    case FileSystemWatcherErrorDetail::LinuxRemoveWatch: formatter.append("Linux remove watch"); break;
    case FileSystemWatcherErrorDetail::LinuxAddRootWatch: formatter.append("Linux add root watch"); break;
    case FileSystemWatcherErrorDetail::LinuxAddSubdirectoryWatch:
        formatter.append("Linux add subdirectory watch");
        break;
    case FileSystemWatcherErrorDetail::LinuxOpenRootDirectory: formatter.append("Linux open root directory"); break;
    case FileSystemWatcherErrorDetail::LinuxWatchHandleCapacity: formatter.append("Linux watch handle capacity"); break;
    case FileSystemWatcherErrorDetail::LinuxPendingDirectoryCapacity:
        formatter.append("Linux pending directory capacity");
        break;
    case FileSystemWatcherErrorDetail::LinuxRelativePathStorage: formatter.append("Linux relative path storage"); break;
    case FileSystemWatcherErrorDetail::AppleRunLoop: formatter.append("Apple run loop"); break;
    case FileSystemWatcherErrorDetail::AppleCreateRunLoopSource:
        formatter.append("Apple create run loop source");
        break;
    case FileSystemWatcherErrorDetail::AppleAllocateWatchPaths: formatter.append("Apple allocate watch paths"); break;
    case FileSystemWatcherErrorDetail::AppleCreateWatchPathString:
        formatter.append("Apple create watch path string");
        break;
    case FileSystemWatcherErrorDetail::AppleCreateWatchPathsArray:
        formatter.append("Apple create watch paths array");
        break;
    case FileSystemWatcherErrorDetail::AppleCreateEventStream: formatter.append("Apple create event stream"); break;
    case FileSystemWatcherErrorDetail::AppleStartEventStream: formatter.append("Apple start event stream"); break;
    case FileSystemWatcherErrorDetail::WindowsOpenDirectory: formatter.append("Windows open directory"); break;
    case FileSystemWatcherErrorDetail::WindowsSubmitDirectoryChanges:
        formatter.append("Windows submit directory changes");
        break;
    default: return false;
    }
    return true;
}

inline ResultErrorFormat formatFileSystemWatcherErrorWithDetails(FileSystemWatcherError       error,
                                                                 FileSystemWatcherErrorDetail detail,
                                                                 uint32_t nativeError, Span<char> output)
{
    ResultErrorFormatter formatter(output);
    switch (error)
    {
    case FileSystemWatcherError::NotInitialized: formatter.append("File system watcher is not initialized"); break;
    case FileSystemWatcherError::AlreadyWatching: formatter.append("Folder watcher is already watching"); break;
    case FileSystemWatcherError::NotWatching: formatter.append("Folder watcher is not watching"); break;
    case FileSystemWatcherError::UnsupportedPathEncoding: formatter.append("Watch path encoding is unsupported"); break;
    case FileSystemWatcherError::PathPreparationFailed: formatter.append("Failed to prepare watch path"); break;
    case FileSystemWatcherError::WatchLimitExceeded:
        formatter.append("File system watcher watch limit exceeded");
        break;
    case FileSystemWatcherError::BufferTooSmall: formatter.append("File system watcher buffer is too small"); break;
    case FileSystemWatcherError::InitializationFailed:
        formatter.append("Failed to initialize file system watcher");
        break;
    case FileSystemWatcherError::WatchSetupFailed: formatter.append("Failed to set up file system watch"); break;
    case FileSystemWatcherError::StopWatchingFailed: formatter.append("Failed to stop file system watch"); break;
    case FileSystemWatcherError::CloseFailed: formatter.append("Failed to close file system watcher"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }

    if (detail != FileSystemWatcherErrorDetail::None)
    {
        formatter.append(" (detail: ");
        if (not appendFileSystemWatcherErrorDetail(formatter, detail))
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

inline ResultErrorFormat formatFileSystemWatcherError(FileSystemWatcherError error, Span<char> output)
{
    return detail::formatFileSystemWatcherErrorWithDetails(error, FileSystemWatcherErrorDetail::None, 0, output);
}

inline ResultErrorFormat formatFileSystemWatcherError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != FileSystemWatcherResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatFileSystemWatcherError(static_cast<FileSystemWatcherError>(result.errorValue()), output);
}

inline ResultErrorFormat formatFileSystemWatcherError(ResultFileSystemWatcher result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.result.category() != FileSystemWatcherResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return detail::formatFileSystemWatcherErrorWithDetails(
        static_cast<FileSystemWatcherError>(result.result.errorValue()), result.detail, result.nativeError, output);
}

//! @}
} // namespace SC
