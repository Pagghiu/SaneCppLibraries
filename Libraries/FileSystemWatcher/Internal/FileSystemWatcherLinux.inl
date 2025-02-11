// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <string.h>      // strlen
#include <sys/inotify.h> // inotify
#include <sys/select.h>  // fd_set / FD_ZERO
#include <sys/stat.h>    // fstat
#include <unistd.h>      // read

#include "../../Containers/Array.h"
#include "../../File/File.h"
#include "../../FileSystemIterator/FileSystemIterator.h"
#include "../../Strings/String.h"
#include "../../Strings/StringConverter.h"
#include "../../Threading/Threading.h"

struct SC::FileSystemWatcher::FolderWatcherInternal
{
    struct Pair
    {
        int32_t notifyID   = 0;
        int32_t nameOffset = 0;

        bool operator==(Pair other) const { return notifyID == other.notifyID; }
    };

    Array<Pair, FolderWatcherSizes::MaxNumberOfSubdirs> notifyHandles;

    Buffer relativePaths;

    FolderWatcher* parentEntry = nullptr; // We could in theory use SC_COMPILER_FIELD_OFFSET somehow to obtain it...
};

struct SC::FileSystemWatcher::ThreadRunnerInternal
{
    Thread       thread;
    Atomic<bool> shouldStop = false;

    PipeDescriptor shutdownPipe; // Allows unblocking the ::read() when stopping the watcher
};

struct SC::FileSystemWatcher::Internal
{
    FileSystemWatcher*    self            = nullptr;
    EventLoopRunner*      eventLoopRunner = nullptr;
    ThreadRunnerInternal* threadingRunner = nullptr;

    FileDescriptor notifyFd;

    [[nodiscard]] Result init(FileSystemWatcher& parent, ThreadRunner& runner)
    {
        self            = &parent;
        threadingRunner = &runner.get();

        SC_TRY(threadingRunner->shutdownPipe.createPipe());
        return notifyFd.assign(::inotify_init1(IN_CLOEXEC));
    }

    [[nodiscard]] Result init(FileSystemWatcher& parent, EventLoopRunner& runner)
    {
        self            = &parent;
        eventLoopRunner = &runner;

        int notifyHandle = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        SC_TRY(notifyFd.assign(notifyHandle));

        SC_TRY(eventLoopRunner->eventLoop->associateExternallyCreatedFileDescriptor(notifyFd));
        runner.asyncPoll.callback.bind<Internal, &Internal::onEventLoopNotification>(*this);
        return runner.asyncPoll.start(*eventLoopRunner->eventLoop, notifyHandle);
    }

    [[nodiscard]] Result close()
    {
        if (eventLoopRunner)
        {
            SC_TRY(eventLoopRunner->asyncPoll.stop());
        }

        for (FolderWatcher* entry = self->watchers.front; entry != nullptr; entry = entry->next)
        {
            SC_TRY(stopWatching(*entry));
        }

        if (threadingRunner)
        {
            if (threadingRunner->thread.wasStarted())
            {
                threadingRunner->shouldStop.exchange(true);
                // Write to shutdownPipe to unblock the ::select() in the dedicated thread
                char dummy = 1;
                SC_TRY(threadingRunner->shutdownPipe.writePipe.write({&dummy, sizeof(dummy)}));
                SC_TRY(threadingRunner->shutdownPipe.close());
                SC_TRY(threadingRunner->thread.join());
            }
        }
        SC_TRY(notifyFd.close());
        return Result(true);
    }

    [[nodiscard]] Result stopWatching(FolderWatcher& folderWatcher)
    {
        folderWatcher.parent->watchers.remove(folderWatcher);
        folderWatcher.parent = nullptr;

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
        return Result(true);
    }

    [[nodiscard]] Result startWatching(FolderWatcher* entry)
    {
        // TODO: Add check for trying to watch folders already being watched or children of recursive watches
        StringNative<1024> buffer = StringEncoding::Native; // TODO: this needs to go into caller context
        StringView         encodedPath;
        SC_TRY(StringConverter(buffer).convertNullTerminateFastPath(entry->path.view(), encodedPath));
        FolderWatcherInternal& opaque = entry->internal.get();

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

        // Watch all subfolders of current directoy.
        // TODO: We should also dynamically add / remove watched directories added after now...
        StringConverter    converter(opaque.relativePaths, StringEncoding::Utf8);
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
                SC_TRY(converter.appendNullTerminated(iterator.get().name, false)); // keep null terminator
                SC_TRY(opaque.notifyHandles.push_back(pair));
            }
        }
        SC_TRY(iterator.checkErrors());

        opaque.parentEntry = entry;

        // Launch the thread that monitors the inotify watch if we're on thread runner
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
            int notifyHandle;
            SC_ASSERT_RELEASE(notifyFd.get(notifyHandle, Result(false)));

            int shutdownHandle;
            SC_ASSERT_RELEASE(runner.shutdownPipe.readPipe.get(shutdownHandle, Result(false)));

            // Setup a select fd_set to listen on both notifyHandle and shutdownHandle simultaneously
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(notifyHandle, &fds);
            FD_SET(shutdownHandle, &fds);
            const int maxFd = notifyHandle > shutdownHandle ? notifyHandle : shutdownHandle;

            int selectRes;
            do
            {
                // Block until some events are received on the notifyFd or when shutdownPipe is written to
                selectRes = ::select(maxFd + 1, &fds, nullptr, nullptr, nullptr);
            } while (selectRes == -1 and errno == EINTR);

            if (FD_ISSET(shutdownHandle, &fds))
            {
                return; // Interrupted by shutdownPipe.writePipe.write (from close())
            }
            // Here select has received data on notifyHandle
            readAndNotify(notifyFd, self->watchers);
        }
        threadingRunner->shouldStop.exchange(false);
    }

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
        // TODO: Handle the case where kernel is sending more than 3kb of events.
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

        // Loop through all inotify_event and find the associated FolderWatcher to notify
        SmallString<1024> bufferString;
        for (const char* iterator = actuallyRead.data();                  //
             iterator < actuallyRead.data() + actuallyRead.sizeInBytes(); //
             iterator += sizeof(*event) + event->len)
        {
            event = reinterpret_cast<const struct inotify_event*>(iterator);
            for (const FolderWatcher* entry = watchers.front; entry != nullptr; entry = entry->next)
            {
                size_t foundIndex;
                // Check if current FolderWatcher has a watcher matching the one from current event
                if (entry->internal.get().notifyHandles.contains(FolderWatcherInternal::Pair{event->wd, 0},
                                                                 &foundIndex))
                {
                    (void)notifySingleEvent(event, prevEvent, entry, foundIndex, bufferString);
                    prevEvent = event;
                    break;
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

        // 1. Compute relative Path
        if (foundIndex == 0)
        {
            // Something changed in the original root folder being watched
            notification.relativePath = StringView({event->name, ::strlen(event->name)}, true, StringEncoding::Utf8);
        }
        else
        {
            // Something changed in any of the sub folders of the original root folder being watched
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

        // 2. Compute event Type
        if (event->mask & (IN_ATTRIB | IN_MODIFY))
        {
            // Try to coalesce Modified after AddRemoveRename for consistency with the other backends
            // I'm not really sure that Modified is consistently pushed after AddRemoveRename from Linux Kernel.
            if (prevEvent != nullptr and (prevEvent->wd == event->wd))
            {
                return Result(false);
            }
            notification.operation = Operation::Modified;
        }
        if (event->mask & ~(IN_ATTRIB | IN_MODIFY))
        {
            notification.operation = Operation::AddRemoveRename;
        }

        // 3. Finally invoke user callback with the notification
        entry->notifyCallback(notification);
        return Result(true);
    }
};

SC::Result SC::FileSystemWatcher::Notification::getFullPath(String& buffer, StringView& outStringView) const
{
    StringConverter converter(buffer, StringConverter::Clear);
    SC_TRY(converter.appendNullTerminated(basePath));
    SC_TRY(converter.appendNullTerminated("/"));
    SC_TRY(converter.appendNullTerminated(relativePath));
    outStringView = buffer.view();
    return Result(true);
}
