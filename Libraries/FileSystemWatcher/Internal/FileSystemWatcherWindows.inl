// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "../../Strings/SmallString.h"
#include "../../Strings/StringBuilder.h"
#include "../../Strings/StringConverter.h"
#include "../../Threading/Threading.h"

#include "../../Async/Internal/EventLoopWindows.h" // EventLoopWinOverlapped

struct SC::FileSystemWatcher::FolderWatcherInternal
{
    AsyncWindowsPoll asyncPoll;
    uint8_t          changesBuffer[FolderWatcherSizes::MaxChangesBufferSize];
    FolderWatcher*   parentEntry = nullptr; // We could in theory use SC_FIELD_OFFSET somehow to obtain it...
    FileDescriptor   fileHandle;

    OVERLAPPED& getOverlapped() { return asyncPoll.getOverlappedOpaque().get().overlapped; }
};

struct SC::FileSystemWatcher::ThreadRunnerInternal
{
    Thread thread;
    Action threadFunction;

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

    [[nodiscard]] Result init(FileSystemWatcher& parent, ThreadRunner& runner)
    {
        self            = &parent;
        threadingRunner = &runner.get();
        threadingRunner->threadFunction.bind<Internal, &Internal::threadRun>(*this);
        return Result(true);
    }

    [[nodiscard]] Result init(FileSystemWatcher& parent, EventLoopRunner& runner)
    {
        self            = &parent;
        eventLoopRunner = &runner;
        return Result(true);
    }

    [[nodiscard]] Result close()
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

    [[nodiscard]] Result stopWatching(FolderWatcher& folderWatcher)
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
            SC_TRUST_RESULT(folderWatcher.internal.get().asyncPoll.stop());
        }
        closeFileHandle(folderWatcher);
        return Result(true);
    }

    [[nodiscard]] Result startWatching(FolderWatcher* entry)
    {
        StringNative<1024> buffer = StringEncoding::Native; // TODO: this needs to go into caller context
        StringConverter    converter(buffer);
        if (threadingRunner)
        {
            threadingRunner->numEntries = 0;
        }
        FileDescriptor::Handle loopFDS = FileDescriptor::Invalid;
        if (eventLoopRunner)
        {
            SC_TRY(eventLoopRunner->eventLoop.getLoopFileDescriptor(loopFDS));
        }
        // TODO: we should probably check if we are leaking on some partial failure codepath...some RAII would help
        if (threadingRunner)
        {
            SC_TRY_MSG(threadingRunner->numEntries < ThreadRunnerDefinition::MaxWatchablePaths,
                       "startWatching exceeded MaxWatchablePaths");
        }
        StringView encodedPath;
        SC_TRY(converter.convertNullTerminateFastPath(entry->path->view(), encodedPath));
        HANDLE newHandle = ::CreateFileW(encodedPath.getNullTerminatedNative(),                            //
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
            opaque.getOverlapped().hEvent                         = ::CreateEventW(nullptr, FALSE, 0, nullptr);
            threadingRunner->hEvents[threadingRunner->numEntries] = opaque.getOverlapped().hEvent;
            threadingRunner->entries[threadingRunner->numEntries] = entry;
            threadingRunner->numEntries++;
        }
        else
        {
            SC_TRY(eventLoopRunner->eventLoop.associateExternallyCreatedFileDescriptor(opaque.fileHandle));
            opaque.asyncPoll.callback.bind<Internal, &Internal::onEventLoopNotification>(*this);
            auto res = opaque.asyncPoll.start(eventLoopRunner->eventLoop, newHandle);
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
            SC_TRY(threadingRunner->thread.start("FileSystemWatcher::init", &threadingRunner->threadFunction))
        }
        return Result(true);
    }

    void threadRun()
    {
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

    void onEventLoopNotification(AsyncWindowsPoll::Result& result)
    {
        FolderWatcherInternal& fwi = SC_FIELD_OFFSET(FolderWatcherInternal, asyncPoll, result.async);
        SC_ASSERT_DEBUG(fwi.fileHandle.isValid());
        notifyEntry(*fwi.parentEntry);
        result.reactivateRequest(true);
    }

    static void notifyEntry(FolderWatcher& entry)
    {
        FolderWatcherInternal&   opaque = entry.internal.get();
        FILE_NOTIFY_INFORMATION* event  = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(opaque.changesBuffer);

        Notification notification;
        notification.basePath = entry.path->view();
        while (notification.basePath.sizeInBytes() > 1 and notification.basePath.endsWithChar('\\'))
        {
            notification.basePath =
                notification.basePath.sliceStartEndBytes(0, notification.basePath.sizeInBytes() - 1);
        }

        do
        {
            const Span<const wchar_t> span(event->FileName, event->FileNameLength / sizeof(wchar_t));
            const StringView          path(span, false);

            notification.relativePath = path;
            switch (event->Action)
            {
            case FILE_ACTION_MODIFIED: notification.operation = Operation::Modified; break;
            default: notification.operation = Operation::AddRemoveRename; break;
            }
            entry.notifyCallback(notification);

            *reinterpret_cast<uint8_t**>(&event) += event->NextEntryOffset;
        } while (event->NextEntryOffset);

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

SC::Result SC::FileSystemWatcher::Notification::getFullPath(String& buffer, StringView& outStringView) const
{
    StringBuilder builder(buffer);
    SC_TRY(builder.append(basePath));
    SC_TRY(builder.append("\\"));
    SC_TRY(builder.append(relativePath));
    outStringView = buffer.view();
    return Result(true);
}
