// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../FileSystemWatcher/FileSystemWatcher.h"
#include "../../Threading/Threading.h"

#include "../../Async/Internal/AsyncWindows.h" // AsyncWinOverlapped

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

struct SC::FileSystemWatcher::FolderWatcherInternal
{
    AsyncFilePoll  asyncPoll;
    uint8_t        changesBuffer[FolderWatcherSizes::MaxChangesBufferSize];
    FolderWatcher* parentEntry = nullptr; // We could in theory use SC_COMPILER_FIELD_OFFSET somehow to obtain it...
    FileDescriptor fileHandle;

    OVERLAPPED& getOverlapped() { return asyncPoll.getOverlappedOpaque().get().overlapped; }
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
        auto& opaque = watcher.internal.get();
        ::SetEvent(opaque.getOverlapped().hEvent);
    }

    void closeWatcherEvent(FolderWatcher& watcher)
    {
        auto& opaque = watcher.internal.get();
        ::CloseHandle(opaque.getOverlapped().hEvent);
        opaque.getOverlapped().hEvent = INVALID_HANDLE_VALUE;
    }

    void closeFileHandle(FolderWatcher& watcher)
    {
        auto& opaque = watcher.internal.get();
        SC_TRUST_RESULT(opaque.fileHandle.close());
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
            // This is not strictly needed as file handle is being closed soon after anyway
            // FolderWatcherInternal& opaque = folderWatcher.internal.get();
            // SC_TRUST_RESULT(eventLoopRunner->eventLoop->removeAllAssociationsFor(opaque.fileHandle));
            SC_TRUST_RESULT(folderWatcher.internal.get().asyncPoll.stop(*eventLoopRunner->eventLoop));
        }
        closeFileHandle(folderWatcher);
        return Result(true);
    }

    static void setDebugName(FolderWatcher& folderWatcher, const char* debugName)
    {
        FolderWatcherInternal& opaque = folderWatcher.internal.get();
        opaque.asyncPoll.setDebugName(debugName);
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
        HANDLE newHandle = ::CreateFileW(entry->path.getNullTerminatedNative(),                            //
                                         FILE_LIST_DIRECTORY,                                              //
                                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,           //
                                         nullptr,                                                          //
                                         OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, //
                                         nullptr);

        SC_TRY(newHandle != INVALID_HANDLE_VALUE);
        FolderWatcherInternal& opaque = entry->internal.get();
        SC_TRY(opaque.fileHandle.assign(newHandle));

        opaque.parentEntry = entry;

        if (threadingRunner)
        {
            opaque.getOverlapped().hEvent = ::CreateEventW(nullptr, FALSE, 0, nullptr);

            threadingRunner->hEvents[threadingRunner->numEntries] = opaque.getOverlapped().hEvent;
            threadingRunner->entries[threadingRunner->numEntries] = entry;
            threadingRunner->numEntries++;
        }
        else
        {
            // TODO: Consider associating / removing file handle with IOCP directly inside AsyncFilePoll
            SC_TRY(eventLoopRunner->eventLoop->associateExternallyCreatedFileDescriptor(opaque.fileHandle));
            opaque.asyncPoll.callback.bind<Internal, &Internal::onEventLoopNotification>(*this);
            auto res = opaque.asyncPoll.start(*eventLoopRunner->eventLoop, newHandle);
            SC_TRY(res);
        }

        BOOL success = ::ReadDirectoryChangesW(newHandle,                         //
                                               opaque.changesBuffer,              //
                                               sizeof(opaque.changesBuffer),      //
                                               TRUE,                              // watchSubtree
                                               FILE_NOTIFY_CHANGE_FILE_NAME |     //
                                                   FILE_NOTIFY_CHANGE_DIR_NAME |  //
                                                   FILE_NOTIFY_CHANGE_LAST_WRITE, //
                                               nullptr,                           // lpBytesReturned
                                               &opaque.getOverlapped(),           // lpOverlapped
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
                DWORD                  transferredBytes;
                HANDLE                 handle;
                if (opaque.fileHandle.get(handle, Result::Error("Invalid fs handle")))
                {
                    ::GetOverlappedResult(handle, &opaque.getOverlapped(), &transferredBytes, FALSE);
                    notifyEntry(entry);
                }
            }
        }
        threadingRunner->shouldStop.exchange(false);
    }

    void onEventLoopNotification(AsyncFilePoll::Result& result)
    {
        SC_COMPILER_WARNING_PUSH_OFFSETOF;
        FolderWatcherInternal& fwi = SC_COMPILER_FIELD_OFFSET(FolderWatcherInternal, asyncPoll, result.getAsync());
        SC_ASSERT_DEBUG(fwi.fileHandle.isValid());
        notifyEntry(*fwi.parentEntry);
        result.reactivateRequest(true);
        SC_COMPILER_WARNING_POP;
    }

    static void notifyEntry(FolderWatcher& entry)
    {
        FolderWatcherInternal&   opaque = entry.internal.get();
        FILE_NOTIFY_INFORMATION* event  = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(opaque.changesBuffer);

        Notification notification;
        notification.basePath = entry.path;

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

        memset(&opaque.getOverlapped(), 0, sizeof(opaque.getOverlapped()));
        HANDLE handle;
        if (opaque.fileHandle.get(handle, Result::Error("Invalid fs handle")))
        {
            BOOL success = ::ReadDirectoryChangesW(handle,                            //
                                                   opaque.changesBuffer,              //
                                                   sizeof(opaque.changesBuffer),      //
                                                   TRUE,                              // watchSubtree
                                                   FILE_NOTIFY_CHANGE_FILE_NAME |     //
                                                       FILE_NOTIFY_CHANGE_DIR_NAME |  //
                                                       FILE_NOTIFY_CHANGE_LAST_WRITE, //
                                                   nullptr,                           // lpBytesReturned
                                                   &opaque.getOverlapped(),           // lpOverlapped
                                                   nullptr);                          // lpCompletionRoutine
            // TODO: Handle ReadDirectoryChangesW error
            (void)success;
        }
    }
};

SC::Result SC::FileSystemWatcher::Notification::getFullPath(Span<wchar_t> buffer, StringViewData& outStringView) const
{
    // Calculate sizes in terms of wchar_t count rather than bytes
    const size_t basePathChars     = basePath.sizeInBytes() / sizeof(wchar_t);
    const size_t relativePathChars = relativePath.sizeInBytes() / sizeof(wchar_t);

    if (buffer.sizeInBytes() / sizeof(wchar_t) < basePathChars + relativePathChars + 2)
    {
        return Result::Error("Buffer too small to hold full path");
    }

    const wchar_t* basePathStr     = basePath.getNullTerminatedNative();
    const wchar_t* relativePathStr = relativePath.getNullTerminatedNative();

    ::memcpy(buffer.data(), basePathStr, basePathChars * sizeof(wchar_t));
    buffer.data()[basePathChars] = L'\\'; // Add the separator
    ::memcpy(buffer.data() + basePathChars + 1, relativePathStr, relativePathChars * sizeof(wchar_t));
    buffer.data()[basePathChars + 1 + relativePathChars] = L'\0'; // Null terminate the string

    outStringView = {{buffer.data(), (basePathChars + 1 + relativePathChars)}, true};
    return Result(true);
}
