// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "../Foundation/StringBuilder.h"
#include "../Foundation/StringConverter.h"
#include "../Threading/Threading.h"

#include "../Async/EventLoopWindows.h" // EventLoopWinOverlapped

struct SC::FileSystemWatcher::FolderWatcherInternal
{
    EventLoopWinOverlapped overlapped;

    HANDLE  fileHandle = INVALID_HANDLE_VALUE;
    uint8_t changesBuffer[FolderWatcherSizes::MaxChangesBufferSize];
};

struct SC::FileSystemWatcher::ThreadRunnerInternal
{
    Thread thread;
    Action threadFunction;

    static constexpr int N = ThreadRunnerSizes::MaxWatchablePaths;

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

    [[nodiscard]] ReturnCode init(FileSystemWatcher& parent, ThreadRunner& runner)
    {
        self            = &parent;
        threadingRunner = &runner.get();
        threadingRunner->threadFunction.bind<Internal, &Internal::threadRun>(this);
        return true;
    }

    [[nodiscard]] ReturnCode init(FileSystemWatcher& parent, EventLoopRunner& runner)
    {
        self            = &parent;
        eventLoopRunner = &runner;
        eventLoopRunner->eventLoopAsync.callback.bind<Internal, &Internal::onEventLoopNotification>(this);
        return true;
    }

    [[nodiscard]] ReturnCode close()
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
                SC_TRY_IF(threadingRunner->thread.join());
            }
        }
        for (FolderWatcher* entry = self->watchers.front; entry != nullptr; entry = entry->next)
        {
            SC_TRY_IF(stopWatching(*entry));
        }
        return true;
    }

    void signalWatcherEvent(FolderWatcher& watcher)
    {
        auto& opaque = watcher.internal.get();
        ::SetEvent(opaque.overlapped.overlapped.hEvent);
    }

    void closeWatcherEvent(FolderWatcher& watcher)
    {
        auto& opaque = watcher.internal.get();
        ::CloseHandle(opaque.overlapped.overlapped.hEvent);
        opaque.overlapped.overlapped.hEvent = INVALID_HANDLE_VALUE;
    }

    void closeFileHandle(FolderWatcher& watcher)
    {
        auto& opaque = watcher.internal.get();
        if (opaque.fileHandle != INVALID_HANDLE_VALUE)
        {
            ::CancelIo(opaque.fileHandle);
            ::CloseHandle(opaque.fileHandle);
            opaque.fileHandle = INVALID_HANDLE_VALUE;
        }
    }

    [[nodiscard]] ReturnCode stopWatching(FolderWatcher& folderWatcher)
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
            eventLoopRunner->eventLoop.decreaseActiveCount();
        }
        closeFileHandle(folderWatcher);
        return true;
    }

    [[nodiscard]] ReturnCode startWatching(FolderWatcher* entry)
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
            SC_TRY_IF(eventLoopRunner->eventLoop.getLoopFileDescriptor(loopFDS));
        }
        // TODO: we should probably check if we are leaking on some partial failure codepath...some RAII would help
        if (threadingRunner)
        {
            SC_TRY_MSG(threadingRunner->numEntries < ThreadRunnerSizes::MaxWatchablePaths,
                       "startWatching exceeded MaxWatchablePaths"_a8);
        }
        StringView encodedPath;
        SC_TRY_IF(converter.convertNullTerminateFastPath(entry->path->view(), encodedPath));
        FolderWatcherInternal& opaque = entry->internal.get();
        opaque.fileHandle             = CreateFile(encodedPath.getNullTerminatedNative(),                            //
                                                   FILE_LIST_DIRECTORY,                                              //
                                                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,           //
                                                   nullptr,                                                          //
                                                   OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, //
                                                   nullptr);
        SC_TRY_IF(opaque.fileHandle != INVALID_HANDLE_VALUE);

        if (threadingRunner)
        {
            opaque.overlapped.overlapped.hEvent                   = CreateEvent(nullptr, FALSE, 0, nullptr);
            threadingRunner->hEvents[threadingRunner->numEntries] = opaque.overlapped.overlapped.hEvent;
            threadingRunner->entries[threadingRunner->numEntries] = entry;
            threadingRunner->numEntries++;
        }
        else
        {
            // TODO: wrap this into a method
            opaque.overlapped.userData                = &eventLoopRunner->eventLoopAsync;
            eventLoopRunner->eventLoopAsync.eventLoop = &eventLoopRunner->eventLoop;
            // 3rd parameter (lpCompletionKey) will get reported in AsyncResult::userData in the callback
            HANDLE res = CreateIoCompletionPort(opaque.fileHandle, loopFDS, reinterpret_cast<ULONG_PTR>(entry), 0);
            if (res == NULL)
            {
                // TODO: Parse error properly
                return "CreateIoCompletionPort error"_a8;
            }
            eventLoopRunner->eventLoop.increaseActiveCount();
        }

        BOOL success = ReadDirectoryChangesW(opaque.fileHandle,                 //
                                             opaque.changesBuffer,              //
                                             sizeof(opaque.changesBuffer),      //
                                             TRUE,                              // watchSubtree
                                             FILE_NOTIFY_CHANGE_FILE_NAME |     //
                                                 FILE_NOTIFY_CHANGE_DIR_NAME |  //
                                                 FILE_NOTIFY_CHANGE_LAST_WRITE, //
                                             nullptr,                           // lpBytesReturned
                                             &opaque.overlapped.overlapped,     // lpOverlapped
                                             nullptr);                          // lpCompletionRoutine
        SC_TRY_MSG(success == TRUE, "ReadDirectoryChangesW"_a8);

        if (threadingRunner and not threadingRunner->thread.wasStarted())
        {
            threadingRunner->shouldStop.exchange(false);
            SC_TRY_IF(threadingRunner->thread.start("FileSystemWatcher::init", &threadingRunner->threadFunction))
        }
        return true;
    }

    void threadRun()
    {
        ThreadRunnerInternal& runner = *threadingRunner;
        while (not runner.shouldStop.load())
        {
            const DWORD result = WaitForMultipleObjects(runner.numEntries, runner.hEvents, TRUE, INFINITE);
            if (result != WAIT_FAILED and not runner.shouldStop.load())
            {
                const DWORD            index  = result - WAIT_OBJECT_0;
                FolderWatcher&         entry  = *runner.entries[index];
                FolderWatcherInternal& opaque = entry.internal.get();
                DWORD                  transferredBytes;
                GetOverlappedResult(opaque.fileHandle, &opaque.overlapped.overlapped, &transferredBytes, FALSE);
                notifyEntry(entry);
            }
        }
        threadingRunner->shouldStop.exchange(false);
    }

    void onEventLoopNotification(AsyncResult::Timeout& result)
    {
        // Comes from completion key passed to CreateIoCompletionPort
        FolderWatcher& entry = *reinterpret_cast<FolderWatcher*>(result.userData);
        // We get called after closing the handle
        if (entry.internal.get().fileHandle != INVALID_HANDLE_VALUE)
        {
            notifyEntry(entry);
        }
    }

    static void notifyEntry(FolderWatcher& entry)
    {
        FolderWatcherInternal&   opaque = entry.internal.get();
        FILE_NOTIFY_INFORMATION* event  = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(opaque.changesBuffer);

        Notification notification;
        notification.basePath = entry.path->view();
        while (notification.basePath.sizeASCII() > 1 and notification.basePath.endsWithChar('\\'))
        {
            notification.basePath =
                notification.basePath.sliceStartEnd<StringIteratorASCII>(0, notification.basePath.sizeASCII() - 1);
        }

        do
        {
            const Span<const wchar_t> span(event->FileName, event->FileNameLength);
            const StringView          path(span, false, StringEncoding::Utf16);

            notification.relativePath = path;
            switch (event->Action)
            {
            case FILE_ACTION_MODIFIED: notification.operation = Operation::Modified; break;
            default: notification.operation = Operation::AddRemoveRename; break;
            }
            entry.notifyCallback(notification);

            *reinterpret_cast<uint8_t**>(&event) += event->NextEntryOffset;
        } while (event->NextEntryOffset);

        memset(&opaque.overlapped.overlapped, 0, sizeof(opaque.overlapped.overlapped));
        BOOL success = ReadDirectoryChangesW(opaque.fileHandle,                 //
                                             opaque.changesBuffer,              //
                                             sizeof(opaque.changesBuffer),      //
                                             TRUE,                              // watchSubtree
                                             FILE_NOTIFY_CHANGE_FILE_NAME |     //
                                                 FILE_NOTIFY_CHANGE_DIR_NAME |  //
                                                 FILE_NOTIFY_CHANGE_LAST_WRITE, //
                                             nullptr,                           // lpBytesReturned
                                             &opaque.overlapped.overlapped,     // lpOverlapped
                                             nullptr);                          // lpCompletionRoutine
        // TODO: Handle ReadDirectoryChangesW error
        (void)success;
    }
};

SC::ReturnCode SC::FileSystemWatcher::Notification::getFullPath(String& buffer, StringView& outStringView) const
{
    StringBuilder builder(buffer);
    SC_TRY_IF(builder.append(basePath));
    SC_TRY_IF(builder.append("\\"_a8));
    SC_TRY_IF(builder.append(relativePath));
    outStringView = buffer.view();
    return true;
}
