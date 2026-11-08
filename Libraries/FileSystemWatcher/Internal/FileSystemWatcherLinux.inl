// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../FileSystemWatcher/FileSystemWatcher.h"
#include "FileSystemWatcherThreading.h"

#include "../../Common/Deferred.h"

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

        // Negative IDs retain cleanup ownership after their paths become invalid.
        int32_t nativeID() const { return notifyID < 0 ? -notifyID : notifyID; }
        void    suppress() { notifyID = -nativeID(); }
    };

    Pair   notifyHandles[FolderWatcherSizes::MaxNumberOfSubdirs];
    size_t notifyHandlesCount = 0;

    char relativePathsStorage[4096];

    StringSpan::NativeWritable relativePaths;

    FolderWatcher* parentEntry = nullptr; // We could in theory use SC_COMPILER_FIELD_OFFSET somehow to obtain it...

    FolderWatcherInternal() { relativePaths.writableSpan = {relativePathsStorage}; }
};

struct SC::FileSystemWatcher::ThreadRunnerInternal
{
    FSWThread     thread;
    FSWAtomicBool shouldStop;

    // Allows unblocking the ::read() when stopping the watcher [0]=read end, [1]=write end
    int shutdownPipe[2] = {-1, -1};

    void closeShutdownPipe()
    {
        for (int& fd : shutdownPipe)
        {
            if (fd != -1)
            {
                ::close(fd);
                fd = -1;
            }
        }
    }
};

struct SC::FileSystemWatcher::Internal
{
    FileSystemWatcher*    self            = nullptr;
    EventLoopRunner*      eventLoopRunner = nullptr;
    ThreadRunnerInternal* threadingRunner = nullptr;

    int  notifyFd         = -1; // inotify file descriptor
    bool monitoringFailed = false;

    ResultFileSystemWatcher init(FileSystemWatcher& parent, ThreadRunner& runner)
    {
        self             = &parent;
        threadingRunner  = &runner.get();
        monitoringFailed = false;

        if (::pipe2(threadingRunner->shutdownPipe, O_CLOEXEC) == -1)
        {
            const uint32_t nativeError = static_cast<uint32_t>(errno);

            self            = nullptr;
            threadingRunner = nullptr;
            return ResultFileSystemWatcher(FileSystemWatcherError::InitializationFailed,
                                           FileSystemWatcherErrorDetail::LinuxCreateShutdownPipe, nativeError);
        }
        notifyFd = ::inotify_init1(IN_CLOEXEC);
        if (notifyFd == -1)
        {
            const uint32_t nativeError = static_cast<uint32_t>(errno);
            threadingRunner->closeShutdownPipe();
            self            = nullptr;
            threadingRunner = nullptr;
            return ResultFileSystemWatcher(FileSystemWatcherError::InitializationFailed,
                                           FileSystemWatcherErrorDetail::LinuxInitializeInotify, nativeError);
        }
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher init(FileSystemWatcher& parent, EventLoopRunner& runner)
    {
        self             = &parent;
        eventLoopRunner  = &runner;
        monitoringFailed = false;

        notifyFd = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (notifyFd == -1)
        {
            const uint32_t nativeError = static_cast<uint32_t>(errno);

            self            = nullptr;
            eventLoopRunner = nullptr;
            return ResultFileSystemWatcher(FileSystemWatcherError::InitializationFailed,
                                           FileSystemWatcherErrorDetail::LinuxInitializeInotify, nativeError);
        }

        eventLoopRunner->internalInit(parent, notifyFd);
        const Result started = eventLoopRunner->linuxStartSharedFileReadiness();
        if (not started)
        {
            ::close(notifyFd);
            notifyFd = -1;

            eventLoopRunner->notifyFd          = -1;
            eventLoopRunner->fileSystemWatcher = nullptr;

            eventLoopRunner = nullptr;
            self            = nullptr;
        }
        return started;
    }

    ResultFileSystemWatcher close()
    {
        if (self == nullptr)
            return ResultFileSystemWatcher(FileSystemWatcherError::NotInitialized);
        if (eventLoopRunner)
        {
            SC_TRY(eventLoopRunner->linuxStopSharedFileReadiness());
        }

        if (threadingRunner)
        {
            if (threadingRunner->thread.wasStarted())
            {
                if (::pthread_equal(::pthread_self(), threadingRunner->thread.thread))
                    return ResultFileSystemWatcher(FileSystemWatcherError::CloseFailed,
                                                   FileSystemWatcherErrorDetail::PosixPthreadJoin, EDEADLK);
                threadingRunner->shouldStop.exchange(true);

                // Write to shutdownPipe to unblock the ::select() in the dedicated thread
                char dummy = 1;
                while (::write(threadingRunner->shutdownPipe[1], &dummy, sizeof(dummy)) == -1)
                {
                    if (errno != EINTR)
                    {
                        const uint32_t nativeError = static_cast<uint32_t>(errno);
                        return ResultFileSystemWatcher(FileSystemWatcherError::CloseFailed,
                                                       FileSystemWatcherErrorDetail::LinuxWriteShutdownPipe,
                                                       nativeError);
                    }
                }
                SC_TRY(threadingRunner->thread.join());
            }
        }
        // The thread must no longer be dispatching before the owner unlinks its folder watchers.
        while (FolderWatcher* entry = self->watchers.front)
        {
            SC_TRY(stopWatching(*entry));
        }

        if (threadingRunner)
        {
            threadingRunner->closeShutdownPipe();
        }
        if (notifyFd != -1)
        {
            ::close(notifyFd);
            notifyFd = -1;
        }
        self = nullptr;
        if (eventLoopRunner)
        {
            eventLoopRunner->notifyFd          = -1;
            eventLoopRunner->fileSystemWatcher = nullptr;
            eventLoopRunner                    = nullptr;
        }
        threadingRunner = nullptr;
        return ResultFileSystemWatcher(true);
    }

    // One inotify descriptor can be shared by multiple folder watchers on this owner.
    bool isWatchUsedByAnotherFolder(int watchId) const
    {
        for (const FolderWatcher* other = self->watchers.front; other != nullptr; other = other->next)
        {
            const FolderWatcherInternal& otherInternal = other->internal.get();
            for (size_t idx = 0; idx < otherInternal.notifyHandlesCount; ++idx)
            {
                if (otherInternal.notifyHandles[idx].nativeID() == watchId)
                    return true;
            }
        }
        return false;
    }

    ResultFileSystemWatcher stopWatching(FolderWatcher& folderWatcher)
    {
        folderWatcher.parent->watchers.remove(folderWatcher);
        folderWatcher.parent = nullptr;

        FolderWatcherInternal& folderInternal = folderWatcher.internal.get();

        if (notifyFd == -1)
        {
            return ResultFileSystemWatcher(FileSystemWatcherError::StopWatchingFailed,
                                           FileSystemWatcherErrorDetail::LinuxInotifyDescriptor);
        }

        ResultFileSystemWatcher result(true);
        for (size_t idx = 0; idx < folderInternal.notifyHandlesCount; ++idx)
        {
            const int watchId = folderInternal.notifyHandles[idx].nativeID();
            if (watchId == 0 or isWatchUsedByAnotherFolder(watchId))
                continue;

            int res;
            do
            {
                res = ::inotify_rm_watch(notifyFd, watchId);
            } while (res == -1 and errno == EINTR);
            if (res == -1 and errno != EINVAL and result)
            {
                result = ResultFileSystemWatcher(FileSystemWatcherError::StopWatchingFailed,
                                                 FileSystemWatcherErrorDetail::LinuxRemoveWatch,
                                                 static_cast<uint32_t>(errno));
            }
        }
        folderInternal.notifyHandlesCount   = 0; // Reset the count to zero
        folderInternal.relativePaths.length = 0;
        return result;
    }

    static ResultFileSystemWatcher getSubFolderPath(StringPath& path, const StringPath& entryPath, const char* name,
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
            if (not path.append("/") or not path.append(relativeDirectory))
                return ResultFileSystemWatcher(FileSystemWatcherError::PathPreparationFailed,
                                               FileSystemWatcherErrorDetail::BuildSubfolderPath);
        }
        if (not path.append("/") or not path.append(relativeName))
            return ResultFileSystemWatcher(FileSystemWatcherError::PathPreparationFailed,
                                           FileSystemWatcherErrorDetail::BuildSubfolderPath);
        return ResultFileSystemWatcher(true);
    }

    void detachUnstartedWatcher(FolderWatcher& entry)
    {
        self->watchers.remove(entry);
        entry.parent = nullptr;
    }

    ResultFileSystemWatcher failStartWatching(FolderWatcher& entry, ResultFileSystemWatcher failure)
    {
        const ResultFileSystemWatcher cleanup = stopWatching(entry);
        return cleanup ? failure : cleanup;
    }

    ResultFileSystemWatcher startWatching(FolderWatcher* entry)
    {
        // TODO: Add check for trying to watch folders already being watched or children of recursive watches

        if (monitoringFailed)
        {
            detachUnstartedWatcher(*entry);
            return ResultFileSystemWatcher(FileSystemWatcherError::MonitoringFailed);
        }
        if (entry->path.view().getEncoding() == StringEncoding::Utf16)
        {
            detachUnstartedWatcher(*entry);
            return ResultFileSystemWatcher(FileSystemWatcherError::UnsupportedPathEncoding);
        }
        FolderWatcherInternal& opaque = entry->internal.get();
        if (not entry->subFolderRelativePathsBuffer.empty())
        {
            opaque.relativePaths.writableSpan = entry->subFolderRelativePathsBuffer;
        }
        opaque.relativePaths.length = 0;

        StringPath currentPath = entry->path;

        if (notifyFd == -1)
        {
            detachUnstartedWatcher(*entry);
            return ResultFileSystemWatcher(FileSystemWatcherError::NotInitialized,
                                           FileSystemWatcherErrorDetail::LinuxInotifyDescriptor);
        }
        if (opaque.notifyHandlesCount >= FolderWatcherSizes::MaxNumberOfSubdirs)
        {
            detachUnstartedWatcher(*entry);
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchLimitExceeded,
                                           FileSystemWatcherErrorDetail::LinuxWatchHandleCapacity);
        }

        // Open the root before registering it, so an open failure cannot leave an inotify watch behind.
        const int rootFd = ::open(currentPath.view().bytesIncludingTerminator(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (rootFd == -1)
        {
            const uint32_t nativeError = static_cast<uint32_t>(errno);
            detachUnstartedWatcher(*entry);
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                           FileSystemWatcherErrorDetail::LinuxOpenRootDirectory, nativeError);
        }
        auto closeRoot = MakeDeferred([rootFd] { ::close(rootFd); });

        const int     rootNotifyFd = notifyFd;
        constexpr int mask =
            IN_ATTRIB | IN_CREATE | IN_MODIFY | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_MOVED_FROM | IN_MOVED_TO;
        const int newHandle = ::inotify_add_watch(rootNotifyFd, currentPath.view().bytesIncludingTerminator(), mask);
        if (newHandle == -1)
        {
            const uint32_t nativeError = static_cast<uint32_t>(errno);
            detachUnstartedWatcher(*entry);
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                           FileSystemWatcherErrorDetail::LinuxAddRootWatch, nativeError);
        }
        FolderWatcherInternal::Pair pair;
        pair.notifyID                                     = newHandle;
        pair.nameOffset                                   = -1;
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

        // fdopendir takes ownership of each descriptor popped from this stack.
        stack[stackSize++] = {rootFd, static_cast<int>(opaque.notifyHandlesCount - 1)};
        closeRoot.disarm();

        const size_t rootPathLength = entry->path.view().sizeInBytes();
        // Clean the stack of file descriptors in case any failure below is returned.
        auto deferredCleanStack = MakeDeferred(
            [&]
            {
                for (int idx = 0; idx < stackSize; ++idx)
                {
                    ::close(stack[idx].fd);
                }
            });
        while (stackSize > 0)
        {
            DirStackEntry entryStack = stack[--stackSize];

            DIR* dir = ::fdopendir(entryStack.fd);
            if (not dir)
            {
                const uint32_t nativeError = static_cast<uint32_t>(errno);
                ::close(entryStack.fd); // fdopendir did not take ownership
                return failStartWatching(*entry,
                                         ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                                                 FileSystemWatcherErrorDetail::LinuxOpenDirectoryStream,
                                                                 nativeError));
            }
            auto closeDir = MakeDeferred([dir] { ::closedir(dir); });

            for (;;)
            {
                errno                       = 0;
                struct dirent* subDirectory = ::readdir(dir);
                if (subDirectory == nullptr)
                {
                    const int nativeError = errno;
                    if (nativeError == EINTR)
                        continue;
                    if (nativeError != 0)
                        return failStartWatching(
                            *entry, ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                                            FileSystemWatcherErrorDetail::LinuxReadDirectoryEntries,
                                                            static_cast<uint32_t>(nativeError)));
                    break;
                }
                if (::strcmp(subDirectory->d_name, ".") == 0 or ::strcmp(subDirectory->d_name, "..") == 0)
                {
                    continue;
                }
                const ResultFileSystemWatcher pathResult =
                    getSubFolderPath(currentPath, entry->path, subDirectory->d_name, opaque, entryStack.notifyHandleId);
                if (not pathResult)
                    return failStartWatching(*entry, pathResult);

                struct stat st;
                if (::stat(currentPath.view().bytesIncludingTerminator(), &st) != 0)
                {
                    const int nativeError = errno;
                    // The entry or a symlink target may have disappeared since readdir.
                    if (nativeError == ENOENT)
                        continue;
                    return failStartWatching(
                        *entry, ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                                        FileSystemWatcherErrorDetail::LinuxInspectDirectoryEntry,
                                                        static_cast<uint32_t>(nativeError)));
                }
                if (S_ISDIR(st.st_mode))
                {
                    if (stackSize >= MaxStackDepth)
                        return failStartWatching(
                            *entry,
                            ResultFileSystemWatcher(FileSystemWatcherError::WatchLimitExceeded,
                                                    FileSystemWatcherErrorDetail::LinuxPendingDirectoryCapacity));
                    if (opaque.notifyHandlesCount >= FolderWatcherSizes::MaxNumberOfSubdirs)
                        return failStartWatching(
                            *entry, ResultFileSystemWatcher(FileSystemWatcherError::WatchLimitExceeded,
                                                            FileSystemWatcherErrorDetail::LinuxWatchHandleCapacity));

                    const char* relativePath = currentPath.view().bytesIncludingTerminator() + rootPathLength;
                    if (relativePath[0] == '/')
                        relativePath++;
                    pair.nameOffset = opaque.relativePaths.length == 0 ? 0 : opaque.relativePaths.length + 1;
                    const StringSpan relativePathSpan =
                        StringSpan::fromNullTerminated(relativePath, StringEncoding::Utf8);
                    if (not relativePathSpan.appendNullTerminatedTo(opaque.relativePaths, false))
                        return failStartWatching(
                            *entry, ResultFileSystemWatcher(FileSystemWatcherError::BufferTooSmall,
                                                            FileSystemWatcherErrorDetail::LinuxRelativePathStorage));

                    const int newHandle =
                        ::inotify_add_watch(rootNotifyFd, currentPath.view().bytesIncludingTerminator(), mask);
                    if (newHandle == -1)
                    {
                        const uint32_t nativeError = static_cast<uint32_t>(errno);
                        return failStartWatching(
                            *entry, ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                                            FileSystemWatcherErrorDetail::LinuxAddSubdirectoryWatch,
                                                            nativeError));
                    }
                    pair.notifyID = newHandle;

                    const int childHandleId = static_cast<int>(opaque.notifyHandlesCount);

                    opaque.notifyHandles[opaque.notifyHandlesCount++] = pair;

                    // Open the subdirectory and push onto the stack
                    const int subFd = ::open(currentPath.view().bytesIncludingTerminator(), O_RDONLY | O_DIRECTORY);
                    if (subFd == -1)
                    {
                        const uint32_t nativeError = static_cast<uint32_t>(errno);
                        return failStartWatching(
                            *entry,
                            ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                                    FileSystemWatcherErrorDetail::LinuxOpenSubdirectory, nativeError));
                    }
                    stack[stackSize++] = {subFd, childHandleId};
                }
            }
        }

        opaque.parentEntry = entry;

        // Launch the thread that monitors the inotify watch if we're on thread runner
        if (threadingRunner and not threadingRunner->thread.wasStarted())
        {
            threadingRunner->shouldStop.exchange(false);
            Function<void(FSWThread&)> threadFunction;
            threadFunction.bind<Internal, &Internal::threadRun>(*this);
            const ResultFileSystemWatcher started = threadingRunner->thread.start(move(threadFunction));
            if (not started)
                return failStartWatching(*entry, started);
        }
        return ResultFileSystemWatcher(true);
    }

    void threadRun(FSWThread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("FileSystemWatcher"));
        ThreadRunnerInternal& runner = *threadingRunner;
        while (not runner.shouldStop.load())
        {
            const int shutdownFd      = runner.shutdownPipe[0];
            const int currentNotifyFd = notifyFd;
            if (shutdownFd == -1 or currentNotifyFd == -1)
            {
                return;
            }

            // Setup a select fd_set to listen on both notifyFd and shutdownPipe simultaneously
            fd_set    fds;
            const int maxFd = currentNotifyFd > shutdownFd ? currentNotifyFd : shutdownFd;

            int selectRes;
            do
            {
                // select may alter the sets on interruption, so rebuild them for each retry.
                FD_ZERO(&fds);
                FD_SET(currentNotifyFd, &fds);
                FD_SET(shutdownFd, &fds);
                // Block until some events are received on the notifyFd or when shutdownPipe is written to
                selectRes = ::select(maxFd + 1, &fds, nullptr, nullptr, nullptr);
            } while (selectRes == -1 and errno == EINTR);

            if (selectRes == -1)
            {
                failMonitoring(ResultFileSystemWatcher(FileSystemWatcherError::MonitoringFailed,
                                                       FileSystemWatcherErrorDetail::LinuxWaitForEvents,
                                                       static_cast<uint32_t>(errno)));
                return;
            }
            if (FD_ISSET(shutdownFd, &fds))
            {
                return; // Interrupted by write to shutdown pipe (from close())
            }
            // Here select has received data on notifyHandle
            if (not readAndNotify(currentNotifyFd, self->watchers))
                return;
        }
        threadingRunner->shouldStop.exchange(false);
    }

    bool readAndNotify(const int notifyFd, WatcherLinkedList& watchers)
    {
        if (monitoringFailed)
            return false;
        int                                numReadBytes;
        alignas(struct inotify_event) char inotifyBuffer[3 * 1024];
        // TODO: Handle the case where kernel is sending more than 3kb of events.
        do
        {
            numReadBytes = ::read(notifyFd, inotifyBuffer, sizeof(inotifyBuffer));
        } while (numReadBytes == -1 and errno == EINTR);

        if (numReadBytes == -1 and (errno == EAGAIN or errno == EWOULDBLOCK))
            return true;
        if (numReadBytes <= 0)
        {
            const uint32_t nativeError = numReadBytes == -1 ? static_cast<uint32_t>(errno) : 0;
            failMonitoring(ResultFileSystemWatcher(FileSystemWatcherError::MonitoringFailed,
                                                   FileSystemWatcherErrorDetail::LinuxReadEvents, nativeError));
            return false;
        }
        Span<char> actuallyRead = {inotifyBuffer, static_cast<size_t>(numReadBytes)};
        notifyWatchers(actuallyRead, watchers);
        return true;
    }

    void failMonitoring(ResultFileSystemWatcher failure)
    {
        if (monitoringFailed)
            return;
        monitoringFailed = true;
        notifyRoots(self->watchers, NotificationKind::WatchStopped, failure);
    }

    template <typename Visit>
    static void forEachLiveFolder(WatcherLinkedList& watchers, Visit visit)
    {
        // Rescan the live list after each callback: it may stop and release another folder.
        // Address order visits each remaining folder at most once without auxiliary storage.
        uintptr_t previousAddress = 0;
        bool      havePrevious    = false;
        for (;;)
        {
            FolderWatcher* next        = nullptr;
            uintptr_t      nextAddress = 0;
            for (FolderWatcher* entry = watchers.front; entry != nullptr; entry = entry->next)
            {
                const uintptr_t address = reinterpret_cast<uintptr_t>(entry);
                if ((not havePrevious or address > previousAddress) and (next == nullptr or address < nextAddress))
                {
                    next        = entry;
                    nextAddress = address;
                }
            }
            if (next == nullptr)
                break;
            previousAddress = nextAddress;
            havePrevious    = true;
            visit(*next);
        }
    }

    static void notifyRoot(const FolderWatcher& entry, NotificationKind kind,
                           ResultFileSystemWatcher failure = ResultFileSystemWatcher(true))
    {
        Notification notification;
        notification.basePath  = entry.path.view();
        notification.operation = Operation::AddRemoveRename;
        notification.kind      = kind;
        notification.failure   = failure;
        entry.notifyCallback(notification);
    }

    static void notifyRoots(WatcherLinkedList& watchers, NotificationKind kind, ResultFileSystemWatcher failure)
    {
        forEachLiveFolder(watchers,
                          [&](const FolderWatcher& entry)
                          {
                              if (entry.internal.get().notifyHandles[0].notifyID > 0)
                                  notifyRoot(entry, kind, failure);
                          });
    }

    static void invalidateWatch(FolderWatcher& entry, const struct inotify_event& event)
    {
        auto&      folder     = entry.internal.get();
        auto&      root       = folder.notifyHandles[0];
        const bool rootActive = root.notifyID > 0;
        const bool rootLost   = rootActive and root.notifyID == event.wd;
        if (rootLost)
            root.suppress();

        bool subtreeLost = false;
        for (size_t index = 1; rootActive and not rootLost and index < folder.notifyHandlesCount; ++index)
        {
            auto& lost = folder.notifyHandles[index];
            if (lost.notifyID != event.wd)
                continue;

            subtreeLost = true;
            lost.suppress();
            const char*  directory = folder.relativePaths.writableSpan.data() + lost.nameOffset;
            const size_t length    = ::strlen(directory);
            for (size_t child = 1; child < folder.notifyHandlesCount; ++child)
            {
                auto&       candidate = folder.notifyHandles[child];
                const char* path      = folder.relativePaths.writableSpan.data() + candidate.nameOffset;
                if (::strncmp(path, directory, length) == 0 and path[length] == '/')
                    candidate.suppress();
            }
        }

        if (event.mask & IN_IGNORED)
        {
            for (size_t index = 0; index < folder.notifyHandlesCount; ++index)
            {
                if (folder.notifyHandles[index].nativeID() == event.wd)
                    folder.notifyHandles[index].notifyID = 0;
            }
        }

        if (rootLost)
            notifyRoot(entry, NotificationKind::WatchStopped,
                       {FileSystemWatcherError::MonitoringFailed, FileSystemWatcherErrorDetail::LinuxWatchInvalidated});
        else if (subtreeLost)
            notifyRoot(entry, NotificationKind::RescanRequired);
    }

    static void notifyWatchers(Span<char> actuallyRead, WatcherLinkedList& watchers)
    {
        const struct inotify_event* event     = nullptr;
        const struct inotify_event* prevEvent = nullptr;

        // Loop through all inotify_event and find the associated FolderWatcher to notify
        for (const char* iterator = actuallyRead.data();                  //
             iterator < actuallyRead.data() + actuallyRead.sizeInBytes(); //
             iterator += sizeof(*event) + event->len)
        {
            event = reinterpret_cast<const struct inotify_event*>(iterator);

            if (event->mask & IN_Q_OVERFLOW)
            {
                notifyRoots(watchers, NotificationKind::RescanRequired, ResultFileSystemWatcher(true));
                prevEvent = nullptr;
                continue;
            }
            if (event->mask & (IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT | IN_IGNORED))
            {
                forEachLiveFolder(watchers, [&](FolderWatcher& entry) { invalidateWatch(entry, *event); });
                prevEvent = nullptr;
                continue;
            }

            // Every folder must compare against the same preceding kernel event.
            const struct inotify_event* previousEvent = prevEvent;
            for (const FolderWatcher* entry = watchers.front; entry != nullptr; entry = entry->next)
            {
                const auto& folder = entry->internal.get();
                if (folder.notifyHandles[0].notifyID <= 0)
                    continue;
                for (size_t idx = 0; idx < folder.notifyHandlesCount; ++idx)
                {
                    if (folder.notifyHandles[idx].notifyID == event->wd)
                    {
                        (void)notifySingleEvent(event, previousEvent, entry, idx);
                        prevEvent = event;
                        break;
                    }
                }
            }
        }
    }

    [[nodiscard]] static ResultFileSystemWatcher notifySingleEvent(const struct inotify_event* event,
                                                                   const struct inotify_event* prevEvent,
                                                                   const FolderWatcher* entry, size_t foundIndex)
    {
        StringPath   eventPath;
        Notification notification;

        notification.basePath = entry->path.view();
        const char* eventName = event->len == 0 ? "" : event->name;

        // 1. Compute relative Path
        if (foundIndex == 0)
        {
            // Something changed in the original root folder being watched
            notification.relativePath = StringSpan::fromNullTerminated(eventName, StringEncoding::Utf8);
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
            const StringSpan relativeName      = StringSpan::fromNullTerminated(eventName, StringEncoding::Utf8);

            if (not eventPath.assign(relativeDirectory) or not eventPath.append("/") or
                not eventPath.append(relativeName))
                return ResultFileSystemWatcher(FileSystemWatcherError::PathPreparationFailed,
                                               FileSystemWatcherErrorDetail::BuildNotificationPath);

            notification.relativePath = eventPath.view();
        }

        // 2. Compute event Type
        if (event->mask & (IN_ATTRIB | IN_MODIFY))
        {
            // Try to coalesce Modified after AddRemoveRename for consistency with the other backends
            // I'm not really sure that Modified is consistently pushed after AddRemoveRename from Linux Kernel.
            if (prevEvent != nullptr and (prevEvent->wd == event->wd))
            {
                return ResultFileSystemWatcher(true);
            }
            notification.operation = Operation::Modified;
        }
        if (event->mask & ~(IN_ATTRIB | IN_MODIFY))
        {
            notification.operation = Operation::AddRemoveRename;
        }

        // 3. Finally invoke user callback with the notification
        entry->notifyCallback(notification);
        return ResultFileSystemWatcher(true);
    }
};

SC::ResultFileSystemWatcher SC::FileSystemWatcher::Notification::getFullPath(StringPath& buffer) const
{
    if (not buffer.assign(basePath) or not buffer.append("/") or not buffer.append(relativePath))
        return ResultFileSystemWatcher(FileSystemWatcherError::PathPreparationFailed,
                                       FileSystemWatcherErrorDetail::BuildFullPath);
    return ResultFileSystemWatcher(true);
}

bool SC::FileSystemWatcher::asyncNotify(FolderWatcher*, size_t)
{
    return internal.get().readAndNotify(internal.get().notifyFd, internal.get().self->watchers);
}

void SC::FileSystemWatcher::asyncNotifyFailure(Result failure)
{
    internal.get().failMonitoring(ResultFileSystemWatcher(failure));
}
