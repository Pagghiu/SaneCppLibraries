// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../FileSystemWatcher/FileSystemWatcher.h"

#include "../../Foundation/Assert.h"
#include "../../Foundation/Deferred.h"
#include "../../Threading/Threading.h"

#include <dirent.h> // opendir, readdir, closedir
#include <errno.h>
#include <fcntl.h>       // open, O_RDONLY, O_DIRECTORY
#include <limits.h>      // PATH_MAX
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

    PipeDescriptor shutdownPipe; // Allows unblocking the ::read() when stopping the watcher
};

struct SC::FileSystemWatcher::Internal
{
    FileSystemWatcher*    self            = nullptr;
    EventLoopRunner*      eventLoopRunner = nullptr;
    ThreadRunnerInternal* threadingRunner = nullptr;

    FileDescriptor notifyFd;

    Result init(FileSystemWatcher& parent, ThreadRunner& runner)
    {
        self            = &parent;
        threadingRunner = &runner.get();

        SC_TRY(threadingRunner->shutdownPipe.createPipe());
        return notifyFd.assign(::inotify_init1(IN_CLOEXEC));
    }

    Result init(FileSystemWatcher& parent, EventLoopRunner& runner)
    {
        self            = &parent;
        eventLoopRunner = &runner;

        int notifyHandle = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        SC_TRY(notifyFd.assign(notifyHandle));

        SC_TRY(eventLoopRunner->eventLoop->associateExternallyCreatedFileDescriptor(notifyFd));
        runner.asyncPoll.callback.bind<Internal, &Internal::onEventLoopNotification>(*this);
        return runner.asyncPoll.start(*eventLoopRunner->eventLoop, notifyHandle);
    }

    Result close()
    {
        if (eventLoopRunner)
        {
            SC_TRY(eventLoopRunner->asyncPoll.stop(*eventLoopRunner->eventLoop));
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

    Result stopWatching(FolderWatcher& folderWatcher)
    {
        folderWatcher.parent->watchers.remove(folderWatcher);
        folderWatcher.parent = nullptr;

        FolderWatcherInternal& folderInternal = folderWatcher.internal.get();

        int rootNotifyFd;
        SC_TRY(notifyFd.get(rootNotifyFd, Result::Error("invalid notifyFd")));

        // for (FolderWatcherInternal::Pair pair : folderInternal.notifyHandles)
        for (size_t idx = 0; idx < folderInternal.notifyHandlesCount; ++idx)
        {
            const int res = ::inotify_rm_watch(rootNotifyFd, folderInternal.notifyHandles[idx].notifyID);
            SC_TRY_MSG(res != -1, "inotify_rm_watch");
        }
        folderInternal.notifyHandlesCount   = 0; // Reset the count to zero
        folderInternal.relativePaths.length = 0;
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

        char currentPath[PATH_MAX];
        ::memcpy(currentPath, entry->path.path.view().getNullTerminatedNative(), entry->path.path.view().sizeInBytes());
        currentPath[entry->path.path.view().sizeInBytes()] = '\0'; // Ensure null termination

        int rootNotifyFd;
        SC_TRY(notifyFd.get(rootNotifyFd, Result::Error("invalid notifyFd")));
        constexpr int mask =
            IN_ATTRIB | IN_CREATE | IN_MODIFY | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_MOVED_FROM | IN_MOVED_TO;
        const int newHandle = ::inotify_add_watch(rootNotifyFd, currentPath, mask);
        if (newHandle == -1)
        {
            return Result::Error("inotify_add_watch");
        }
        FolderWatcherInternal::Pair pair;
        pair.notifyID   = newHandle;
        pair.nameOffset = 0;
        SC_TRY_MSG(opaque.notifyHandlesCount < FolderWatcherSizes::MaxNumberOfSubdirs,
                   "Too many subdirectories being watched");
        opaque.notifyHandles[opaque.notifyHandlesCount++] = pair;

        // Watch all subfolders of current directory using a stack of file descriptors and names
        constexpr int MaxStackDepth = FolderWatcherSizes::MaxNumberOfSubdirs;
        struct DirStackEntry
        {
            int fd;
            int pathLength;
        };
        DirStackEntry stack[MaxStackDepth];

        int stackSize = 0;

        // Push the root directory onto the stack
        DIR* rootDir = ::opendir(currentPath);
        if (!rootDir)
        {
            return Result::Error("Failed to open root directory");
        }
        const int rootFd = ::dirfd(rootDir);

        auto closeRootDir = MakeDeferred([rootDir] { ::closedir(rootDir); });

        stack[stackSize++] = {rootFd, static_cast<int>(entry->path.path.view().sizeInBytes())};

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
            // Build the current path
            if (entryStack.pathLength < PATH_MAX)
            {
                currentPath[entryStack.pathLength] = '\0';
            }
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
                size_t nameLen = ::strlen(subDirectory->d_name);
                if (entryStack.pathLength + 1 + nameLen >= PATH_MAX)
                {
                    continue; // Skip if path is too long
                }
                // Append '/' and the subdirectory name
                currentPath[entryStack.pathLength] = '/';
                ::memcpy(currentPath + entryStack.pathLength + 1, subDirectory->d_name, nameLen);
                currentPath[entryStack.pathLength + 1 + nameLen] = '\0';
                struct stat st;
                if (::stat(currentPath, &st) != 0)
                {
                    continue;
                }
                if (S_ISDIR(st.st_mode))
                {
                    const int newHandle = ::inotify_add_watch(rootNotifyFd, currentPath, mask);
                    if (newHandle == -1)
                    {
                        (void)stopWatching(*entry);
                        return Result::Error("inotify_add_watch (subdirectory)");
                    }
                    if (stackSize < MaxStackDepth)
                    {
                        // Open the subdirectory and push onto the stack
                        const int subFd = ::open(currentPath, O_RDONLY | O_DIRECTORY);
                        if (subFd != -1)
                        {
                            stack[stackSize++] = {subFd, static_cast<int>(entryStack.pathLength + 1 + nameLen)};
                        }
                    }
                    else
                    {
                        (void)stopWatching(*entry);
                        return Result::Error("Exceeded maximum stack depth for nested directories");
                    }
                    const char* relativePath = currentPath + rootPathLength;
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
        char         bufferString[StringPath::MaxPath];
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
                internal.relativePaths.writableSpan.data() + internal.notifyHandles[foundIndex].nameOffset;

            const StringSpan relativeDirectory({dirStart, ::strlen(dirStart)}, true, StringEncoding::Utf8);
            const StringSpan relativeName({event->name, event->len - 1}, true, StringEncoding::Utf8);
            if (relativeDirectory.sizeInBytes() + relativeName.sizeInBytes() + 2 > StringPath::MaxPath)
            {
                return Result::Error("Relative path too long");
            }
            ::memcpy(bufferString, relativeDirectory.getNullTerminatedNative(), relativeDirectory.sizeInBytes());
            bufferString[relativeDirectory.sizeInBytes()] = '/'; // Add the separator
            ::memcpy(bufferString + relativeDirectory.sizeInBytes() + 1, relativeName.getNullTerminatedNative(),
                     relativeName.sizeInBytes());
            bufferString[relativeDirectory.sizeInBytes() + 1 + relativeName.sizeInBytes()] = '\0';

            notification.relativePath = {{bufferString, strlen(bufferString)}, true, StringEncoding::Utf8};
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
    if (StringPath::MaxPath < (basePath.sizeInBytes() + relativePath.sizeInBytes() + 2))
    {
        return Result::Error("Buffer too small to hold full path");
    }
    ::memcpy(buffer.path.buffer, basePath.getNullTerminatedNative(), basePath.sizeInBytes());
    buffer.path.buffer[basePath.sizeInBytes()] = '/'; // Add the separator
    ::memcpy(buffer.path.buffer + basePath.sizeInBytes() + 1, relativePath.getNullTerminatedNative(),
             relativePath.sizeInBytes());
    buffer.path.buffer[basePath.sizeInBytes() + 1 + relativePath.sizeInBytes()] = '\0'; // Null terminate the string
    buffer.path.length = basePath.sizeInBytes() + 1 + relativePath.sizeInBytes();
    return Result(true);
}
