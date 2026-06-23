// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../FileSystemWatcher/FileSystemWatcher.h"
#include "FileSystemWatcherThreading.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "../../Common/Deferred.h"

struct SC::FileSystemWatcher::FolderWatcherInternal
{
    uint8_t        changesBuffer[FolderWatcherSizes::MaxChangesBufferSize];
    FolderWatcher* parentEntry = nullptr; // We could in theory use SC_COMPILER_FIELD_OFFSET somehow to obtain it...
    HANDLE         fileHandle;
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

    Result init(FileSystemWatcher& parent, ThreadRunner& runner)
    {
        self            = &parent;
        threadingRunner = &runner.get();
        return Result(true);
    }

    Result init(FileSystemWatcher& parent, EventLoopRunner& runner)
    {
        self            = &parent;
        eventLoopRunner = &runner;
        eventLoopRunner->internalInit(parent, 0);
        return Result(true);
    }

    Result close()
    {
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
        return Result(true);
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

    Result submitRead(FolderWatcher& entry)
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
        SC_TRY_MSG(success == TRUE, "ReadDirectoryChangesW");
        clearPending.disarm();
        return Result(true);
    }

    Result stopWatching(FolderWatcher& folderWatcher)
    {
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
            closeFileHandle(folderWatcher);
            SC_TRY(eventLoopRunner->windowsWaitFolderExternalCompletionStopped(folderWatcher));
            return Result(true);
        }
        closeFileHandle(folderWatcher);
        return Result(true);
    }

    Result startWatching(FolderWatcher* entry)
    {
        if (threadingRunner)
        {
            threadingRunner->numEntries = 0;
        }
        // TODO: we should probably check if we are leaking on some partial failure code path...some RAII would help
        if (threadingRunner)
        {
            SC_TRY_MSG(threadingRunner->numEntries < ThreadRunnerDefinition::MaxWatchablePaths,
                       "startWatching exceeded MaxWatchablePaths");
        }
        HANDLE newHandle = ::CreateFileW(entry->path.view().getNullTerminatedNative(),                     //
                                         FILE_LIST_DIRECTORY,                                              //
                                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,           //
                                         nullptr,                                                          //
                                         OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, //
                                         nullptr);

        SC_TRY_MSG(newHandle != INVALID_HANDLE_VALUE, "CreateFileW failed");
        FolderWatcherInternal& opaque = entry->internal.get();
        opaque.fileHandle             = newHandle;

        opaque.parentEntry = entry;

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
        return Result(true);
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
                const DWORD            index  = result - WAIT_OBJECT_0;
                FolderWatcher&         entry  = *runner.entries[index];
                FolderWatcherInternal& opaque = entry.internal.get();

                DWORD transferredBytes = 0;
                SC_FILE_SYSTEM_WATCHER_ASSERT_DEBUG(opaque.fileHandle != INVALID_HANDLE_VALUE);
                OVERLAPPED* overlapped = getOverlapped(entry);
                const BOOL  overlappedResult =
                    ::GetOverlappedResult(opaque.fileHandle, overlapped, &transferredBytes, FALSE);
                notifyEntry(entry, overlappedResult ? static_cast<size_t>(transferredBytes) : 0);
            }
        }
        threadingRunner->shouldStop.exchange(false);
    }

    static void notifyEntry(FolderWatcher& entry, size_t transferredBytes)
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

        if (transferredBytes == 0)
        {
            // Windows reports a zero-byte completion when the change buffer overflowed.
            // The specific file names are lost, so report a conservative root-level change
            // instead of parsing stale buffer contents.
            notification.operation = Operation::AddRemoveRename;
            entry.notifyCallback(notification);
            if (entry.parent != nullptr)
            {
                SC_FILE_SYSTEM_WATCHER_TRUST_RESULT(entry.parent->internal.get().submitRead(entry));
            }
            return;
        }

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
            entry.notifyCallback(notification);
            if (not event->NextEntryOffset)
                break;

            *reinterpret_cast<uint8_t**>(&event) += event->NextEntryOffset;
        } while (true);

        if (entry.parent != nullptr)
        {
            SC_FILE_SYSTEM_WATCHER_TRUST_RESULT(entry.parent->internal.get().submitRead(entry));
        }
    }
};

SC::Result SC::FileSystemWatcher::Notification::getFullPath(StringPath& buffer) const
{
    SC_TRY_MSG(buffer.assign(basePath), "Buffer too small to hold full path");
    SC_TRY_MSG(buffer.append(L"\\"), "Buffer too small to hold full path");
    SC_TRY_MSG(buffer.append(relativePath), "Buffer too small to hold full path");
    return Result(true);
}

void SC::FileSystemWatcher::asyncNotify(FolderWatcher* watcher, size_t bytesTransferred)
{
    SC_FILE_SYSTEM_WATCHER_ASSERT_DEBUG(watcher != nullptr);
    SC_FILE_SYSTEM_WATCHER_ASSERT_DEBUG(watcher->internal.get().fileHandle != INVALID_HANDLE_VALUE);
    FileSystemWatcher::Internal::notifyEntry(*watcher->internal.get().parentEntry, bytesTransferred);
}
