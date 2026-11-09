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

    static constexpr int N = ThreadRunnerDefinition::MaxWatchablePaths;

    HANDLE         hEvents[N] = {0};
    FolderWatcher* entries[N] = {nullptr};
    DWORD          numEntries = 0;
    FSWAtomicBool  shouldStop;
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
        self            = &parent;
        threadingRunner = &runner.get();
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
            if (threadingRunner->thread.wasStarted())
            {
                threadingRunner->shouldStop.exchange(true);
                do
                {
                    for (DWORD idx = 0; idx < threadingRunner->numEntries; ++idx)
                    {
                        ::SetEvent(threadingRunner->hEvents[idx]);
                    }
                } while (threadingRunner->shouldStop.load());
                SC_TRY(threadingRunner->thread.join());
            }
        }
        while (FolderWatcher* entry = self->watchers.front)
        {
            SC_TRY(stopWatching(*entry));
        }
        return ResultFileSystemWatcher(true);
    }

    void signalWatcherEvent(FolderWatcher& watcher)
    {
        OVERLAPPED* overlapped = getOverlapped(watcher);
        ::SetEvent(overlapped->hEvent);
    }

    void closeWatcherEvent(FolderWatcher& watcher)
    {
        OVERLAPPED* overlapped = getOverlapped(watcher);
        ::CloseHandle(overlapped->hEvent);
        overlapped->hEvent = INVALID_HANDLE_VALUE;
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
        auto& folder = folderWatcher.internal.get();
        if (folder.deliveryAlive != nullptr)
        {
            *folder.deliveryAlive = false;
            folder.deliveryAlive  = nullptr;
        }
        folderWatcher.parent->watchers.remove(folderWatcher);
        folderWatcher.parent = nullptr;
        if (threadingRunner)
        {
            signalWatcherEvent(folderWatcher);
            closeWatcherEvent(folderWatcher);
        }
        else
        {
            SC_TRY(eventLoopRunner->windowsRequestStopFolderExternalCompletion(folderWatcher));
            SC_TRY(eventLoopRunner->windowsWaitFolderExternalCompletionStopped(folderWatcher));
            closeFileHandle(folderWatcher);
            return ResultFileSystemWatcher(true);
        }
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
        // TODO: we should probably check if we are leaking on some partial failure code path...some RAII would help
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
        if (threadingRunner)
        {
            threadingRunner->numEntries = 0;
            if (threadingRunner->numEntries >= ThreadRunnerDefinition::MaxWatchablePaths)
            {
                ::CloseHandle(newHandle);
                detachUnstartedWatcher(*entry);
                return ResultFileSystemWatcher(FileSystemWatcherError::WatchLimitExceeded,
                                               FileSystemWatcherErrorDetail::WatchPathCapacity);
            }
        }
        FolderWatcherInternal& opaque = entry->internal.get();
        opaque.fileHandle             = newHandle;

        OVERLAPPED* overlapped = getOverlapped(*entry);
        if (threadingRunner)
        {
            overlapped->hEvent = ::CreateEventW(nullptr, FALSE, 0, nullptr);

            threadingRunner->hEvents[threadingRunner->numEntries] = overlapped->hEvent;
            threadingRunner->entries[threadingRunner->numEntries] = entry;
            threadingRunner->numEntries++;
        }
        else
        {
            SC_TRY(eventLoopRunner->windowsStartFolderExternalCompletion(*entry, newHandle));
        }

        SC_TRY(submitRead(*entry));

        if (threadingRunner and not threadingRunner->thread.wasStarted())
        {
            threadingRunner->shouldStop.exchange(false);
            SC_TRY(threadingRunner->thread.start(&threadDispatch, this));
        }
        return ResultFileSystemWatcher(true);
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
        while (not runner.shouldStop.load())
        {
            const DWORD result = ::WaitForMultipleObjects(runner.numEntries, runner.hEvents, TRUE, INFINITE);
            if (result != WAIT_FAILED and not runner.shouldStop.load())
            {
                const DWORD    index = result - WAIT_OBJECT_0;
                FolderWatcher& entry = *runner.entries[index];
                (void)completeRead(entry);
            }
        }
        threadingRunner->shouldStop.exchange(false);
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
