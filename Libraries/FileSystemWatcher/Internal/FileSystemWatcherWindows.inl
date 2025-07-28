// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../FileSystemWatcher/FileSystemWatcher.h"
#include "../../Threading/Atomic.h"
#include "../../Threading/Threading.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

struct SC::FileSystemWatcher::FolderWatcherInternal
{
    uint8_t        changesBuffer[FolderWatcherSizes::MaxChangesBufferSize];
    FolderWatcher* parentEntry = nullptr; // We could in theory use SC_COMPILER_FIELD_OFFSET somehow to obtain it...
    HANDLE         fileHandle;
};

struct SC::FileSystemWatcher::ThreadRunnerInternal
{
    Thread thread;

    static constexpr int N = ThreadRunnerDefinition::MaxWatchablePaths;

    HANDLE         hEvents[N] = {0};
    FolderWatcher* entries[N] = {nullptr};
    DWORD          numEntries = 0;
    Atomic<bool>   shouldStop = false;
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
        for (FolderWatcher* entry = self->watchers.front; entry != nullptr; entry = entry->next)
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
            SC_TRUST_RESULT(eventLoopRunner->windowsStopFolderFilePoll(folderWatcher));
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
        HANDLE newHandle = ::CreateFileW(entry->path.path.buffer,                                          //
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
            SC_TRY(eventLoopRunner->windowsStartFolderFilePoll(*entry, newHandle));
        }

        BOOL success = ::ReadDirectoryChangesW(newHandle,                         //
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

        if (threadingRunner and not threadingRunner->thread.wasStarted())
        {
            threadingRunner->shouldStop.exchange(false);
            Function<void(Thread&)> threadFunction;
            threadFunction.bind<Internal, &Internal::threadRun>(*this);
            SC_TRY(threadingRunner->thread.start(move(threadFunction)))
        }
        return Result(true);
    }

    void threadRun(Thread& thread)
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

                DWORD transferredBytes;
                SC_ASSERT_DEBUG(opaque.fileHandle != INVALID_HANDLE_VALUE);
                OVERLAPPED* overlapped = getOverlapped(entry);
                ::GetOverlappedResult(opaque.fileHandle, overlapped, &transferredBytes, FALSE);
                notifyEntry(entry);
            }
        }
        threadingRunner->shouldStop.exchange(false);
    }

    static void notifyEntry(FolderWatcher& entry)
    {
        FolderWatcherInternal&   opaque = entry.internal.get();
        FILE_NOTIFY_INFORMATION* event  = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(opaque.changesBuffer);

        Notification notification;
        notification.basePath = entry.path.path;

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

        OVERLAPPED* overlapped = entry.parent->internal.get().getOverlapped(entry);
        ::memset(overlapped, 0, sizeof(OVERLAPPED));
        SC_ASSERT_DEBUG(opaque.fileHandle != INVALID_HANDLE_VALUE);
        BOOL success = ::ReadDirectoryChangesW(opaque.fileHandle,                 //
                                               opaque.changesBuffer,              //
                                               sizeof(opaque.changesBuffer),      //
                                               TRUE,                              // watchSubtree
                                               FILE_NOTIFY_CHANGE_FILE_NAME |     //
                                                   FILE_NOTIFY_CHANGE_DIR_NAME |  //
                                                   FILE_NOTIFY_CHANGE_LAST_WRITE, //
                                               nullptr,                           // lpBytesReturned
                                               overlapped,                        // lpOverlapped
                                               nullptr);                          // lpCompletionRoutine
        // TODO: Handle ReadDirectoryChangesW error
        (void)success;
    }
};

SC::Result SC::FileSystemWatcher::Notification::getFullPath(StringPath& buffer) const
{
    SC_TRY_MSG(buffer.path.assign(basePath), "Buffer too small to hold full path");
    SC_TRY_MSG(buffer.path.append(L"\\"), "Buffer too small to hold full path");
    SC_TRY_MSG(buffer.path.append(relativePath), "Buffer too small to hold full path");
    return Result(true);
}

void SC::FileSystemWatcher::asyncNotify(FolderWatcher* watcher)
{
    SC_ASSERT_DEBUG(watcher != nullptr);
    SC_ASSERT_DEBUG(watcher->internal.get().fileHandle != INVALID_HANDLE_VALUE);
    FileSystemWatcher::Internal::notifyEntry(*watcher->internal.get().parentEntry);
}