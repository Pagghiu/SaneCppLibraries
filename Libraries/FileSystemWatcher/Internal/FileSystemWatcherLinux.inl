// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../FileSystemWatcher/FileSystemWatcher.h"

#include "../../Foundation/Assert.h"
#include "../../Foundation/Deferred.h"
#include "../../Threading/Atomic.h"
#include "../../Threading/Threading.h"

#include <dirent.h>      // opendir, readdir, closedir
#include <errno.h>       // errno
#include <fcntl.h>       // open, O_RDONLY, O_DIRECTORY
#include <string.h>      // strlen
#include <sys/inotify.h> // inotify
#include <sys/select.h>  // fd_set / FD_ZERO
#include <sys/stat.h>    // fstat
#include <unistd.h>      // read

struct SC::FileSystemWatcher::FolderWatcherInternal
{
    struct Pair
    {
        int32_t notifyID   = 0;
        int32_t nameOffset = 0;

        bool operator==(Pair other) const { return notifyID == other.notifyID; }
    };

    Pair   notifyHandles[FolderWatcherSizes::MaxNumberOfSubdirs];
    size_t notifyHandlesCount = 0;

    char                       relativePathsStorage[1024];
    StringSpan::NativeWritable relativePaths;

    FolderWatcher* parentEntry = nullptr; // We could in theory use SC_COMPILER_FIELD_OFFSET somehow to obtain it...

    FolderWatcherInternal() { relativePaths.writableSpan = {relativePathsStorage}; }
};

struct SC::FileSystemWatcher::ThreadRunnerInternal
{
    Thread       thread;
    Atomic<bool> shouldStop = false;

    // Allows unblocking the ::read() when stopping the watcher [0]=read end, [1]=write end
    int shutdownPipe[2] = {-1, -1};
};

struct SC::FileSystemWatcher::Internal
{
    FileSystemWatcher*    self            = nullptr;
    EventLoopRunner*      eventLoopRunner = nullptr;
    ThreadRunnerInternal* threadingRunner = nullptr;

    int notifyFd = -1; // inotify file descriptor

    Result init(FileSystemWatcher& parent, ThreadRunner& runner)
    {
        self            = &parent;
        threadingRunner = &runner.get();

        if (::pipe2(threadingRunner->shutdownPipe, O_CLOEXEC) == -1)
        {
            return Result::Error("pipe2 failed");
        }
        notifyFd = ::inotify_init1(IN_CLOEXEC);
        return notifyFd != -1 ? Result(true) : Result::Error("inotify_init1 failed");
    }

    Result init(FileSystemWatcher& parent, EventLoopRunner& runner)
    {
        self            = &parent;
        eventLoopRunner = &runner;

        notifyFd = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (notifyFd == -1)
        {
            return Result::Error("inotify_init1 failed");
        }

        eventLoopRunner->internalInit(parent, notifyFd);
        return eventLoopRunner->linuxStartSharedFilePoll();
    }

    Result close()
    {
        if (eventLoopRunner)
        {
            SC_TRY(eventLoopRunner->linuxStopSharedFilePoll());
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
                while (::write(threadingRunner->shutdownPipe[1], &dummy, sizeof(dummy)) == -1)
                {
                    if (errno != EINTR)
                    {
                        return Result::Error("write to shutdown pipe failed");
                    }
                }
                if (threadingRunner->shutdownPipe[0] != -1)
                {
                    ::close(threadingRunner->shutdownPipe[0]);
                    threadingRunner->shutdownPipe[0] = -1;
                }
                if (threadingRunner->shutdownPipe[1] != -1)
                {
                    ::close(threadingRunner->shutdownPipe[1]);
                    threadingRunner->shutdownPipe[1] = -1;
                }
                SC_TRY(threadingRunner->thread.join());
            }
        }
        if (notifyFd != -1)
        {
            ::close(notifyFd);
            notifyFd = -1;
        }
        return Result(true);
    }

    Result stopWatching(FolderWatcher& folderWatcher)
    {
        folderWatcher.parent->watchers.remove(folderWatcher);
        folderWatcher.parent = nullptr;

        FolderWatcherInternal& folderInternal = folderWatcher.internal.get();

        if (notifyFd == -1)
        {
            return Result::Error("invalid notifyFd");
        }

        for (size_t idx = 0; idx < folderInternal.notifyHandlesCount; ++idx)
        {
            const int res = ::inotify_rm_watch(notifyFd, folderInternal.notifyHandles[idx].notifyID);
            SC_TRY_MSG(res != -1, "inotify_rm_watch");
        }
        folderInternal.notifyHandlesCount   = 0; // Reset the count to zero
        folderInternal.relativePaths.length = 0;
        return Result(true);
    }

    static Result getSubFolderPath(StringPath& path, const StringPath& entryPath, const char* name,
                                   FolderWatcherInternal& opaque, int notifyHandleId)
    {
        // Append '/' and the subdirectory name
        path = entryPath;
        const char* dirStart =
            opaque.notifyHandles[notifyHandleId].nameOffset >= 0
                ? opaque.relativePaths.writableSpan.data() + opaque.notifyHandles[notifyHandleId].nameOffset
                : "";

        const StringSpan relativeDirectory = StringSpan::fromNullTerminated(dirStart, StringEncoding::Utf8);
        const StringSpan relativeName      = StringSpan::fromNullTerminated(name, StringEncoding::Utf8);

        if (not relativeDirectory.isEmpty())
        {
            SC_TRY_MSG(path.path.append("/"), "Relative path too long");
            SC_TRY_MSG(path.path.append(relativeDirectory), "Relative path too long");
        }
        SC_TRY_MSG(path.path.append("/"), "Relative path too long");
        SC_TRY_MSG(path.path.append(relativeName), "Relative path too long");
        return Result(true);
    }

    Result startWatching(FolderWatcher* entry)
    {
        // TODO: Add check for trying to watch folders already being watched or children of recursive watches

        SC_TRY_MSG(entry->path.path.view().getEncoding() != StringEncoding::Utf16,
                   "FolderWatcher on Linux does not support UTF16 encoded paths. Use UTF8 or ASCII encoding instead.");
        FolderWatcherInternal& opaque = entry->internal.get();
        if (not entry->subFolderRelativePathsBuffer.empty())
        {
            opaque.relativePaths.writableSpan = entry->subFolderRelativePathsBuffer;
        }
        opaque.relativePaths.length = 0;

        StringPath currentPath = entry->path;

        int rootNotifyFd;
        if (notifyFd == -1)
        {
            return Result::Error("invalid notifyFd");
        }
        rootNotifyFd = notifyFd;
        constexpr int mask =
            IN_ATTRIB | IN_CREATE | IN_MODIFY | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_MOVED_FROM | IN_MOVED_TO;
        const int newHandle = ::inotify_add_watch(rootNotifyFd, currentPath.path.buffer, mask);
        if (newHandle == -1)
        {
            return Result::Error("inotify_add_watch");
        }
        FolderWatcherInternal::Pair pair;
        pair.notifyID   = newHandle;
        pair.nameOffset = -1;
        SC_TRY_MSG(opaque.notifyHandlesCount < FolderWatcherSizes::MaxNumberOfSubdirs,
                   "Too many subdirectories being watched");
        opaque.notifyHandles[opaque.notifyHandlesCount++] = pair;

        // Watch all subfolders of current directory using a stack of file descriptors and names
        constexpr int MaxStackDepth = FolderWatcherSizes::MaxNumberOfSubdirs;
        struct DirStackEntry
        {
            int fd;
            int notifyHandleId;
        };
        DirStackEntry stack[MaxStackDepth];

        int stackSize = 0;

        // Push the root directory onto the stack
        DIR* rootDir = ::opendir(currentPath.path.buffer);
        if (!rootDir)
        {
            return Result::Error("Failed to open root directory");
        }
        const int rootFd = ::dirfd(rootDir);

        auto closeRootDir = MakeDeferred([rootDir] { ::closedir(rootDir); });

        stack[stackSize++] = {rootFd, static_cast<int>(opaque.notifyHandlesCount - 1)};

        const size_t rootPathLength = entry->path.path.view().sizeInBytes();
        // Clean the stack of fd in case any of the return Result::Error below is hit.
        auto deferredCleanStack = MakeDeferred(
            [&]
            {
                for (size_t idx = 0; idx < stackSize; ++idx)
                {
                    ::close(stack[idx].fd);
                }
            });
        while (stackSize > 0)
        {
            DirStackEntry entryStack = stack[--stackSize];

            DIR* dir = ::fdopendir(entryStack.fd);
            if (!dir)
            {
                ::close(entryStack.fd); // Prevent file descriptor leak if fdopendir fails
                continue;               // Cannot open directory, just skip it
            }
            auto closeDir = MakeDeferred([dir] { ::closedir(dir); });

            struct dirent* subDirectory;
            while ((subDirectory = ::readdir(dir)) != nullptr)
            {
                if (::strcmp(subDirectory->d_name, ".") == 0 || ::strcmp(subDirectory->d_name, "..") == 0)
                {
                    continue;
                }
                SC_TRY(getSubFolderPath(currentPath, entry->path, subDirectory->d_name, opaque,
                                        entryStack.notifyHandleId));

                struct stat st;
                if (::stat(currentPath.path.buffer, &st) != 0)
                {
                    continue;
                }
                if (S_ISDIR(st.st_mode))
                {
                    const int newHandle = ::inotify_add_watch(rootNotifyFd, currentPath.path.buffer, mask);
                    if (newHandle == -1)
                    {
                        (void)stopWatching(*entry);
                        return Result::Error("inotify_add_watch (subdirectory)");
                    }
                    if (stackSize < MaxStackDepth)
                    {
                        // Open the subdirectory and push onto the stack
                        const int subFd = ::open(currentPath.path.buffer, O_RDONLY | O_DIRECTORY);
                        if (subFd != -1)
                        {
                            stack[stackSize].fd             = subFd;
                            stack[stackSize].notifyHandleId = opaque.notifyHandlesCount;
                            stackSize++;
                        }
                    }
                    else
                    {
                        (void)stopWatching(*entry);
                        return Result::Error("Exceeded maximum stack depth for nested directories");
                    }
                    const char* relativePath = currentPath.path.buffer + rootPathLength;
                    if (relativePath[0] == '/')
                    {
                        relativePath++;
                    }
                    pair.notifyID   = newHandle;
                    pair.nameOffset = opaque.relativePaths.length == 0 ? 0 : opaque.relativePaths.length + 1;
                    StringSpan relativePathSpan = StringSpan::fromNullTerminated(relativePath, StringEncoding::Utf8);
                    SC_TRY_MSG(relativePathSpan.appendNullTerminatedTo(opaque.relativePaths, false),
                               "Not enough buffer space to hold sub-folders relative paths");

                    SC_TRY_MSG(opaque.notifyHandlesCount < FolderWatcherSizes::MaxNumberOfSubdirs,
                               "Too many subdirectories being watched");
                    opaque.notifyHandles[opaque.notifyHandlesCount++] = pair;
                }
            }
        }

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
            // Setup a select fd_set to listen on both notifyFd and shutdownPipe simultaneously
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(notifyFd, &fds);
            FD_SET(runner.shutdownPipe[0], &fds);
            const int maxFd = notifyFd > runner.shutdownPipe[0] ? notifyFd : runner.shutdownPipe[0];

            int selectRes;
            do
            {
                // Block until some events are received on the notifyFd or when shutdownPipe is written to
                selectRes = ::select(maxFd + 1, &fds, nullptr, nullptr, nullptr);
            } while (selectRes == -1 and errno == EINTR);

            if (threadingRunner->shutdownPipe[0] == -1 or FD_ISSET(runner.shutdownPipe[0], &fds))
            {
                return; // Interrupted by write to shutdown pipe (from close())
            }
            // Here select has received data on notifyHandle
            readAndNotify(notifyFd, self->watchers);
        }
        threadingRunner->shouldStop.exchange(false);
    }

    static void readAndNotify(const int notifyFd, WatcherLinkedList watchers)
    {
        int  numReadBytes;
        char inotifyBuffer[3 * 1024];
        // TODO: Handle the case where kernel is sending more than 3kb of events.
        do
        {
            numReadBytes = ::read(notifyFd, inotifyBuffer, sizeof(inotifyBuffer));
        } while (numReadBytes == -1 and errno == EINTR);

        Span<char> actuallyRead = {inotifyBuffer, static_cast<size_t>(numReadBytes)};
        notifyWatchers(actuallyRead, watchers);
    }

    static void notifyWatchers(Span<char> actuallyRead, WatcherLinkedList watchers)
    {
        const struct inotify_event* event     = nullptr;
        const struct inotify_event* prevEvent = nullptr;

        // Loop through all inotify_event and find the associated FolderWatcher to notify
        for (const char* iterator = actuallyRead.data();                  //
             iterator < actuallyRead.data() + actuallyRead.sizeInBytes(); //
             iterator += sizeof(*event) + event->len)
        {
            event = reinterpret_cast<const struct inotify_event*>(iterator);
            for (const FolderWatcher* entry = watchers.front; entry != nullptr; entry = entry->next)
            {
                // Check if current FolderWatcher has a watcher matching the one from current event
                FolderWatcherInternal::Pair pair{event->wd, 0};
                for (size_t idx = 0; idx < entry->internal.get().notifyHandlesCount; ++idx)
                {
                    if (entry->internal.get().notifyHandles[idx] == pair)
                    {
                        (void)notifySingleEvent(event, prevEvent, entry, idx);
                        prevEvent = event;
                        break;
                    }
                }
            }
        }
    }

    [[nodiscard]] static Result notifySingleEvent(const struct inotify_event* event,
                                                  const struct inotify_event* prevEvent, const FolderWatcher* entry,
                                                  size_t foundIndex)
    {
        StringPath   eventPath;
        Notification notification;

        notification.basePath = entry->path.path;

        // 1. Compute relative Path
        if (foundIndex == 0)
        {
            // Something changed in the original root folder being watched
            notification.relativePath = StringSpan({event->name, ::strlen(event->name)}, true, StringEncoding::Utf8);
        }
        else
        {
            // Something changed in any of the sub folders of the original root folder being watched
            const FolderWatcherInternal& internal = entry->internal.get();

            const char* dirStart =
                internal.notifyHandles[foundIndex].nameOffset >= 0
                    ? internal.relativePaths.writableSpan.data() + internal.notifyHandles[foundIndex].nameOffset
                    : "";

            const StringSpan relativeDirectory = StringSpan::fromNullTerminated(dirStart, StringEncoding::Utf8);
            const StringSpan relativeName      = StringSpan::fromNullTerminated(event->name, StringEncoding::Utf8);

            SC_TRY_MSG(eventPath.path.assign(relativeDirectory), "Relative path too long");
            SC_TRY_MSG(eventPath.path.append("/"), "Relative path too long");
            SC_TRY_MSG(eventPath.path.append(relativeName), "Relative path too long");

            notification.relativePath = eventPath.path.view();
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

SC::Result SC::FileSystemWatcher::Notification::getFullPath(StringPath& buffer) const
{
    SC_TRY_MSG(buffer.path.assign(basePath), "Buffer too small to hold full path");
    SC_TRY_MSG(buffer.path.append("/"), "Buffer too small to hold full path");
    SC_TRY_MSG(buffer.path.append(relativePath), "Buffer too small to hold full path");
    return Result(true);
}

void SC::FileSystemWatcher::asyncNotify(FolderWatcher*)
{
    internal.get().readAndNotify(internal.get().notifyFd, internal.get().self->watchers);
}
