// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../FileSystemWatcher/FileSystemWatcher.h"
#include "FileSystemWatcherThreading.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "../../Common/Deferred.h"

struct SC::FileSystemWatcher::FolderWatcherInternal
{
    uint8_t changesBuffer[FolderWatcherSizes::MaxChangesBufferSize];
    HANDLE  fileHandle;
    bool*   deliveryAlive = nullptr;
};

struct SC::FileSystemWatcher::ThreadRunnerInternal
{
    FSWThread thread;

    // Slot zero wakes the worker before the owner changes the wait set.
    HANDLE         hEvents[MAXIMUM_WAIT_OBJECTS]     = {};
    FolderWatcher* entries[MAXIMUM_WAIT_OBJECTS - 1] = {};
    HANDLE         paused                            = nullptr;
    HANDLE         resume                            = nullptr;
    bool           exiting                           = false;
    DWORD          count                             = 0;
};

struct SC::FileSystemWatcher::Internal
{
    FileSystemWatcher*    self            = nullptr;
    EventLoopRunner*      eventLoopRunner = nullptr;
    ThreadRunnerInternal* threadingRunner = nullptr;

    OVERLAPPED* getOverlapped(FolderWatcher& watcher)
    {
        if (eventLoopRunner)
        {
            return reinterpret_cast<OVERLAPPED*>(eventLoopRunner->windowsGetOverlapped(watcher));
        }
        else
        {
            return &watcher.asyncStorage.reinterpret_as<OVERLAPPED>();
        }
    }

    ResultFileSystemWatcher init(FileSystemWatcher& parent, ThreadRunner& runner)
    {
        auto&  state     = runner.get();
        HANDLE events[3] = {};
        for (HANDLE& event : events)
        {
            event = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (event == nullptr)
            {
                const auto error = static_cast<uint32_t>(::GetLastError());
                for (HANDLE created : events)
                    if (created != nullptr)
                        ::CloseHandle(created);
                return {FileSystemWatcherError::InitializationFailed,
                        FileSystemWatcherErrorDetail::WindowsCreateWatchEvent, error};
            }
        }
        state.hEvents[0] = events[0];
        state.paused     = events[1];
        state.resume     = events[2];
        state.exiting    = false;
        self             = &parent;
        threadingRunner  = &runner.get();
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher init(FileSystemWatcher& parent, EventLoopRunner& runner)
    {
        self            = &parent;
        eventLoopRunner = &runner;
        eventLoopRunner->internalInit(parent, 0);
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher close()
    {
        if (self == nullptr)
            return ResultFileSystemWatcher(FileSystemWatcherError::NotInitialized);
        if (threadingRunner)
        {
            SC_TRY(pauseWorker(FileSystemWatcherError::CloseFailed));
            auto& runner   = *threadingRunner;
            runner.exiting = true;
            if (runner.thread.wasStarted())
            {
                ::SetEvent(runner.resume);
                SC_TRY(runner.thread.join());
            }
            while (FolderWatcher* entry = self->watchers.front)
                SC_TRY(removeThreadWatch(*entry));
            ::CloseHandle(runner.paused);
            ::CloseHandle(runner.resume);
            ::CloseHandle(threadingRunner->hEvents[0]);
            threadingRunner->hEvents[0] = nullptr;
            threadingRunner             = nullptr;
        }
        else
        {
            while (FolderWatcher* entry = self->watchers.front)
                SC_TRY(stopWatching(*entry));
            eventLoopRunner->fileSystemWatcher = nullptr;
            eventLoopRunner                    = nullptr;
        }
        self = nullptr;
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher pauseWorker(FileSystemWatcherError error)
    {
        auto& runner = *threadingRunner;
        if (not runner.thread.wasStarted())
            return ResultFileSystemWatcher(true);
        if (::GetThreadId(runner.thread.thread) == ::GetCurrentThreadId())
            return {error, FileSystemWatcherErrorDetail::WindowsWaitForSingleObject, ERROR_POSSIBLE_DEADLOCK};
        if (not ::SetEvent(runner.hEvents[0]))
            return {error, FileSystemWatcherErrorDetail::WindowsSignalWatchEvent,
                    static_cast<uint32_t>(::GetLastError())};
        HANDLE      wait[] = {runner.paused, runner.thread.thread};
        const DWORD result = ::WaitForMultipleObjects(2, wait, FALSE, INFINITE);
        if (result == WAIT_OBJECT_0)
            return ResultFileSystemWatcher(true);
        if (result == WAIT_OBJECT_0 + 1)
        {
            SC_TRY(runner.thread.join());
            ::ResetEvent(runner.hEvents[0]);
            return ResultFileSystemWatcher(true);
        }
        return {error, FileSystemWatcherErrorDetail::WindowsWaitForWatchEvents,
                static_cast<uint32_t>(::GetLastError())};
    }

    ResultFileSystemWatcher resumeWorker()
    {
        auto& runner = *threadingRunner;
        if (runner.thread.wasStarted())
        {
            if (not ::SetEvent(runner.resume))
                return {FileSystemWatcherError::WatchSetupFailed, FileSystemWatcherErrorDetail::WindowsSignalWatchEvent,
                        static_cast<uint32_t>(::GetLastError())};
            return ResultFileSystemWatcher(true);
        }
        return runner.count == 0 ? ResultFileSystemWatcher(true) : runner.thread.start(&threadDispatch, this);
    }

    ResultFileSystemWatcher removeThreadWatch(FolderWatcher& entry)
    {
        auto& runner     = *threadingRunner;
        auto& folder     = entry.internal.get();
        auto* overlapped = getOverlapped(entry);
        if (not ::CancelIoEx(folder.fileHandle, overlapped))
        {
            const auto error = static_cast<uint32_t>(::GetLastError());
            if (error != ERROR_NOT_FOUND)
                return {FileSystemWatcherError::StopWatchingFailed,
                        FileSystemWatcherErrorDetail::WindowsCancelDirectoryChanges, error};
        }
        // A manual-reset event lets us wait even when the worker observed the completion.
        DWORD bytes = 0;
        (void)::GetOverlappedResult(folder.fileHandle, overlapped, &bytes, TRUE);
        for (DWORD index = 0; index < runner.count; ++index)
        {
            if (runner.entries[index] != &entry)
                continue;
            --runner.count;
            runner.entries[index]     = runner.entries[runner.count];
            runner.hEvents[index + 1] = runner.hEvents[runner.count + 1];
            break;
        }
        ::CloseHandle(overlapped->hEvent);
        closeFileHandle(entry);
        detachUnstartedWatcher(entry);
        return ResultFileSystemWatcher(true);
    }

    void closeFileHandle(FolderWatcher& watcher)
    {
        auto& opaque = watcher.internal.get();
        ::CloseHandle(opaque.fileHandle);
        opaque.fileHandle = INVALID_HANDLE_VALUE;
    }

    ResultFileSystemWatcher submitRead(FolderWatcher& entry)
    {
        FolderWatcherInternal& opaque     = entry.internal.get();
        OVERLAPPED*            overlapped = getOverlapped(entry);
        SC_FILE_SYSTEM_WATCHER_ASSERT_DEBUG(opaque.fileHandle != INVALID_HANDLE_VALUE);

        if (eventLoopRunner)
        {
            SC_TRY(eventLoopRunner->windowsMarkFolderExternalCompletionPending(entry));
        }
        auto clearPending = MakeDeferred(
            [&]()
            {
                if (eventLoopRunner)
                {
                    SC_FILE_SYSTEM_WATCHER_TRUST_RESULT(
                        eventLoopRunner->windowsClearFolderExternalCompletionPending(entry));
                }
            });

        const BOOL success = ::ReadDirectoryChangesW(opaque.fileHandle,                 //
                                                     opaque.changesBuffer,              //
                                                     sizeof(opaque.changesBuffer),      //
                                                     TRUE,                              // watchSubtree
                                                     FILE_NOTIFY_CHANGE_FILE_NAME |     //
                                                         FILE_NOTIFY_CHANGE_DIR_NAME |  //
                                                         FILE_NOTIFY_CHANGE_LAST_WRITE, //
                                                     nullptr,                           // lpBytesReturned
                                                     overlapped,                        // lpOverlapped
                                                     nullptr);                          // lpCompletionRoutine
        if (success != TRUE)
        {
            const uint32_t nativeError = static_cast<uint32_t>(::GetLastError());
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                           FileSystemWatcherErrorDetail::WindowsSubmitDirectoryChanges, nativeError);
        }
        clearPending.disarm();
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher stopWatching(FolderWatcher& folderWatcher)
    {
        if (threadingRunner)
        {
            SC_TRY(pauseWorker(FileSystemWatcherError::StopWatchingFailed));
            const auto removed = removeThreadWatch(folderWatcher);
            const auto resumed = resumeWorker();
            return removed ? resumed : removed;
        }
        auto& folder = folderWatcher.internal.get();
        if (folder.deliveryAlive != nullptr)
        {
            *folder.deliveryAlive = false;
            folder.deliveryAlive  = nullptr;
        }
        folderWatcher.parent->watchers.remove(folderWatcher);
        folderWatcher.parent = nullptr;
        SC_TRY(eventLoopRunner->windowsRequestStopFolderExternalCompletion(folderWatcher));
        SC_TRY(eventLoopRunner->windowsWaitFolderExternalCompletionStopped(folderWatcher));
        closeFileHandle(folderWatcher);
        return ResultFileSystemWatcher(true);
    }

    void detachUnstartedWatcher(FolderWatcher& entry)
    {
        self->watchers.remove(entry);
        entry.parent = nullptr;
    }

    ResultFileSystemWatcher startWatching(FolderWatcher* entry)
    {
        HANDLE newHandle = ::CreateFileW(entry->path.view().getNullTerminatedNative(),                     //
                                         FILE_LIST_DIRECTORY,                                              //
                                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,           //
                                         nullptr,                                                          //
                                         OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, //
                                         nullptr);

        if (newHandle == INVALID_HANDLE_VALUE)
        {
            const uint32_t nativeError = static_cast<uint32_t>(::GetLastError());
            detachUnstartedWatcher(*entry);
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                           FileSystemWatcherErrorDetail::WindowsOpenDirectory, nativeError);
        }
        BY_HANDLE_FILE_INFORMATION fileInfo = {};
        if (::GetFileInformationByHandle(newHandle, &fileInfo) == FALSE)
        {
            const uint32_t nativeError = static_cast<uint32_t>(::GetLastError());
            ::CloseHandle(newHandle);
            detachUnstartedWatcher(*entry);
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                           FileSystemWatcherErrorDetail::WindowsQueryRootDirectory, nativeError);
        }
        if ((fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            ::CloseHandle(newHandle);
            detachUnstartedWatcher(*entry);
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                           FileSystemWatcherErrorDetail::WindowsQueryRootDirectory, ERROR_DIRECTORY);
        }
        auto& folder      = entry->internal.get();
        folder.fileHandle = newHandle;
        if (threadingRunner)
            return startThreadWatch(*entry);

        const auto started = eventLoopRunner->windowsStartFolderExternalCompletion(*entry, newHandle);
        if (not started)
        {
            closeFileHandle(*entry);
            detachUnstartedWatcher(*entry);
            return started;
        }
        const auto submitted = submitRead(*entry);
        if (submitted)
            return submitted;
        const auto stopped = stopWatching(*entry);
        return stopped ? submitted : stopped;
    }

    ResultFileSystemWatcher startThreadWatch(FolderWatcher& entry)
    {
        auto rollback = MakeDeferred(
            [&]
            {
                closeFileHandle(entry);
                detachUnstartedWatcher(entry);
            });
        auto& runner = *threadingRunner;
        if (runner.count == MAXIMUM_WAIT_OBJECTS - 1)
            return {FileSystemWatcherError::WatchLimitExceeded, FileSystemWatcherErrorDetail::WatchPathCapacity};
        auto* overlapped   = getOverlapped(entry);
        *overlapped        = {};
        overlapped->hEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (overlapped->hEvent == nullptr)
            return {FileSystemWatcherError::WatchSetupFailed, FileSystemWatcherErrorDetail::WindowsCreateWatchEvent,
                    static_cast<uint32_t>(::GetLastError())};
        auto closeEvent = MakeDeferred([&] { ::CloseHandle(overlapped->hEvent); });
        SC_TRY(pauseWorker(FileSystemWatcherError::WatchSetupFailed));
        const auto submitted = submitRead(entry);
        if (not submitted)
        {
            const auto resumed = resumeWorker();
            return resumed ? submitted : resumed;
        }
        runner.entries[runner.count] = &entry;
        ++runner.count;
        runner.hEvents[runner.count] = overlapped->hEvent;
        closeEvent.disarm();
        rollback.disarm();
        const auto resumed = resumeWorker();
        if (resumed)
            return resumed;
        const auto removed = removeThreadWatch(entry);
        return removed ? resumed : removed;
    }

    static DWORD WINAPI threadDispatch(LPVOID arg)
    {
        Internal& self = *static_cast<Internal*>(arg);
        self.threadRun(self.threadingRunner->thread);
        return 0;
    }

    void threadRun(FSWThread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("FileSystemWatcher::init"));
        ThreadRunnerInternal& runner = *threadingRunner;
        for (;;)
        {
            const DWORD result = ::WaitForMultipleObjects(runner.count + 1, runner.hEvents, FALSE, INFINITE);
            if (result == WAIT_OBJECT_0)
            {
                ::SetEvent(runner.paused);
                if (::WaitForSingleObject(runner.resume, INFINITE) != WAIT_OBJECT_0 or runner.exiting)
                    return;
                continue;
            }
            if (result == WAIT_FAILED)
            {
                const auto error = static_cast<uint32_t>(::GetLastError());
                for (DWORD index = 0; index < runner.count; ++index)
                    notifyStatus(*runner.entries[index], NotificationKind::WatchStopped,
                                 {FileSystemWatcherError::MonitoringFailed,
                                  FileSystemWatcherErrorDetail::WindowsWaitForWatchEvents, error});
                return;
            }
            const DWORD index = result - WAIT_OBJECT_0;
            ::ResetEvent(runner.hEvents[index]);
            (void)completeRead(*runner.entries[index - 1]);
        }
    }

    static void notifyStatus(const FolderWatcher& entry, NotificationKind kind, ResultFileSystemWatcher failure)
    {
        Notification notification;
        notification.basePath  = entry.path.view();
        notification.operation = Operation::AddRemoveRename;
        notification.kind      = kind;
        notification.failure   = failure;
        auto callback          = entry.notifyCallback;
        callback(notification);
    }

    bool completeRead(FolderWatcher& entry)
    {
        auto& folder           = entry.internal.get();
        DWORD transferredBytes = 0;
        if (not ::GetOverlappedResult(folder.fileHandle, getOverlapped(entry), &transferredBytes, FALSE))
        {
            const DWORD error = ::GetLastError();
            if (error != ERROR_NOTIFY_ENUM_DIR)
            {
                notifyStatus(entry, NotificationKind::WatchStopped,
                             {FileSystemWatcherError::MonitoringFailed,
                              FileSystemWatcherErrorDetail::WindowsCompleteDirectoryChanges, error});
                return false;
            }
            transferredBytes = 0;
        }

        bool alive = true;
        if (eventLoopRunner)
            folder.deliveryAlive = &alive;
        if (transferredBytes == 0)
            notifyStatus(entry, NotificationKind::RescanRequired, ResultFileSystemWatcher(true));
        else
            notifyEntry(entry, transferredBytes, alive);
        if (not alive or entry.parent == nullptr)
            return false;

        folder.deliveryAlive = nullptr;
        const auto submitted = submitRead(entry);
        if (submitted)
            return true;
        if (submitted.isError(FileSystemWatcherError::WatchSetupFailed))
            notifyStatus(entry, NotificationKind::WatchStopped,
                         {FileSystemWatcherError::MonitoringFailed, submitted.detail, submitted.nativeError});
        else
            notifyStatus(entry, NotificationKind::WatchStopped, submitted);
        return false;
    }

    static void notifyEntry(FolderWatcher& entry, size_t transferredBytes, const bool& alive)
    {
        FolderWatcherInternal& opaque = entry.internal.get();
        uint8_t                changesBuffer[FolderWatcherSizes::MaxChangesBufferSize];

        if (transferredBytes > sizeof(changesBuffer))
        {
            transferredBytes = sizeof(changesBuffer);
        }
        for (size_t idx = 0; idx < transferredBytes; ++idx)
        {
            changesBuffer[idx] = opaque.changesBuffer[idx];
        }

        Notification notification;
        notification.basePath = entry.path.view();

        FILE_NOTIFY_INFORMATION* event = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(changesBuffer);
        do
        {
            notification.relativePath = {
                {reinterpret_cast<const char*>(event->FileName), static_cast<size_t>(event->FileNameLength)},
                true,
                StringEncoding::Utf16};

            switch (event->Action)
            {
            case FILE_ACTION_MODIFIED: notification.operation = Operation::Modified; break;
            default: notification.operation = Operation::AddRemoveRename; break;
            }
            auto callback = entry.notifyCallback;
            callback(notification);
            if (not alive or not event->NextEntryOffset)
                break;

            *reinterpret_cast<uint8_t**>(&event) += event->NextEntryOffset;
        } while (true);
    }
};

SC::ResultFileSystemWatcher SC::FileSystemWatcher::Notification::getFullPath(StringPath& buffer) const
{
    if (not buffer.assign(basePath) or not buffer.append(L"\\") or not buffer.append(relativePath))
        return ResultFileSystemWatcher(FileSystemWatcherError::PathPreparationFailed,
                                       FileSystemWatcherErrorDetail::BuildFullPath);
    return ResultFileSystemWatcher(true);
}

bool SC::FileSystemWatcher::asyncNotify(FolderWatcher* watcher, size_t bytesTransferred)
{
    (void)bytesTransferred;
    SC_FILE_SYSTEM_WATCHER_ASSERT_DEBUG(watcher != nullptr);
    SC_FILE_SYSTEM_WATCHER_ASSERT_DEBUG(watcher->internal.get().fileHandle != INVALID_HANDLE_VALUE);
    return internal.get().completeRead(*watcher);
}

void SC::FileSystemWatcher::asyncNotifyFailure(FolderWatcher& watcher, Result failure)
{
    Internal::notifyStatus(watcher, NotificationKind::WatchStopped, ResultFileSystemWatcher(failure));
}
