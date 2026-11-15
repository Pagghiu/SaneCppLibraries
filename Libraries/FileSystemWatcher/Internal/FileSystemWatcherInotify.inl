// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../FileSystemWatcher/FileSystemWatcher.h"
#include "FileSystemWatcherThreading.h"

#include "../../Common/Deferred.h"

#include <dirent.h>      // opendir, readdir, closedir
#include <errno.h>       // errno
#include <fcntl.h>       // open, O_RDONLY, O_DIRECTORY
#include <poll.h>        // poll
#include <stdio.h>       // snprintf
#include <string.h>      // strlen
#include <sys/inotify.h> // inotify
#include <sys/stat.h>    // fstat
#include <unistd.h>      // read

#if SC_PLATFORM_FREEBSD
#include <sys/event.h> // kevent and EVFILT_USER
#endif

struct SC::FileSystemWatcher::FolderWatcherInternal
{
    struct Pair
    {
        int32_t  notifyID   = 0;
        int32_t  nameOffset = 0;
        uint64_t device     = 0;
        uint64_t inode      = 0;

        bool matches(const struct stat& status) const
        {
            return S_ISDIR(status.st_mode) and device == static_cast<uint64_t>(status.st_dev) and
                   inode == static_cast<uint64_t>(status.st_ino);
        }

        // Negative IDs retain cleanup ownership after their paths become invalid.
        int32_t nativeID() const { return notifyID < 0 ? -notifyID : notifyID; }
        void    suppress() { notifyID = -nativeID(); }
    };

#if SC_PLATFORM_FREEBSD
    Pair       defaultHandles[FolderWatcherSizes::MaxNumberOfSubdirs];
    Span<Pair> notifyHandles{defaultHandles};
    Span<char> entryStorage;
    bool       limitedPending = false;
#else
    Pair notifyHandles[FolderWatcherSizes::MaxNumberOfSubdirs];
#endif
    size_t   notifyHandlesCount = 0;
    uint64_t registration       = 0;

    char relativePathsStorage[4096];

    StringSpan::NativeWritable relativePaths;

    size_t capacity() const
    {
#if SC_PLATFORM_FREEBSD
        return notifyHandles.sizeInElements();
#else
        return FolderWatcherSizes::MaxNumberOfSubdirs;
#endif
    }

#if SC_PLATFORM_FREEBSD
    ResultFileSystemWatcher prepareStorage()
    {
        static_assert(sizeof(Pair) == FileSystemWatcher::FolderWatcher::entryStorageSize(1) - alignof(uint64_t) + 1,
                      "FolderWatcher::entryStorageSize must match inotify directory storage");
        limitedPending = false;
        notifyHandles  = defaultHandles;
        if (entryStorage.empty())
            return ResultFileSystemWatcher(true);

        const uintptr_t entries  = reinterpret_cast<uintptr_t>(entryStorage.data());
        const uintptr_t paths    = reinterpret_cast<uintptr_t>(relativePaths.writableSpan.data());
        const bool      overlaps = entries >= paths ? entries - paths < relativePaths.writableSpan.sizeInBytes()
                                                    : paths - entries < entryStorage.sizeInBytes();
        const size_t    padding  = (alignof(Pair) - entries % alignof(Pair)) % alignof(Pair);
        if (entries == 0 or paths == 0 or overlaps or padding >= entryStorage.sizeInBytes())
            return {FileSystemWatcherError::BufferTooSmall, FileSystemWatcherErrorDetail::BSDEntryStorage};

        const size_t count = (entryStorage.sizeInBytes() - padding) / sizeof(Pair);
        if (count == 0)
            return {FileSystemWatcherError::BufferTooSmall, FileSystemWatcherErrorDetail::BSDEntryStorage};
        Pair* handles = reinterpret_cast<Pair*>(entryStorage.data() + padding);
        placementNewArray(handles, count);
        notifyHandles = {handles, count};
        return ResultFileSystemWatcher(true);
    }
#endif

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

    struct Delivery
    {
        Delivery* previous;
        uint64_t  latestRegistration;
        bool      alive = true;
    };

    FSWMutex       mutex;
    FSWCondition   callbackFinished;
    FolderWatcher* activeThreadCallback = nullptr;
    pthread_t      callbackThread{};
    Delivery*      delivery = nullptr;
    bool           closing  = false;

    uint64_t latestRegistration = 0;

    int  notifyFd         = -1; // inotify file descriptor
    bool monitoringFailed = false;

#if SC_PLATFORM_FREEBSD
    int readinessQueue = -1;

    ResultFileSystemWatcher wakeForCoverage()
    {
        struct kevent event;
        EV_SET(&event, 0, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);
        if (changeQueue(&event, 1, nullptr, 0) == -1)
            return {FileSystemWatcherError::MonitoringFailed, FileSystemWatcherErrorDetail::BSDTriggerControlEvent,
                    static_cast<uint32_t>(errno)};
        return ResultFileSystemWatcher(true);
    }

    int changeQueue(const struct kevent* changes, int changeCount, struct kevent* events, int capacity)
    {
        const struct timespec noWait = {0, 0};
        int                   result;
        do
            result = ::kevent(readinessQueue, changes, changeCount, events, capacity, &noWait);
        while (result == -1 and errno == EINTR);
        return result;
    }
#endif

    int readinessDescriptor() const
    {
#if SC_PLATFORM_FREEBSD
        return readinessQueue;
#else
        return notifyFd;
#endif
    }

    void closeDescriptors()
    {
#if SC_PLATFORM_FREEBSD
        if (readinessQueue != -1)
        {
            ::close(readinessQueue);
            readinessQueue = -1;
        }
#endif
        if (notifyFd != -1)
        {
            ::close(notifyFd);
            notifyFd = -1;
        }
    }

    ResultFileSystemWatcher initDescriptors(int flags)
    {
#if SC_PLATFORM_FREEBSD
        // A control wakeup can arrive without an inotify record.
        flags |= IN_NONBLOCK;
#endif
        notifyFd = ::inotify_init1(flags);
        if (notifyFd == -1)
            return {FileSystemWatcherError::InitializationFailed, FileSystemWatcherErrorDetail::LinuxInitializeInotify,
                    static_cast<uint32_t>(errno)};
#if SC_PLATFORM_FREEBSD
        auto rollback  = MakeDeferred([this] { closeDescriptors(); });
        readinessQueue = ::kqueuex(KQUEUE_CLOEXEC);
        if (readinessQueue == -1)
            return {FileSystemWatcherError::InitializationFailed, FileSystemWatcherErrorDetail::BSDCreateQueue,
                    static_cast<uint32_t>(errno)};
        struct kevent changes[2];
        EV_SET(&changes[0], notifyFd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
        EV_SET(&changes[1], 0, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, nullptr);
        if (changeQueue(changes, 2, nullptr, 0) == -1)
            return {FileSystemWatcherError::InitializationFailed, FileSystemWatcherErrorDetail::BSDRegisterControlEvent,
                    static_cast<uint32_t>(errno)};
        rollback.disarm();
#endif
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher init(FileSystemWatcher& parent, ThreadRunner& runner)
    {
        if (self != nullptr or runner.get().shutdownPipe[0] != -1)
            return {FileSystemWatcherError::InitializationFailed};
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
        const auto initialized = initDescriptors(IN_CLOEXEC);
        if (not initialized)
        {
            threadingRunner->closeShutdownPipe();
            self            = nullptr;
            threadingRunner = nullptr;
            return initialized;
        }
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher init(FileSystemWatcher& parent, EventLoopRunner& runner)
    {
        if (self != nullptr or runner.fileSystemWatcher != nullptr)
            return {FileSystemWatcherError::InitializationFailed};
        self             = &parent;
        eventLoopRunner  = &runner;
        monitoringFailed = false;

        const auto initialized = initDescriptors(IN_NONBLOCK | IN_CLOEXEC);
        if (not initialized)
        {
            self            = nullptr;
            eventLoopRunner = nullptr;
            return initialized;
        }

        eventLoopRunner->internalInit(parent, readinessDescriptor());
#if SC_PLATFORM_FREEBSD
        const Result started = eventLoopRunner->bsdStartSharedFileReadiness();
#else
        const Result started = eventLoopRunner->linuxStartSharedFileReadiness();
#endif
        if (not started)
        {
            closeDescriptors();

            eventLoopRunner->notifyFd          = -1;
            eventLoopRunner->fileSystemWatcher = nullptr;

            eventLoopRunner = nullptr;
            self            = nullptr;
        }
        return started;
    }

    ResultFileSystemWatcher close()
    {
        mutex.lock();
        if (self == nullptr or closing)
        {
            mutex.unlock();
            return {FileSystemWatcherError::NotInitialized};
        }
        if (threadingRunner and threadingRunner->thread.wasStarted() and
            ::pthread_equal(::pthread_self(), threadingRunner->thread.thread))
        {
            mutex.unlock();
            return {FileSystemWatcherError::CloseFailed, FileSystemWatcherErrorDetail::PosixPthreadJoin, EDEADLK};
        }
        closing = true;
        if (eventLoopRunner)
        {
            for (Delivery* active = delivery; active != nullptr; active = active->previous)
                active->alive = false;
            delivery = nullptr;
        }
        mutex.unlock();

        const auto closed = closeRunner();
        mutex.lock();
        auto unlock = MakeDeferred([this] { mutex.unlock(); });
        closing     = false;
        if (not closed)
        {
            monitoringFailed = true;
            return closed;
        }
        return closeWatches();
    }

    ResultFileSystemWatcher closeRunner()
    {
        if (eventLoopRunner)
        {
#if SC_PLATFORM_FREEBSD
            SC_TRY(eventLoopRunner->bsdStopSharedFileReadiness());
#else
            SC_TRY(eventLoopRunner->linuxStopSharedFileReadiness());
#endif
        }

        if (threadingRunner)
        {
            if (threadingRunner->thread.wasStarted())
            {
                threadingRunner->shouldStop.exchange(true);

                // Write to shutdownPipe to wake the watcher thread.
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
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher closeWatches()
    {
        ResultFileSystemWatcher result(true);
        while (FolderWatcher* entry = self->watchers.front)
        {
            const auto stopped = stopWatchingLocked(*entry);
            if (not stopped and result)
                result = stopped;
        }

        if (threadingRunner)
        {
            threadingRunner->closeShutdownPipe();
        }
        closeDescriptors();
        self = nullptr;
        if (eventLoopRunner)
        {
            eventLoopRunner->notifyFd          = -1;
            eventLoopRunner->fileSystemWatcher = nullptr;
            eventLoopRunner                    = nullptr;
        }
        threadingRunner = nullptr;
        return result;
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

    ResultFileSystemWatcher removeDirectoryWatch(int watchId)
    {
        if (watchId == 0 or isWatchUsedByAnotherFolder(watchId))
            return ResultFileSystemWatcher(true);

        int removed;
        do
            removed = ::inotify_rm_watch(notifyFd, watchId);
        while (removed == -1 and errno == EINTR);
        if (removed == -1 and errno != EINVAL)
            return {FileSystemWatcherError::StopWatchingFailed, FileSystemWatcherErrorDetail::LinuxRemoveWatch,
                    static_cast<uint32_t>(errno)};
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher stopWatching(FolderWatcher& folderWatcher)
    {
        mutex.lock();
        auto unlock = MakeDeferred([this] { mutex.unlock(); });
        while (activeThreadCallback == &folderWatcher and not ::pthread_equal(::pthread_self(), callbackThread))
            callbackFinished.wait(mutex);
        if (self == nullptr or folderWatcher.parent != self)
            return {FileSystemWatcherError::NotWatching};
        return stopWatchingLocked(folderWatcher);
    }

    ResultFileSystemWatcher stopWatchingLocked(FolderWatcher& folderWatcher)
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
            const auto removed = removeDirectoryWatch(folderInternal.notifyHandles[idx].nativeID());
            if (not removed and result)
                result = removed;
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

    int registerDirectory(int directoryFd)
    {
        constexpr int mask = IN_ONLYDIR | IN_ATTRIB | IN_CREATE | IN_MODIFY | IN_DELETE | IN_DELETE_SELF |
                             IN_MOVE_SELF | IN_MOVED_FROM | IN_MOVED_TO;
#if SC_PLATFORM_FREEBSD
        return ::inotify_add_watch_at(notifyFd, directoryFd, ".", mask);
#else
        // Resolve the open descriptor through procfs so registration and enumeration use the same object.
        char descriptorPath[sizeof("/proc/self/fd/") + 3 * sizeof(int)];
        ::snprintf(descriptorPath, sizeof(descriptorPath), "/proc/self/fd/%d", directoryFd);
        return ::inotify_add_watch(notifyFd, descriptorPath, mask);
#endif
    }

    static bool sameDirectory(const struct stat& first, const struct stat& second)
    {
        return S_ISDIR(second.st_mode) and first.st_dev == second.st_dev and first.st_ino == second.st_ino;
    }

    static ResultFileSystemWatcher verifyDirectory(int directoryFd, int parentFd, const char* name, int flags)
    {
        struct stat opened, named;
        if (::fstat(directoryFd, &opened) == -1 or ::fstatat(parentFd, name, &named, flags) == -1)
            return {FileSystemWatcherError::WatchSetupFailed,
                    FileSystemWatcherErrorDetail::LinuxVerifyDirectoryIdentity, static_cast<uint32_t>(errno)};
        if (not sameDirectory(opened, named))
            return {FileSystemWatcherError::WatchSetupFailed,
                    FileSystemWatcherErrorDetail::LinuxVerifyDirectoryIdentity, ESTALE};
        return ResultFileSystemWatcher(true);
    }

    void detachUnstartedWatcher(FolderWatcher& entry)
    {
        self->watchers.remove(entry);
        entry.parent = nullptr;
    }

    ResultFileSystemWatcher failStartWatching(FolderWatcher& entry, ResultFileSystemWatcher failure)
    {
        const ResultFileSystemWatcher cleanup = stopWatchingLocked(entry);
        return cleanup ? failure : cleanup;
    }

    ResultFileSystemWatcher addSubdirectory(FolderWatcherInternal& folder, int fd, const struct stat& status,
                                            const char* path)
    {
        if (folder.notifyHandlesCount >= folder.capacity())
            return {FileSystemWatcherError::WatchLimitExceeded, FileSystemWatcherErrorDetail::LinuxWatchHandleCapacity};
        if (path[0] == '/')
            ++path;

        FolderWatcherInternal::Pair pair;
        pair.nameOffset         = folder.relativePaths.length == 0 ? 0 : folder.relativePaths.length + 1;
        const auto oldLength    = folder.relativePaths.length;
        const auto relativePath = StringSpan::fromNullTerminated(path, StringEncoding::Utf8);
        if (not relativePath.appendNullTerminatedTo(folder.relativePaths, false))
            return {FileSystemWatcherError::BufferTooSmall, FileSystemWatcherErrorDetail::LinuxRelativePathStorage};

        pair.notifyID = registerDirectory(fd);
        if (pair.notifyID == -1)
        {
            folder.relativePaths.length = oldLength;
            return {FileSystemWatcherError::WatchSetupFailed, FileSystemWatcherErrorDetail::LinuxAddSubdirectoryWatch,
                    static_cast<uint32_t>(errno)};
        }
        pair.device                                       = static_cast<uint64_t>(status.st_dev);
        pair.inode                                        = static_cast<uint64_t>(status.st_ino);
        folder.notifyHandles[folder.notifyHandlesCount++] = pair;
        return ResultFileSystemWatcher(true);
    }

    // Takes ownership of the open directory descriptor, including on failure.
    ResultFileSystemWatcher scanSubdirectories(FolderWatcher& entry, int directoryFd, int directoryIndex)
    {
        struct Directory
        {
            DIR* stream;
            int  watchIndex;
        };
#if SC_PLATFORM_FREEBSD
        // Each additional directory needs at least a separator and one path byte.
        Directory pending[1 + StringPath::MaxPath / 2];
#else
        Directory pending[FolderWatcherSizes::MaxNumberOfSubdirs];
#endif
        size_t depth        = 0;
        auto   closeStreams = MakeDeferred(
            [&]
            {
                while (depth != 0)
                    ::closedir(pending[--depth].stream);
            });
        const auto push = [&](int fd, int watchIndex) -> ResultFileSystemWatcher
        {
            if (depth == sizeof(pending) / sizeof(pending[0]))
            {
                ::close(fd);
                return {FileSystemWatcherError::WatchLimitExceeded,
                        FileSystemWatcherErrorDetail::LinuxPendingDirectoryCapacity};
            }
            DIR* stream = ::fdopendir(fd);
            if (stream == nullptr)
            {
                const uint32_t nativeError = static_cast<uint32_t>(errno);
                ::close(fd);
                return {FileSystemWatcherError::WatchSetupFailed,
                        FileSystemWatcherErrorDetail::LinuxOpenDirectoryStream, nativeError};
            }
            pending[depth++] = {stream, watchIndex};
            return ResultFileSystemWatcher(true);
        };
        const auto rootPushed = push(directoryFd, directoryIndex);
        if (not rootPushed)
            return rootPushed;

        auto&      folder = entry.internal.get();
        StringPath currentPath;
        while (depth != 0)
        {
            const Directory& directory = pending[depth - 1];
            errno                      = 0;
            const dirent* item         = ::readdir(directory.stream);
            if (item == nullptr)
            {
                const int nativeError = errno;
                if (nativeError == EINTR)
                    continue;
                if (nativeError != 0)
                    return {FileSystemWatcherError::WatchSetupFailed,
                            FileSystemWatcherErrorDetail::LinuxReadDirectoryEntries,
                            static_cast<uint32_t>(nativeError)};
                ::closedir(pending[--depth].stream);
                continue;
            }

            const char* name = item->d_name;
            if (::strcmp(name, ".") == 0 or ::strcmp(name, "..") == 0)
                continue;
            const int   parentFd = ::dirfd(directory.stream);
            struct stat status;
            int         inspected;
            do
                inspected = ::fstatat(parentFd, name, &status, AT_SYMLINK_NOFOLLOW);
            while (inspected == -1 and errno == EINTR);
            if (inspected == -1 and errno != ENOENT)
                return {FileSystemWatcherError::WatchSetupFailed,
                        FileSystemWatcherErrorDetail::LinuxInspectDirectoryEntry, static_cast<uint32_t>(errno)};
            if (inspected == -1)
                continue;
            if (not S_ISDIR(status.st_mode))
            {
#if SC_PLATFORM_FREEBSD
                folder.limitedPending |= not S_ISREG(status.st_mode);
#endif
                continue;
            }

            int childFd;
            do
                childFd = ::openat(parentFd, name, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
            while (childFd == -1 and errno == EINTR);
            if (childFd == -1)
            {
                const int nativeError = errno;
                if (nativeError == ENOENT)
                    continue;
                return {FileSystemWatcherError::WatchSetupFailed, FileSystemWatcherErrorDetail::LinuxOpenSubdirectory,
                        static_cast<uint32_t>(nativeError)};
            }
            auto        closeChild = MakeDeferred([childFd] { ::close(childFd); });
            struct stat opened;
            if (::fstat(childFd, &opened) == -1)
                return {FileSystemWatcherError::WatchSetupFailed,
                        FileSystemWatcherErrorDetail::LinuxVerifyDirectoryIdentity, static_cast<uint32_t>(errno)};
            if (not sameDirectory(status, opened))
                return {FileSystemWatcherError::WatchSetupFailed,
                        FileSystemWatcherErrorDetail::LinuxVerifyDirectoryIdentity, ESTALE};

            const auto pathReady = getSubFolderPath(currentPath, entry.path, name, folder, directory.watchIndex);
            if (not pathReady)
                return pathReady;
            const int   childIndex = static_cast<int>(folder.notifyHandlesCount);
            const char* relative   = currentPath.view().bytesIncludingTerminator() + entry.path.view().sizeInBytes();
            const auto  added      = addSubdirectory(folder, childFd, opened, relative);
            if (not added)
                return added;
            const auto verified = verifyDirectory(childFd, parentFd, name, AT_SYMLINK_NOFOLLOW);
            if (not verified)
                return verified;
            closeChild.disarm();
            const auto pushed = push(childFd, childIndex);
            if (not pushed)
                return pushed;
        }
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher watch(FolderWatcher& entry, StringSpan path)
    {
        mutex.lock();
        auto unlock = MakeDeferred([this] { mutex.unlock(); });
        if (self == nullptr or closing)
            return {FileSystemWatcherError::NotInitialized};
        if (entry.parent != nullptr)
            return {FileSystemWatcherError::AlreadyWatching};
        if (not entry.path.assign(path))
            return {FileSystemWatcherError::PathPreparationFailed, FileSystemWatcherErrorDetail::AssignWatchPath};
        if (latestRegistration == static_cast<uint64_t>(-1))
            return {FileSystemWatcherError::WatchLimitExceeded};
        entry.parent = self;
        self->watchers.queueBack(entry);
        entry.internal.get().registration = ++latestRegistration;
        return startWatching(&entry);
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
#if SC_PLATFORM_FREEBSD
        const auto storage = opaque.prepareStorage();
        if (not storage)
        {
            detachUnstartedWatcher(*entry);
            return storage;
        }
#endif

        StringPath currentPath = entry->path;

        if (notifyFd == -1)
        {
            detachUnstartedWatcher(*entry);
            return ResultFileSystemWatcher(FileSystemWatcherError::NotInitialized,
                                           FileSystemWatcherErrorDetail::LinuxInotifyDescriptor);
        }
        if (opaque.notifyHandlesCount >= opaque.capacity())
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

        struct stat rootStatus;
        if (::fstat(rootFd, &rootStatus) == -1)
        {
            const uint32_t nativeError = static_cast<uint32_t>(errno);
            detachUnstartedWatcher(*entry);
            return {FileSystemWatcherError::WatchSetupFailed,
                    FileSystemWatcherErrorDetail::LinuxVerifyDirectoryIdentity, nativeError};
        }

        const int newHandle = registerDirectory(rootFd);
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
        pair.device                                       = static_cast<uint64_t>(rootStatus.st_dev);
        pair.inode                                        = static_cast<uint64_t>(rootStatus.st_ino);
        opaque.notifyHandles[opaque.notifyHandlesCount++] = pair;

        const auto verifiedRoot = verifyDirectory(rootFd, AT_FDCWD, currentPath.view().bytesIncludingTerminator(), 0);
        if (not verifiedRoot)
            return failStartWatching(*entry, verifiedRoot);

        closeRoot.disarm();
        const auto scanned = scanSubdirectories(*entry, rootFd, 0);
        if (not scanned)
            return failStartWatching(*entry, scanned);
#if SC_PLATFORM_FREEBSD
        if (opaque.limitedPending)
        {
            const auto woken = wakeForCoverage();
            if (not woken)
                return failStartWatching(*entry, woken);
        }
#endif

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
            const int currentNotifyFd = readinessDescriptor();
            if (shutdownFd == -1 or currentNotifyFd == -1)
            {
                return;
            }

            pollfd descriptors[] = {{shutdownFd, POLLIN, 0}, {currentNotifyFd, POLLIN, 0}};
            int    ready;
            do
                ready = ::poll(descriptors, sizeof(descriptors) / sizeof(descriptors[0]), -1);
            while (ready == -1 and errno == EINTR);

            const int status = descriptors[0].revents | descriptors[1].revents;
            if (ready == -1 or (status & (POLLERR | POLLHUP | POLLNVAL)))
            {
                const int nativeError = ready == -1 ? errno : (status & POLLNVAL) ? EBADF : EIO;
                failMonitoring({FileSystemWatcherError::MonitoringFailed,
                                FileSystemWatcherErrorDetail::LinuxWaitForEvents, static_cast<uint32_t>(nativeError)});
                return;
            }
            if (descriptors[0].revents & POLLIN)
                return;
            if ((descriptors[1].revents & POLLIN) and not readAndNotify())
                return;
        }
        threadingRunner->shouldStop.exchange(false);
    }

    bool readAndNotify()
    {
#if SC_PLATFORM_FREEBSD
        struct kevent events[2];
        if (changeQueue(nullptr, 0, events, 2) == -1)
        {
            failMonitoring({FileSystemWatcherError::MonitoringFailed, FileSystemWatcherErrorDetail::BSDReadEvents,
                            static_cast<uint32_t>(errno)});
            return false;
        }
#endif
        int                                numReadBytes;
        alignas(struct inotify_event) char inotifyBuffer[3 * 1024];
        // TODO: Handle the case where kernel is sending more than 3kb of events.
        do
        {
            numReadBytes = ::read(notifyFd, inotifyBuffer, sizeof(inotifyBuffer));
        } while (numReadBytes == -1 and errno == EINTR);

        const bool wouldBlock = numReadBytes == -1 and (errno == EAGAIN or errno == EWOULDBLOCK);
        if (not wouldBlock and numReadBytes <= 0)
        {
            const uint32_t nativeError = numReadBytes == -1 ? static_cast<uint32_t>(errno) : 0;
            failMonitoring(ResultFileSystemWatcher(FileSystemWatcherError::MonitoringFailed,
                                                   FileSystemWatcherErrorDetail::LinuxReadEvents, nativeError));
            return false;
        }
        return dispatch(
            [&](Delivery& frame)
            {
#if SC_PLATFORM_FREEBSD
                notifyLimitedCoverage(frame);
#endif
                if (frame.alive and not closing and not wouldBlock)
                    notifyWatchers({inotifyBuffer, static_cast<size_t>(numReadBytes)}, self->watchers, frame);
            });
    }

    template <typename Visit>
    bool dispatch(Visit visit)
    {
        mutex.lock();
        if (self == nullptr or closing or monitoringFailed)
        {
            mutex.unlock();
            return false;
        }
        Delivery frame{delivery, latestRegistration};
        delivery = &frame;
        visit(frame);
        if (not frame.alive)
            return false;
        delivery           = frame.previous;
        const bool running = not closing and not monitoringFailed;
        mutex.unlock();
        return running;
    }

    void failMonitoring(ResultFileSystemWatcher failure)
    {
        dispatch(
            [&](Delivery& frame)
            {
                monitoringFailed = true;
                notifyRoots(self->watchers, NotificationKind::WatchStopped, failure, frame);
            });
    }

    template <typename Visit>
    void forEachLiveFolder(WatcherLinkedList& watchers, Delivery& frame, Visit visit)
    {
        // Callbacks can remove other folders or start a new registration at the same address.
        // Traverse the live list in registration order and exclude registrations made during this batch.
        uint64_t previous = 0;
        while (frame.alive and not closing)
        {
            FolderWatcher* entry = watchers.front;
            while (entry != nullptr and entry->internal.get().registration <= previous)
                entry = entry->next;
            if (entry == nullptr or entry->internal.get().registration > frame.latestRegistration)
                break;
            previous = entry->internal.get().registration;
            visit(*entry);
        }
    }

    void notify(FolderWatcher& entry, Notification& notification, Delivery& frame)
    {
        StringPath basePath   = entry.path;
        notification.basePath = basePath.view();
        if (threadingRunner)
        {
            activeThreadCallback = &entry;
            callbackThread       = ::pthread_self();
        }
        mutex.unlock();
        if (entry.notifyCallback.isValid())
            entry.notifyCallback(notification);
        if (not frame.alive)
            return;
        mutex.lock();
        if (threadingRunner)
        {
            activeThreadCallback = nullptr;
            callbackFinished.broadcast();
        }
    }

    void notifyRoot(FolderWatcher& entry, NotificationKind kind, Delivery& frame,
                    ResultFileSystemWatcher failure = ResultFileSystemWatcher(true))
    {
        Notification notification;
        notification.operation = Operation::AddRemoveRename;
        notification.kind      = kind;
        notification.failure   = failure;
        notify(entry, notification, frame);
    }

    void notifyRoots(WatcherLinkedList& watchers, NotificationKind kind, ResultFileSystemWatcher failure,
                     Delivery& frame)
    {
        forEachLiveFolder(watchers, frame,
                          [&](FolderWatcher& entry)
                          {
                              if (entry.internal.get().notifyHandles[0].notifyID > 0)
                                  notifyRoot(entry, kind, frame, failure);
                          });
    }

#if SC_PLATFORM_FREEBSD
    void notifyLimitedCoverage(Delivery& frame)
    {
        forEachLiveFolder(self->watchers, frame,
                          [&](FolderWatcher& entry)
                          {
                              auto& folder = entry.internal.get();
                              if (folder.notifyHandles[0].notifyID > 0 and folder.limitedPending)
                              {
                                  folder.limitedPending = false;
                                  notifyRoot(entry, NotificationKind::CoverageLimited, frame,
                                             {FileSystemWatcherError::CoverageLimited,
                                              FileSystemWatcherErrorDetail::BSDPassiveEntries});
                              }
                          });
    }

    static bool isPassiveArrival(FolderWatcher& entry, size_t index, const struct inotify_event& event)
    {
        if (event.len == 0 or not(event.mask & (IN_CREATE | IN_MOVED_TO)))
            return false;
        StringPath path;
        if (not getSubFolderPath(path, entry.path, event.name, entry.internal.get(), static_cast<int>(index)))
            return false;
        struct stat status;
        return ::lstat(path.view().bytesIncludingTerminator(), &status) == 0 and not S_ISDIR(status.st_mode) and
               not S_ISREG(status.st_mode);
    }
#endif

    ResultFileSystemWatcher updateDirectory(FolderWatcher& entry, size_t parentIndex, const inotify_event& event)
    {
        auto&      folder     = entry.internal.get();
        StringPath parentPath = entry.path;
        if (parentIndex != 0)
        {
            const char* relativeParent =
                folder.relativePaths.writableSpan.data() + folder.notifyHandles[parentIndex].nameOffset;
            if (not parentPath.append("/") or
                not parentPath.append(StringSpan::fromNullTerminated(relativeParent, StringEncoding::Utf8)))
                return {FileSystemWatcherError::PathPreparationFailed,
                        FileSystemWatcherErrorDetail::BuildSubfolderPath};
        }

        StringPath childPath;
        const auto pathReady = getSubFolderPath(childPath, entry.path, event.name, folder, parentIndex);
        if (not pathReady)
            return pathReady;
        const char*  relative = childPath.view().bytesIncludingTerminator() + entry.path.view().sizeInBytes() + 1;
        const size_t length   = ::strlen(relative);

        const int parentFd = ::open(parentPath.view().bytesIncludingTerminator(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (parentFd == -1)
        {
            if (errno == ENOENT or errno == ENOTDIR)
                return ResultFileSystemWatcher(true); // A queued event for an already departed parent.
            return {FileSystemWatcherError::WatchSetupFailed, FileSystemWatcherErrorDetail::LinuxOpenSubdirectory,
                    static_cast<uint32_t>(errno)};
        }
        auto        closeParent = MakeDeferred([parentFd] { ::close(parentFd); });
        struct stat parentStatus;
        if (::fstat(parentFd, &parentStatus) == -1)
            return {FileSystemWatcherError::WatchSetupFailed,
                    FileSystemWatcherErrorDetail::LinuxVerifyDirectoryIdentity, static_cast<uint32_t>(errno)};
        if (not folder.notifyHandles[parentIndex].matches(parentStatus))
            return ResultFileSystemWatcher(true);

        struct stat childStatus;
        const bool  exists = ::fstatat(parentFd, event.name, &childStatus, AT_SYMLINK_NOFOLLOW) == 0;
        if (not exists and errno != ENOENT)
            return {FileSystemWatcherError::WatchSetupFailed, FileSystemWatcherErrorDetail::LinuxInspectDirectoryEntry,
                    static_cast<uint32_t>(errno)};

        // If a path already denotes the registered directory, a queued event for it needs no update.
        for (size_t index = 1; index < folder.notifyHandlesCount; ++index)
        {
            const auto& candidate = folder.notifyHandles[index];
            const char* path      = folder.relativePaths.writableSpan.data() + candidate.nameOffset;
            if (candidate.notifyID > 0 and ::strcmp(path, relative) == 0 and exists and candidate.matches(childStatus))
                return ResultFileSystemWatcher(true);
        }

        // Retire the old directory and all its descendants. Other roots can still own the same kernel watches.
        ResultFileSystemWatcher cleanup(true);
        char*                   paths = folder.relativePaths.writableSpan.data();
        for (size_t index = 1; index < folder.notifyHandlesCount; ++index)
        {
            auto&       candidate = folder.notifyHandles[index];
            const char* path      = paths + candidate.nameOffset;
            if (::strncmp(path, relative, length) == 0 and (path[length] == '\0' or path[length] == '/'))
            {
                const int watchId  = candidate.nativeID();
                candidate.notifyID = 0;
                const auto removed = removeDirectoryWatch(watchId);
                if (not removed)
                {
                    candidate.notifyID = -watchId;
                    if (cleanup)
                        cleanup = removed;
                }
            }
        }

        size_t count = 1;
        size_t bytes = 0;
        for (size_t index = 1; index < folder.notifyHandlesCount; ++index)
        {
            auto candidate = folder.notifyHandles[index];
            if (candidate.notifyID == 0)
                continue;
            const char*  path      = paths + candidate.nameOffset;
            const size_t pathBytes = ::strlen(path) + 1;
            ::memmove(paths + bytes, path, pathBytes);
            candidate.nameOffset          = static_cast<int32_t>(bytes);
            folder.notifyHandles[count++] = candidate;
            bytes += pathBytes;
        }
        folder.notifyHandlesCount   = count;
        folder.relativePaths.length = bytes == 0 ? 0 : bytes - 1;
        if (not cleanup)
            return cleanup;

        if (not exists or not S_ISDIR(childStatus.st_mode))
            return ResultFileSystemWatcher(true);

        // The current directory may already be a replacement by the time a departure event is read.
        int childFd;
        do
            childFd = ::openat(parentFd, event.name, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        while (childFd == -1 and errno == EINTR);
        if (childFd == -1)
        {
            if (errno == ENOENT)
                return ResultFileSystemWatcher(true);
            return {FileSystemWatcherError::WatchSetupFailed, FileSystemWatcherErrorDetail::LinuxOpenSubdirectory,
                    static_cast<uint32_t>(errno)};
        }
        auto        closeChild = MakeDeferred([childFd] { ::close(childFd); });
        struct stat opened;
        if (::fstat(childFd, &opened) == -1)
            return {FileSystemWatcherError::WatchSetupFailed,
                    FileSystemWatcherErrorDetail::LinuxVerifyDirectoryIdentity, static_cast<uint32_t>(errno)};
        if (not sameDirectory(childStatus, opened))
            return {FileSystemWatcherError::WatchSetupFailed,
                    FileSystemWatcherErrorDetail::LinuxVerifyDirectoryIdentity, ESTALE};
        const int  childIndex = static_cast<int>(folder.notifyHandlesCount);
        const auto added      = addSubdirectory(folder, childFd, opened, relative);
        if (not added)
            return added;
        const auto verified = verifyDirectory(childFd, parentFd, event.name, AT_SYMLINK_NOFOLLOW);
        if (not verified)
            return verified;
        closeChild.disarm();
        return scanSubdirectories(entry, childFd, childIndex);
    }

    void invalidateWatch(FolderWatcher& entry, const struct inotify_event& event, Delivery& frame)
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
            if (lost.notifyID != event.wd or (event.mask & IN_MOVE_SELF))
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
            notifyRoot(entry, NotificationKind::WatchStopped, frame,
                       {FileSystemWatcherError::MonitoringFailed, FileSystemWatcherErrorDetail::LinuxWatchInvalidated});
        else if (subtreeLost)
            notifyRoot(entry, NotificationKind::RescanRequired, frame);
    }

    void notifyWatchers(Span<char> actuallyRead, WatcherLinkedList& watchers, Delivery& frame)
    {
        const struct inotify_event* event     = nullptr;
        const struct inotify_event* prevEvent = nullptr;

        // Loop through all inotify_event and find the associated FolderWatcher to notify
        for (const char* iterator = actuallyRead.data();                                                  //
             frame.alive and not closing and iterator < actuallyRead.data() + actuallyRead.sizeInBytes(); //
             iterator += sizeof(*event) + event->len)
        {
            event = reinterpret_cast<const struct inotify_event*>(iterator);

            if (event->mask & IN_Q_OVERFLOW)
            {
                notifyRoots(watchers, NotificationKind::RescanRequired, ResultFileSystemWatcher(true), frame);
                prevEvent = nullptr;
                continue;
            }
            if (event->mask & (IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT | IN_IGNORED))
            {
                forEachLiveFolder(watchers, frame,
                                  [&](FolderWatcher& entry) { invalidateWatch(entry, *event, frame); });
                prevEvent = nullptr;
                continue;
            }
            if ((event->mask & IN_ISDIR) and (event->mask & (IN_CREATE | IN_MOVED_TO | IN_DELETE | IN_MOVED_FROM)))
            {
                forEachLiveFolder(watchers, frame,
                                  [&](FolderWatcher& entry)
                                  {
                                      auto& folder = entry.internal.get();
                                      if (folder.notifyHandles[0].notifyID <= 0)
                                          return;
                                      for (size_t idx = 0; idx < folder.notifyHandlesCount; ++idx)
                                      {
                                          if (folder.notifyHandles[idx].notifyID != event->wd)
                                              continue;
                                          auto updated = updateDirectory(entry, idx, *event);
#if SC_PLATFORM_FREEBSD
                                          if (updated and folder.limitedPending)
                                              updated = wakeForCoverage();
#endif
                                          if (updated)
                                          {
                                              // Mutations before registration must still be recovered by a rescan.
                                              notifyRoot(entry, NotificationKind::RescanRequired, frame);
                                          }
                                          else
                                          {
                                              folder.notifyHandles[0].suppress();
                                              notifyRoot(entry, NotificationKind::WatchStopped, frame, updated);
                                          }
                                          return;
                                      }
                                  });
                prevEvent = nullptr;
                continue;
            }

            // Every folder must compare against the same preceding kernel event.
            const struct inotify_event* previousEvent = prevEvent;
            forEachLiveFolder(watchers, frame,
                              [&](FolderWatcher& entry)
                              {
                                  const auto& folder = entry.internal.get();
                                  if (folder.notifyHandles[0].notifyID <= 0)
                                      return;
                                  for (size_t idx = 0; idx < folder.notifyHandlesCount; ++idx)
                                  {
                                      if (folder.notifyHandles[idx].notifyID == event->wd)
                                      {
#if SC_PLATFORM_FREEBSD
                                          if (isPassiveArrival(entry, idx, *event))
                                          {
                                              notifyRoot(entry, NotificationKind::CoverageLimited, frame,
                                                         {FileSystemWatcherError::CoverageLimited,
                                                          FileSystemWatcherErrorDetail::BSDPassiveEntries});
                                              return;
                                          }
#endif
                                          notifySingleEvent(event, previousEvent, entry, idx, frame);
                                          prevEvent = event;
                                          return;
                                      }
                                  }
                              });
        }
    }

    void notifySingleEvent(const struct inotify_event* event, const struct inotify_event* prevEvent,
                           FolderWatcher& entry, size_t foundIndex, Delivery& frame)
    {
        StringPath   eventPath;
        Notification notification;

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
            const FolderWatcherInternal& internal = entry.internal.get();

            const char* dirStart =
                internal.notifyHandles[foundIndex].nameOffset >= 0
                    ? internal.relativePaths.writableSpan.data() + internal.notifyHandles[foundIndex].nameOffset
                    : "";

            const StringSpan relativeDirectory = StringSpan::fromNullTerminated(dirStart, StringEncoding::Utf8);
            const StringSpan relativeName      = StringSpan::fromNullTerminated(eventName, StringEncoding::Utf8);

            if (not eventPath.assign(relativeDirectory) or not eventPath.append("/") or
                not eventPath.append(relativeName))
            {
                notifyRoot(entry, NotificationKind::RescanRequired, frame);
                return;
            }

            notification.relativePath = eventPath.view();
        }

        // 2. Compute event Type
        if (event->mask & (IN_ATTRIB | IN_MODIFY))
        {
            // Try to coalesce Modified after AddRemoveRename for consistency with the other backends
            // I'm not really sure that Modified is consistently pushed after AddRemoveRename from Linux Kernel.
            if (prevEvent != nullptr and (prevEvent->wd == event->wd))
            {
                return;
            }
            notification.operation = Operation::Modified;
        }
        if (event->mask & ~(IN_ATTRIB | IN_MODIFY))
        {
            notification.operation = Operation::AddRemoveRename;
        }

        // 3. Finally invoke user callback with the notification
        notify(entry, notification, frame);
    }
};

SC::ResultFileSystemWatcher SC::FileSystemWatcher::Notification::getFullPath(StringPath& buffer) const
{
    if (not buffer.assign(basePath) or not buffer.append("/") or not buffer.append(relativePath))
        return ResultFileSystemWatcher(FileSystemWatcherError::PathPreparationFailed,
                                       FileSystemWatcherErrorDetail::BuildFullPath);
    return ResultFileSystemWatcher(true);
}

bool SC::FileSystemWatcher::asyncNotify(FolderWatcher*, size_t) { return internal.get().readAndNotify(); }

void SC::FileSystemWatcher::asyncNotifyFailure(Result failure)
{
    internal.get().failMonitoring(ResultFileSystemWatcher(failure));
}
