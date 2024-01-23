// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include "../../FileSystemIterator/FileSystemIterator.h"
#include <errno.h>
#include <limits.h>      // PATH_MAX
#include <signal.h>      //
#include <stdio.h>       //snprintf
#include <string.h>      // strlen
#include <sys/inotify.h> // inotify
#include <sys/stat.h>    // fstat
#include <unistd.h>      // read
#endif

#include "../../Strings/SmallString.h"
#include "../../Strings/StringBuilder.h"
#include "../../Strings/StringConverter.h"
#include "../../Threading/Threading.h"

struct SC::FileSystemWatcher::FolderWatcherInternal
{
#if SC_PLATFORM_WINDOWS
    OVERLAPPED&    getOverlapped() { return asyncPoll.getOverlappedOpaque().get().overlapped; }
    uint8_t        changesBuffer[FolderWatcherSizes::MaxChangesBufferSize];
    FileDescriptor fileHandle;
#else
    struct Pair
    {
        int32_t notifyID   = 0;
        int32_t nameOffset = 0;

        bool operator==(Pair other) const { return notifyID == other.notifyID; }
    };

    Array<Pair, FolderWatcherSizes::MaxNumberOfSubdirs> notifyHandles;

    Vector<char>   relativePaths;

#endif
    FolderWatcher* parentEntry = nullptr; // We could in theory use SC_COMPILER_FIELD_OFFSET somehow to obtain it...
};

struct SC::FileSystemWatcher::ThreadRunnerInternal
{
    Thread       thread;
    Atomic<bool> shouldStop = false;

#if SC_PLATFORM_WINDOWS
    static constexpr int N = ThreadRunnerDefinition::MaxWatchablePaths;

    HANDLE         hEvents[N] = {0};
    FolderWatcher* entries[N] = {nullptr};
    DWORD          numEntries = 0;
#else
    PipeDescriptor shutdownPipe;
#endif
};

struct SC::FileSystemWatcher::Internal
{
    FileSystemWatcher*    self            = nullptr;
    EventLoopRunner*      eventLoopRunner = nullptr;
    ThreadRunnerInternal* threadingRunner = nullptr;
#if SC_PLATFORM_WINDOWS
#else
    FileDescriptor notifyFd;
#endif
    [[nodiscard]] Result init(FileSystemWatcher& parent, ThreadRunner& runner)
    {
        self            = &parent;
        threadingRunner = &runner.get();
#if SC_PLATFORM_WINDOWS
        return Result(true);
#else
        SC_TRY(threadingRunner->shutdownPipe.createPipe());
        return notifyFd.assign(::inotify_init1(IN_CLOEXEC));
#endif
    }

    [[nodiscard]] Result init(FileSystemWatcher& parent, EventLoopRunner& runner)
    {
        self            = &parent;
        eventLoopRunner = &runner;
#if SC_PLATFORM_WINDOWS
#else
        int notifyHandle = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        SC_TRY(notifyFd.assign(notifyHandle));

        SC_TRY(eventLoopRunner->eventLoop.associateExternallyCreatedFileDescriptor(notifyFd));
        runner.asyncPoll.callback.bind<Internal, &Internal::onEventLoopNotification>(*this);
        return runner.asyncPoll.start(eventLoopRunner->eventLoop, notifyHandle);
#endif
    }

    [[nodiscard]] Result close()
    {
#if SC_PLATFORM_WINDOWS
#else
        if (eventLoopRunner)
        {
            SC_TRY(eventLoopRunner->asyncPoll.stop());
        }

        for (FolderWatcher* entry = self->watchers.front; entry != nullptr; entry = entry->next)
        {
            SC_TRY(stopWatching(*entry));
        }
#endif
        if (threadingRunner)
        {
            if (threadingRunner->thread.wasStarted())
            {
                threadingRunner->shouldStop.exchange(true);
#if SC_PLATFORM_WINDOWS
                do
                {
                    for (DWORD idx = 0; idx < threadingRunner->numEntries; ++idx)
                    {
                        ::SetEvent(threadingRunner->hEvents[idx]);
                    }
                } while (threadingRunner->shouldStop.load());
#else
                char dummy = 1;
                SC_TRY(threadingRunner->shutdownPipe.writePipe.write({&dummy, sizeof(dummy)}));
                SC_TRY(threadingRunner->shutdownPipe.close());
                SC_TRY(notifyFd.close());
#endif
                SC_TRY(threadingRunner->thread.join());
            }
        }

#if SC_PLATFORM_WINDOWS
        for (FolderWatcher* entry = self->watchers.front; entry != nullptr; entry = entry->next)
        {
            SC_TRY(stopWatching(*entry));
        }
#endif
        return Result(true);
    }

#if SC_PLATFORM_WINDOWS
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
#endif

    [[nodiscard]] Result stopWatching(FolderWatcher& folderWatcher)
    {
        folderWatcher.parent->watchers.remove(folderWatcher);
        folderWatcher.parent = nullptr;
#if SC_PLATFORM_WINDOWS
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
#else
        FolderWatcherInternal& folderInternal = folderWatcher.internal.get();

        int rootNotifyFd;
        SC_TRY(notifyFd.get(rootNotifyFd, Result::Error("invalid notifyFd")));

        for (FolderWatcherInternal::Pair pair : folderInternal.notifyHandles)
        {
            const int res = ::inotify_rm_watch(rootNotifyFd, pair.notifyID);
            SC_TRY_MSG(res != -1, "inotify_rm_watch");
        }
        folderInternal.notifyHandles.clear();
        folderInternal.relativePaths.clear();
#endif
        return Result(true);
    }

    [[nodiscard]] Result startWatching(FolderWatcher* entry)
    {
        // TODO: Add check for trying to watch folders already being watched or children of recursive watches
        StringNative<1024>     buffer = StringEncoding::Native; // TODO: this needs to go into caller context
        StringConverter        converter(buffer);
        FileDescriptor::Handle loopFDS = FileDescriptor::Invalid;
        if (eventLoopRunner)
        {
            SC_TRY(eventLoopRunner->eventLoop.getLoopFileDescriptor(loopFDS));
        }
        StringView encodedPath;
        SC_TRY(converter.convertNullTerminateFastPath(entry->path.view(), encodedPath));
        FolderWatcherInternal& opaque = entry->internal.get();
#if SC_PLATFORM_WINDOWS
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
        HANDLE newHandle = ::CreateFileW(encodedPath.getNullTerminatedNative(),                            //
                                         FILE_LIST_DIRECTORY,                                              //
                                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,           //
                                         nullptr,                                                          //
                                         OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, //
                                         nullptr);

        SC_TRY(newHandle != INVALID_HANDLE_VALUE);
        SC_TRY(opaque.fileHandle.assign(newHandle));
#else
        int rootNotifyFd;
        SC_TRY(notifyFd.get(rootNotifyFd, Result::Error("invalid notifyFd")));
        const int newHandle = ::inotify_add_watch(rootNotifyFd, encodedPath.getNullTerminatedNative(),
                                                  IN_ATTRIB | IN_CREATE | IN_MODIFY | IN_DELETE | IN_DELETE_SELF |
                                                      IN_MOVE_SELF | IN_MOVED_FROM | IN_MOVED_TO);
        if (newHandle == -1)
        {
            return Result::Error("inotify_add_watch");
        }
        FolderWatcherInternal::Pair pair;
        pair.notifyID   = newHandle;
        pair.nameOffset = 0;
        SC_TRY(opaque.notifyHandles.push_back(pair));

        // TODO: We should also dynamically add / remove watched directories added after now...
        StringConverter    sb(opaque.relativePaths, StringEncoding::Utf8);
        FileSystemIterator iterator;
        iterator.options.recursive = true;
        SC_TRY(iterator.init(encodedPath));
        while (iterator.enumerateNext())
        {
            if (iterator.get().isDirectory())
            {
                int notifyFd = ::inotify_add_watch(rootNotifyFd, iterator.get().path.getNullTerminatedNative(),
                                                   IN_ATTRIB | IN_CREATE | IN_MODIFY | IN_DELETE | IN_DELETE_SELF |
                                                       IN_MOVE_SELF | IN_MOVED_FROM | IN_MOVED_TO);
                if (notifyFd == -1)
                {
                    (void)stopWatching(*entry); // This is to remove notifications for all directories added so far
                    return Result::Error("inotify_add_watch (subdirectory)");
                }
                pair.notifyID   = notifyFd;
                pair.nameOffset = opaque.relativePaths.size();
                SC_TRY(sb.appendNullTerminated(iterator.get().name, false)); // keep null terminator
                SC_TRY(opaque.notifyHandles.push_back(pair));
            }
        }
        SC_TRY(iterator.checkErrors());
#endif

        opaque.parentEntry = entry;

#if SC_PLATFORM_WINDOWS
        if (threadingRunner)
        {
            opaque.getOverlapped().hEvent = ::CreateEventW(nullptr, FALSE, 0, nullptr);

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
#endif
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
        thread.setThreadName(SC_NATIVE_STR("FileSystemWatcher"));
        ThreadRunnerInternal& runner = *threadingRunner;
        while (not runner.shouldStop.load())
        {
#if SC_PLATFORM_WINDOWS
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
#else
            int notifyHandle;
            SC_ASSERT_RELEASE(notifyFd.get(notifyHandle, Result(false)));

            int shutdownHandle;
            SC_ASSERT_RELEASE(runner.shutdownPipe.readPipe.get(shutdownHandle, Result(false)));

            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(notifyHandle, &fds);
            FD_SET(shutdownHandle, &fds);
            const int maxFd = notifyHandle > shutdownHandle ? notifyHandle : shutdownHandle;

            int selectRes;
            do
            {
                selectRes = ::select(maxFd + 1, &fds, nullptr, nullptr, nullptr);
            } while (selectRes == -1 and errno == EINTR);

            if (FD_ISSET(shutdownHandle, &fds))
            {
                return; // Interrupted
            }

            readAndNotify(notifyFd, self->watchers);
#endif
        }
        threadingRunner->shouldStop.exchange(false);
    }

#if SC_PLATFORM_WINDOWS
    void onEventLoopNotification(AsyncWindowsPoll::Result& result)
    {
        SC_COMPILER_WARNING_PUSH_OFFSETOF;
        FolderWatcherInternal& fwi = SC_COMPILER_FIELD_OFFSET(FolderWatcherInternal, asyncPoll, result.async);
        SC_COMPILER_WARNING_POP;

        SC_ASSERT_DEBUG(fwi.fileHandle.isValid());
        notifyEntry(*fwi.parentEntry);
        result.reactivateRequest(true);
    }

    static void notifyEntry(FolderWatcher& entry)
    {
        FolderWatcherInternal&   opaque = entry.internal.get();
        FILE_NOTIFY_INFORMATION* event  = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(opaque.changesBuffer);

        Notification notification;
        notification.basePath = entry.path.view();
        while (notification.basePath.sizeInBytes() > 1 and notification.basePath.endsWithCodePoint('\\'))
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
#else
    void onEventLoopNotification(AsyncFilePoll::Result& result)
    {
        readAndNotify(notifyFd, self->watchers);
        result.reactivateRequest(true);
    }

    static void readAndNotify(const FileDescriptor& notifyFd, IntrusiveDoubleLinkedList<FolderWatcher> watchers)
    {
        int notifyHandle;
        SC_ASSERT_RELEASE(notifyFd.get(notifyHandle, Result(false)));
        int  numReadBytes;
        char inotifyBuffer[3 * 1024];
        do
        {
            numReadBytes = ::read(notifyHandle, inotifyBuffer, sizeof(inotifyBuffer));
        } while (numReadBytes == -1 and errno == EINTR);

        Span<char> actuallyRead = {inotifyBuffer, static_cast<size_t>(numReadBytes)};
        notifyWatchers(actuallyRead, watchers);
    }

    static void notifyWatchers(Span<char> actuallyRead, IntrusiveDoubleLinkedList<FolderWatcher> watchers)
    {
        const struct inotify_event* event     = nullptr;
        const struct inotify_event* prevEvent = nullptr;

        SmallString<1024> bufferString;
        for (const char* iterator = actuallyRead.data();                  //
             iterator < actuallyRead.data() + actuallyRead.sizeInBytes(); //
             iterator += sizeof(*event) + event->len)
        {
            event = reinterpret_cast<const struct inotify_event*>(iterator);
            for (const FolderWatcher* entry = watchers.front; entry != nullptr; entry = entry->next)
            {
                size_t foundIndex;
                if (entry->internal.get().notifyHandles.contains(FolderWatcherInternal::Pair{event->wd, 0},
                                                                 &foundIndex))
                {
                    if (notifySingleEvent(event, prevEvent, entry, foundIndex, bufferString))
                    {
                        prevEvent = event;
                        break;
                    }
                    prevEvent = event;
                }
            }
        }
    }

    [[nodiscard]] static Result notifySingleEvent(const struct inotify_event* event,
                                                  const struct inotify_event* prevEvent, const FolderWatcher* entry,
                                                  size_t foundIndex, String& bufferString)
    {
        Notification notification;

        notification.basePath = entry->path.view();
        if (foundIndex == 0)
        {
            notification.relativePath = StringView({event->name, ::strlen(event->name)}, true, StringEncoding::Utf8);
        }
        else
        {
            const FolderWatcherInternal& internal = entry->internal.get();
            const char* dirStart = internal.relativePaths.data() + internal.notifyHandles[foundIndex].nameOffset;

            const StringView relativeDirectory({dirStart, ::strlen(dirStart)}, true, StringEncoding::Utf8);
            const StringView relativeName({event->name, ::strlen(event->name)}, true, StringEncoding::Utf8);

            StringConverter converter(bufferString, StringConverter::Clear);
            SC_TRY(converter.appendNullTerminated(relativeDirectory));
            SC_TRY(converter.appendNullTerminated("/"));
            SC_TRY(converter.appendNullTerminated(relativeName));
            notification.relativePath = bufferString.view();
        }
        if (event->mask & (IN_ATTRIB | IN_MODIFY))
        {
            if (prevEvent != nullptr and (prevEvent->wd == event->wd))
            {
                // Try to coalesce Modified after AddRemoveRename for consistency with the other backends
                // I'm not really sure that Modified is consistently pushed after AddRemoveRename from Linux Kernel.
                return Result(false);
            }
            notification.operation = Operation::Modified;
        }
        if (event->mask & ~(IN_ATTRIB | IN_MODIFY))
        {
            notification.operation = Operation::AddRemoveRename;
        }
        entry->notifyCallback(notification);
        return Result(true);
    }
#endif
};

SC::Result SC::FileSystemWatcher::Notification::getFullPath(String& buffer, StringView& outStringView) const
{
    StringBuilder builder(buffer);
    SC_TRY(builder.append(basePath));
#if SC_PLATFORM_WINDOWS
    SC_TRY(builder.append("\\"));
#else
    SC_TRY(builder.append("/"));
#endif
    SC_TRY(builder.append(relativePath));
    outStringView = buffer.view();
    return Result(true);
}
