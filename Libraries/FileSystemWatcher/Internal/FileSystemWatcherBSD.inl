// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../FileSystemWatcher/FileSystemWatcher.h"
#include "FileSystemWatcherBSDReconcile.h"
#include "FileSystemWatcherThreading.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/event.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

struct SC::FileSystemWatcher::FolderWatcherInternal
{
    using Inventory = FileSystemWatcherBSDInventory;
    using Entry     = Inventory::State::Entry;

    static constexpr size_t DefaultEntries = 128;
    static_assert(sizeof(Entry) <= 64, "Increase the public BSD entry storage size");
    static_assert(alignof(Entry) <= alignof(uint64_t), "Increase the public BSD entry storage alignment");
    static_assert(TypeTraits::IsTriviallyCopyable<Entry>::value,
                  "External entry storage must be trivially destructible");

    Inventory inventory;

    Entry   defaultEntries[DefaultEntries];
    char    defaultNames[4096];
    uint8_t defaultMarks[DefaultEntries];

    Span<char> nameStorage;
    Span<char> entryStorage;

    Span<Entry>   entries;
    Span<char>    names;
    Span<uint8_t> marks;

    uintptr_t rootToken = 0;
    int       rootFd    = -1;

    bool active         = false;
    bool limitedPending = false;
};

struct SC::FileSystemWatcher::ThreadRunnerInternal
{
    FSWThread          thread;
    FSWAtomicBool      shouldStop;
    FileSystemWatcher* owner = nullptr;
};

struct SC::FileSystemWatcher::Internal
{
    using Inventory = FileSystemWatcherBSDInventory;
    using State     = Inventory::State;
    using Reconcile = FileSystemWatcherBSDReconcile;

    struct DeliveryScope
    {
        DeliveryScope* previous = nullptr;
        bool           alive    = true;
    };

    FileSystemWatcher*    self            = nullptr;
    EventLoopRunner*      eventLoopRunner = nullptr;
    ThreadRunnerInternal* threadingRunner = nullptr;

    FSWMutex     mutex;
    FSWCondition callbackFinished;

    FolderWatcher* activeThreadCallback = nullptr;
    DeliveryScope* deliveries           = nullptr;

    FileSystemWatcherBSDRegistrationTokens tokens;

    int queueFd = -1;

    bool closing          = false;
    bool monitoringFailed = false;

    static int changeControl(int queue, unsigned short flags, unsigned int notes)
    {
        struct kevent event;
        EV_SET(&event, 0, EVFILT_USER, flags, notes, 0, nullptr);
        int result;
        do
            result = ::kevent(queue, &event, 1, nullptr, 0, nullptr);
        while (result == -1 and errno == EINTR);
        return result;
    }

    ResultFileSystemWatcher createQueue()
    {
        queueFd = ::kqueue();
        if (queueFd == -1)
            return {FileSystemWatcherError::InitializationFailed, FileSystemWatcherErrorDetail::BSDCreateQueue,
                    static_cast<uint32_t>(errno)};
        if (::fcntl(queueFd, F_SETFD, FD_CLOEXEC) == -1 or changeControl(queueFd, EV_ADD | EV_CLEAR, 0) == -1)
        {
            const uint32_t nativeError = static_cast<uint32_t>(errno);
            ::close(queueFd);
            queueFd = -1;
            return {FileSystemWatcherError::InitializationFailed, FileSystemWatcherErrorDetail::BSDRegisterControlEvent,
                    nativeError};
        }
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher init(FileSystemWatcher& owner, ThreadRunner& runner)
    {
        if (self != nullptr or runner.get().owner != nullptr)
            return {FileSystemWatcherError::InitializationFailed};
        SC_TRY(createQueue());
        self                   = &owner;
        threadingRunner        = &runner.get();
        threadingRunner->owner = &owner;
        threadingRunner->shouldStop.store(false);
        closing          = false;
        monitoringFailed = false;
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher init(FileSystemWatcher& owner, EventLoopRunner& runner)
    {
        if (self != nullptr or runner.fileSystemWatcher != nullptr)
            return {FileSystemWatcherError::InitializationFailed};
        SC_TRY(createQueue());
        self             = &owner;
        eventLoopRunner  = &runner;
        closing          = false;
        monitoringFailed = false;
        runner.internalInit(owner, queueFd);
        const Result started = runner.bsdStartSharedFileReadiness();
        if (not started)
        {
            ::close(queueFd);
            queueFd                  = -1;
            runner.notifyFd          = -1;
            runner.fileSystemWatcher = nullptr;
            eventLoopRunner          = nullptr;
            self                     = nullptr;
        }
        return started;
    }

    static bool overlaps(Span<char> first, Span<char> second)
    {
        if (first.empty() or second.empty())
            return false;
        const uintptr_t a = reinterpret_cast<uintptr_t>(first.data());
        const uintptr_t b = reinterpret_cast<uintptr_t>(second.data());
        return a >= b ? a - b < second.sizeInBytes() : b - a < first.sizeInBytes();
    }

    static ResultFileSystemWatcher prepareStorage(FolderWatcherInternal& folder)
    {
        folder.names = folder.nameStorage.empty() ? Span<char>(folder.defaultNames) : folder.nameStorage;
        if (folder.entryStorage.empty())
        {
            folder.entries = folder.defaultEntries;
            folder.marks   = folder.defaultMarks;
            return ResultFileSystemWatcher(true);
        }
        if (folder.entryStorage.data() == nullptr or overlaps(folder.entryStorage, folder.names))
            return {FileSystemWatcherError::BufferTooSmall, FileSystemWatcherErrorDetail::BSDEntryStorage};
        using Entry             = FolderWatcherInternal::Entry;
        const uintptr_t address = reinterpret_cast<uintptr_t>(folder.entryStorage.data());
        const size_t    padding = (alignof(Entry) - address % alignof(Entry)) % alignof(Entry);
        if (padding >= folder.entryStorage.sizeInBytes())
            return {FileSystemWatcherError::BufferTooSmall, FileSystemWatcherErrorDetail::BSDEntryStorage};
        const size_t count = (folder.entryStorage.sizeInBytes() - padding) / (sizeof(Entry) + 1);
        if (count == 0)
            return {FileSystemWatcherError::BufferTooSmall, FileSystemWatcherErrorDetail::BSDEntryStorage};
        char*  start   = folder.entryStorage.data() + padding;
        Entry* entries = reinterpret_cast<Entry*>(start);
        placementNewArray(entries, count);
        folder.entries = {entries, count};
        folder.marks   = {reinterpret_cast<uint8_t*>(start + sizeof(Entry) * count), count};
        return ResultFileSystemWatcher(true);
    }

    static ResultFileSystemWatcher inventoryError(Inventory::Result result, FileSystemWatcherError error,
                                                  FileSystemWatcherErrorDetail detail)
    {
        if (result.cleanupError != Inventory::Error::None)
            return {FileSystemWatcherError::StopWatchingFailed, FileSystemWatcherErrorDetail::BSDCleanup,
                    static_cast<uint32_t>(result.cleanupNativeError)};
        if (result.error == Inventory::Error::PathState)
        {
            if (result.pathError == State::Error::RecordCapacity)
                error = FileSystemWatcherError::WatchLimitExceeded;
            else if (result.pathError == State::Error::NameCapacity)
                error = FileSystemWatcherError::BufferTooSmall;
        }
        else if (result.error == Inventory::Error::InvalidStorage)
            error = FileSystemWatcherError::BufferTooSmall;
        return {error, detail, static_cast<uint32_t>(result.nativeError)};
    }

    ResultFileSystemWatcher watch(FolderWatcher& watcher, StringSpan path)
    {
        mutex.lock();
        auto unlock = MakeDeferred([this] { mutex.unlock(); });
        if (self == nullptr or closing)
            return {FileSystemWatcherError::NotInitialized};
        if (watcher.parent != nullptr)
            return {FileSystemWatcherError::AlreadyWatching};
        if (monitoringFailed)
            return {FileSystemWatcherError::MonitoringFailed};
        if (path.getEncoding() == StringEncoding::Utf16)
            return {FileSystemWatcherError::UnsupportedPathEncoding};
        if (not watcher.path.assign(path))
            return {FileSystemWatcherError::PathPreparationFailed, FileSystemWatcherErrorDetail::AssignWatchPath};
        FolderWatcherInternal& folder = watcher.internal.get();
        SC_TRY(prepareStorage(folder));

        int root;
        do
            root = ::open(watcher.path.view().bytesIncludingTerminator(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        while (root == -1 and errno == EINTR);
        if (root == -1)
            return {FileSystemWatcherError::WatchSetupFailed, FileSystemWatcherErrorDetail::BSDOpenRootDirectory,
                    static_cast<uint32_t>(errno)};
        folder.rootFd = root;
        char scan[FileSystemWatcherBSDDirectoryReader::MinimumStorageBytes];

        const Inventory::Result started =
            folder.inventory.start(queueFd, root, tokens, folder.entries, folder.names, scan);
        if (not started)
        {
            const int closed     = ::close(root);
            const int closeError = errno;
            folder.rootFd        = -1;
            if (closed == -1 and started.cleanupError == Inventory::Error::None)
                return {FileSystemWatcherError::StopWatchingFailed, FileSystemWatcherErrorDetail::BSDCleanup,
                        static_cast<uint32_t>(closeError)};
            return inventoryError(started, FileSystemWatcherError::WatchSetupFailed,
                                  FileSystemWatcherErrorDetail::BSDInventory);
        }
        folder.active         = true;
        folder.rootToken      = folder.inventory.paths().get(folder.inventory.rootIndex())->native.token;
        folder.limitedPending = folder.inventory.passiveEntries() != 0;
        if (folder.limitedPending and changeControl(queueFd, 0, NOTE_TRIGGER) == -1)
        {
            const uint32_t          nativeError = static_cast<uint32_t>(errno);
            const Inventory::Result cleanup     = folder.inventory.stop();

            const int closed     = ::close(root);
            const int closeError = errno;
            folder.rootFd        = -1;
            folder.active        = false;
            if (not cleanup)
                return inventoryError(cleanup, FileSystemWatcherError::StopWatchingFailed,
                                      FileSystemWatcherErrorDetail::BSDCleanup);
            if (closed == -1)
                return {FileSystemWatcherError::StopWatchingFailed, FileSystemWatcherErrorDetail::BSDCleanup,
                        static_cast<uint32_t>(closeError)};
            return {FileSystemWatcherError::WatchSetupFailed, FileSystemWatcherErrorDetail::BSDTriggerControlEvent,
                    nativeError};
        }
        watcher.parent = self;
        self->watchers.queueBack(watcher);
        if (threadingRunner and not threadingRunner->thread.wasStarted())
        {
            Function<void(FSWThread&)> callback;
            callback.bind<Internal, &Internal::threadRun>(*this);
            const ResultFileSystemWatcher result = threadingRunner->thread.start(move(callback));
            if (not result)
            {
                const ResultFileSystemWatcher cleanup = stopWatchingLocked(watcher);
                return cleanup ? result : cleanup;
            }
        }
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher stopWatchingLocked(FolderWatcher& watcher)
    {
        FolderWatcherInternal&  folder = watcher.internal.get();
        ResultFileSystemWatcher result(true);
        if (folder.active)
        {
            const Inventory::Result stopped = folder.inventory.stop();
            folder.active                   = false;
            if (not stopped)
                result = inventoryError(stopped, FileSystemWatcherError::StopWatchingFailed,
                                        FileSystemWatcherErrorDetail::BSDInventory);
        }
        if (folder.rootFd != -1)
        {
            const int status = ::close(folder.rootFd);
            folder.rootFd    = -1;
            if (status == -1 and result)
                result = {FileSystemWatcherError::StopWatchingFailed, FileSystemWatcherErrorDetail::BSDCleanup,
                          static_cast<uint32_t>(errno)};
        }
        folder.limitedPending = false;
        folder.rootToken      = 0;
        self->watchers.remove(watcher);
        watcher.parent = nullptr;
        return result;
    }

    ResultFileSystemWatcher stopWatching(FolderWatcher& watcher)
    {
        mutex.lock();
        auto       unlock   = MakeDeferred([this] { mutex.unlock(); });
        const bool isWorker = threadingRunner != nullptr and threadingRunner->thread.wasStarted() and
                              ::pthread_equal(::pthread_self(), threadingRunner->thread.thread);
        while (activeThreadCallback == &watcher and not isWorker)
            callbackFinished.wait(mutex);
        if (watcher.parent != self or self == nullptr)
            return {FileSystemWatcherError::NotWatching};
        return stopWatchingLocked(watcher);
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
        if (threadingRunner and threadingRunner->thread.wasStarted())
        {
            threadingRunner->shouldStop.store(true);
            if (changeControl(queueFd, 0, NOTE_TRIGGER) == -1)
            {
                const uint32_t nativeError = static_cast<uint32_t>(errno);
                threadingRunner->shouldStop.store(false);
                closing = false;
                mutex.unlock();
                return {FileSystemWatcherError::CloseFailed, FileSystemWatcherErrorDetail::BSDTriggerControlEvent,
                        nativeError};
            }
        }
        if (eventLoopRunner)
        {
            for (DeliveryScope* delivery = deliveries; delivery != nullptr; delivery = delivery->previous)
                delivery->alive = false;
            deliveries = nullptr;
        }
        ThreadRunnerInternal* threadRunner = threadingRunner;
        EventLoopRunner*      asyncRunner  = eventLoopRunner;
        mutex.unlock();

        if (asyncRunner)
        {
            const Result stopped = asyncRunner->bsdStopSharedFileReadiness();
            if (not stopped)
            {
                mutex.lock();
                closing          = false;
                monitoringFailed = true;
                mutex.unlock();
                return stopped;
            }
        }
        if (threadRunner and threadRunner->thread.wasStarted())
        {
            const ResultFileSystemWatcher joined = threadRunner->thread.join();
            if (not joined)
            {
                mutex.lock();
                closing          = false;
                monitoringFailed = true;
                mutex.unlock();
                return joined;
            }
        }

        mutex.lock();
        ResultFileSystemWatcher result(true);
        while (FolderWatcher* watcher = self->watchers.front)
        {
            const ResultFileSystemWatcher stopped = stopWatchingLocked(*watcher);
            if (not stopped and result)
                result = stopped;
        }
        if (::close(queueFd) == -1 and result)
            result = {FileSystemWatcherError::CloseFailed, FileSystemWatcherErrorDetail::BSDCleanup,
                      static_cast<uint32_t>(errno)};
        queueFd = -1;
        if (asyncRunner)
        {
            asyncRunner->notifyFd          = -1;
            asyncRunner->fileSystemWatcher = nullptr;
        }
        if (threadRunner)
            threadRunner->owner = nullptr;
        self             = nullptr;
        eventLoopRunner  = nullptr;
        threadingRunner  = nullptr;
        closing          = false;
        monitoringFailed = false;
        mutex.unlock();
        return result;
    }

    // On entry the mutex is held. If a callback closes an Async owner, returns false without reacquiring it.
    bool deliver(FolderWatcher& watcher, NotificationKind kind, Operation operation, const char* relative,
                 ResultFileSystemWatcher failure, DeliveryScope& scope)
    {
        StringPath base = watcher.path;

        Function<void(const Notification&)> callback = watcher.notifyCallback;

        Notification notification;
        notification.basePath     = base.view();
        notification.relativePath = StringSpan::fromNullTerminated(relative, StringEncoding::Utf8);
        notification.kind         = kind;
        notification.operation    = operation;
        notification.failure      = failure;
        if (threadingRunner)
            activeThreadCallback = &watcher;
        mutex.unlock();
        if (callback.isValid())
            callback(notification);
        if (not scope.alive)
            return false;
        mutex.lock();
        if (threadingRunner)
        {
            activeThreadCallback = nullptr;
            callbackFinished.broadcast();
        }
        return true;
    }

    // Caller holds the mutex, and a false return means a callback closed the owner.
    bool failMonitoring(ResultFileSystemWatcher failure, DeliveryScope& scope)
    {
        monitoringFailed   = true;
        uintptr_t previous = 0;
        for (;;)
        {
            FolderWatcher* selected = nullptr;
            uintptr_t      token    = UINTPTR_MAX;
            for (FolderWatcher* item = self->watchers.front; item != nullptr; item = item->next)
            {
                const uintptr_t current = item->internal.get().rootToken;
                if (current > previous and (selected == nullptr or current < token))
                {
                    selected = item;
                    token    = current;
                }
            }
            if (selected == nullptr)
                return true;
            previous = token;
            if (not deliver(*selected, NotificationKind::WatchStopped, Operation::AddRemoveRename, "", failure, scope))
                return false;
        }
    }

    bool dispatch(const struct kevent& event, DeliveryScope& scope)
    {
        if (closing or monitoringFailed)
            return true;
        if ((event.flags & EV_ERROR) != 0)
            return failMonitoring({FileSystemWatcherError::MonitoringFailed,
                                   FileSystemWatcherErrorDetail::BSDReadEvents, static_cast<uint32_t>(event.data)},
                                  scope);
        if (event.filter == EVFILT_USER)
        {
            for (;;)
            {
                FolderWatcher* selected = nullptr;
                for (FolderWatcher* item = self->watchers.front; item != nullptr; item = item->next)
                {
                    if (item->internal.get().limitedPending)
                    {
                        selected = item;
                        break;
                    }
                }
                if (selected == nullptr)
                    return true;
                selected->internal.get().limitedPending = false;
                if (not deliver(
                        *selected, NotificationKind::CoverageLimited, Operation::AddRemoveRename, "",
                        {FileSystemWatcherError::CoverageLimited, FileSystemWatcherErrorDetail::BSDPassiveEntries},
                        scope))
                    return false;
                if (closing or monitoringFailed)
                    return true;
            }
        }
        if (event.filter != EVFILT_VNODE)
            return true;
        const uintptr_t token    = reinterpret_cast<uintptr_t>(event.udata);
        FolderWatcher*  selected = nullptr;
        size_t          index    = State::InvalidIndex;
        for (FolderWatcher* item = self->watchers.front; item != nullptr; item = item->next)
        {
            FolderWatcherInternal& folder = item->internal.get();
            if (not folder.active)
                continue;
            index = folder.inventory.paths().findToken(token);
            if (index != State::InvalidIndex)
            {
                selected = item;
                break;
            }
        }
        if (selected == nullptr)
            return true; // A queued event for a retired token.
        FolderWatcherInternal& folder = selected->internal.get();
        const State::Entry*    entry  = folder.inventory.paths().get(index);
        if ((event.fflags & NOTE_REVOKE) != 0)
            return failMonitoring(
                {FileSystemWatcherError::MonitoringFailed, FileSystemWatcherErrorDetail::BSDVnodeRevoked}, scope);
        if (index == folder.inventory.rootIndex() and (event.fflags & (NOTE_DELETE | NOTE_RENAME)) != 0)
            return failMonitoring(
                {FileSystemWatcherError::MonitoringFailed, FileSystemWatcherErrorDetail::BSDReconcile, ENOENT}, scope);
        const bool structural = (event.fflags & (NOTE_DELETE | NOTE_RENAME)) != 0 or
                                (entry->native.directory and (event.fflags & NOTE_WRITE) != 0);
        char relative[4097] = {};
        if (structural)
        {
            char scan[FileSystemWatcherBSDDirectoryReader::MinimumStorageBytes];

            Reconcile::Change change;

            const Inventory::Result reconciled = Reconcile::run(folder.inventory, scan, folder.marks, relative, change);
            if (not reconciled)
            {
                // A failed scan stops the inventory itself.
                if (reconciled.error != Inventory::Error::InvalidStorage)
                    folder.active = false;
                return failMonitoring(inventoryError(reconciled, FileSystemWatcherError::MonitoringFailed,
                                                     FileSystemWatcherErrorDetail::BSDReconcile),
                                      scope);
            }
            if (change.passiveAdded)
            {
                folder.limitedPending = true;
                if (changeControl(queueFd, 0, NOTE_TRIGGER) == -1)
                    return failMonitoring({FileSystemWatcherError::MonitoringFailed,
                                           FileSystemWatcherErrorDetail::BSDTriggerControlEvent,
                                           static_cast<uint32_t>(errno)},
                                          scope);
            }
            const auto kind = change.rescanRequired ? NotificationKind::RescanRequired : NotificationKind::Change;
            return deliver(*selected, kind, Operation::AddRemoveRename, relative, ResultFileSystemWatcher(true), scope);
        }
        if (folder.inventory.paths().copyRelativePath(index, relative) != State::Error::None)
            return deliver(*selected, NotificationKind::RescanRequired, Operation::AddRemoveRename, "",
                           ResultFileSystemWatcher(true), scope);
        return deliver(*selected, NotificationKind::Change, Operation::Modified, relative,
                       ResultFileSystemWatcher(true), scope);
    }

    bool dispatchOne(const struct kevent& event)
    {
        mutex.lock();
        if (self == nullptr or closing)
        {
            mutex.unlock();
            return false;
        }
        DeliveryScope scope{deliveries};
        deliveries           = &scope;
        const bool delivered = dispatch(event, scope);
        if (not scope.alive)
            return false;
        deliveries         = scope.previous;
        const bool running = delivered and not closing and not monitoringFailed;
        mutex.unlock();
        return running;
    }

    void threadRun(FSWThread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("FileSystemWatcher"));
        while (not threadingRunner->shouldStop.load())
        {
            struct kevent event;
            int           count;
            do
                count = ::kevent(queueFd, nullptr, 0, &event, 1, nullptr);
            while (count == -1 and errno == EINTR);
            if (count == -1)
            {
                notifyFailure({FileSystemWatcherError::MonitoringFailed, FileSystemWatcherErrorDetail::BSDWaitForEvents,
                               static_cast<uint32_t>(errno)});
                return;
            }
            if (threadingRunner->shouldStop.load())
                return;
            if (count != 0 and not dispatchOne(event))
                return;
        }
    }

    bool readAndNotify()
    {
        mutex.lock();
        const bool running      = self != nullptr and not closing and not monitoringFailed;
        const int  currentQueue = queueFd;
        mutex.unlock();
        if (not running)
            return false;
        struct kevent event;

        const struct timespec noWait = {0, 0};

        int count;
        do
            count = ::kevent(currentQueue, nullptr, 0, &event, 1, &noWait);
        while (count == -1 and errno == EINTR);
        if (count == -1)
        {
            notifyFailure({FileSystemWatcherError::MonitoringFailed, FileSystemWatcherErrorDetail::BSDReadEvents,
                           static_cast<uint32_t>(errno)});
            return false;
        }
        return count == 0 or dispatchOne(event);
    }

    void notifyFailure(ResultFileSystemWatcher failure)
    {
        mutex.lock();
        if (self == nullptr or closing or monitoringFailed)
        {
            mutex.unlock();
            return;
        }
        DeliveryScope scope{deliveries};
        deliveries = &scope;
        (void)failMonitoring(failure, scope);
        if (scope.alive)
        {
            deliveries = scope.previous;
            mutex.unlock();
        }
    }
};

SC::ResultFileSystemWatcher SC::FileSystemWatcher::Notification::getFullPath(StringPath& buffer) const
{
    if (not buffer.assign(basePath) or not buffer.append("/") or not buffer.append(relativePath))
        return {FileSystemWatcherError::PathPreparationFailed, FileSystemWatcherErrorDetail::BuildFullPath};
    return ResultFileSystemWatcher(true);
}

bool SC::FileSystemWatcher::asyncNotify(FolderWatcher*, size_t) { return internal.get().readAndNotify(); }

void SC::FileSystemWatcher::asyncNotifyFailure(Result failure)
{
    internal.get().notifyFailure(ResultFileSystemWatcher(failure));
}
