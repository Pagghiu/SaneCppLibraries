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
    case FileSystemWatcherErrorDetail::WindowsCreateWatchEvent: formatter.append("Windows create watch event"); break;
    case FileSystemWatcherErrorDetail::WindowsSignalWatchEvent: formatter.append("Windows signal watch event"); break;
    case FileSystemWatcherErrorDetail::WindowsWaitForWatchEvents:
        formatter.append("Windows wait for watch events");
        break;
    case FileSystemWatcherErrorDetail::WindowsCancelDirectoryChanges:
        formatter.append("Windows cancel directory changes");
        break;
    case FileSystemWatcherErrorDetail::PosixCreateShutdownPipe: formatter.append("POSIX create shutdown pipe"); break;
    case FileSystemWatcherErrorDetail::InotifyInitialize: formatter.append("inotify initialize"); break;
    case FileSystemWatcherErrorDetail::PosixWriteShutdownPipe: formatter.append("POSIX write shutdown pipe"); break;
    case FileSystemWatcherErrorDetail::InotifyDescriptor: formatter.append("inotify descriptor"); break;
    case FileSystemWatcherErrorDetail::InotifyRemoveWatch: formatter.append("inotify remove watch"); break;
    case FileSystemWatcherErrorDetail::InotifyAddRootWatch: formatter.append("inotify add root watch"); break;
    case FileSystemWatcherErrorDetail::InotifyAddSubdirectoryWatch:
        formatter.append("inotify add subdirectory watch");
        break;
    case FileSystemWatcherErrorDetail::InotifyOpenRootDirectory: formatter.append("inotify open root directory"); break;
    case FileSystemWatcherErrorDetail::InotifyOpenDirectoryStream:
        formatter.append("inotify open directory stream");
        break;
    case FileSystemWatcherErrorDetail::InotifyReadDirectoryEntries:
        formatter.append("inotify read directory entries");
        break;
    case FileSystemWatcherErrorDetail::InotifyInspectDirectoryEntry:
        formatter.append("inotify inspect directory entry");
        break;
    case FileSystemWatcherErrorDetail::InotifyOpenSubdirectory: formatter.append("inotify open subdirectory"); break;
    case FileSystemWatcherErrorDetail::InotifyReadEvents: formatter.append("inotify read events"); break;
    case FileSystemWatcherErrorDetail::InotifyVerifyDirectoryIdentity:
        formatter.append("inotify verify directory identity");
        break;
    case FileSystemWatcherErrorDetail::InotifyWatchInvalidated: formatter.append("inotify watch invalidated"); break;
    case FileSystemWatcherErrorDetail::InotifyWaitForEvents: formatter.append("inotify wait for events"); break;
    case FileSystemWatcherErrorDetail::KqueueCreate: formatter.append("kqueue create"); break;
    case FileSystemWatcherErrorDetail::KqueueRegisterControlEvent:
        formatter.append("kqueue register control event");
        break;
    case FileSystemWatcherErrorDetail::KqueueTriggerControlEvent:
        formatter.append("kqueue trigger control event");
        break;
    case FileSystemWatcherErrorDetail::KqueueOpenRootDirectory: formatter.append("kqueue open root directory"); break;
    case FileSystemWatcherErrorDetail::KqueueInventory: formatter.append("kqueue build inventory"); break;
    case FileSystemWatcherErrorDetail::KqueueReconcile: formatter.append("kqueue reconcile inventory"); break;
    case FileSystemWatcherErrorDetail::KqueueWaitForEvents: formatter.append("kqueue wait for events"); break;
    case FileSystemWatcherErrorDetail::KqueueReadEvents: formatter.append("kqueue read events"); break;
    case FileSystemWatcherErrorDetail::BSDPassiveEntries: formatter.append("BSD passive entries"); break;
    case FileSystemWatcherErrorDetail::BSDEntryStorage: formatter.append("BSD entry storage"); break;
    case FileSystemWatcherErrorDetail::KqueueCleanup: formatter.append("kqueue cleanup"); break;
    case FileSystemWatcherErrorDetail::KqueueVnodeRevoked: formatter.append("kqueue vnode revoked"); break;
    case FileSystemWatcherErrorDetail::InotifyWatchHandleCapacity:
        formatter.append("inotify watch handle capacity");
        break;
    case FileSystemWatcherErrorDetail::InotifyPendingDirectoryCapacity:
        formatter.append("inotify pending directory capacity");
        break;
    case FileSystemWatcherErrorDetail::InotifyRelativePathStorage:
        formatter.append("inotify relative path storage");
        break;
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
    case FileSystemWatcherErrorDetail::WindowsCompleteDirectoryChanges:
        formatter.append("Windows complete directory changes");
        break;
    case FileSystemWatcherErrorDetail::AppleOpenRootDirectory: formatter.append("Apple open root directory"); break;
    case FileSystemWatcherErrorDetail::AppleResolveWatchRoot: formatter.append("Apple resolve watch root"); break;
    case FileSystemWatcherErrorDetail::AppleWatchInvalidated: formatter.append("Apple watch invalidated"); break;
    case FileSystemWatcherErrorDetail::WindowsQueryRootDirectory:
        formatter.append("Windows query root directory");
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
    case FileSystemWatcherError::NotInitialized: formatter.append("Watcher is not initialized"); break;
    case FileSystemWatcherError::AlreadyWatching: formatter.append("Folder watcher is already watching"); break;
    case FileSystemWatcherError::NotWatching: formatter.append("Folder watcher is not watching"); break;
    case FileSystemWatcherError::UnsupportedPathEncoding: formatter.append("Path encoding is not supported"); break;
    case FileSystemWatcherError::PathPreparationFailed: formatter.append("Failed to prepare path"); break;
    case FileSystemWatcherError::WatchLimitExceeded: formatter.append("Watch capacity exceeded"); break;
    case FileSystemWatcherError::BufferTooSmall: formatter.append("Buffer capacity is insufficient"); break;
    case FileSystemWatcherError::InitializationFailed: formatter.append("Failed to initialize watcher"); break;
    case FileSystemWatcherError::WatchSetupFailed: formatter.append("Failed to configure watching"); break;
    case FileSystemWatcherError::StopWatchingFailed: formatter.append("Failed to stop watching"); break;
    case FileSystemWatcherError::CloseFailed: formatter.append("Failed to close watcher"); break;
    case FileSystemWatcherError::MonitoringFailed: formatter.append("Filesystem monitoring stopped"); break;
    case FileSystemWatcherError::CoverageLimited: formatter.append("Filesystem monitoring coverage is limited"); break;
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
