// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

//! [OpaqueDefinition1Snippet]
#include "../../Common/Deferred.h"
#include "../../FileSystemWatcher/FileSystemWatcher.h"
#include "FileSystemWatcherThreading.h"

#include <CoreServices/CoreServices.h> // FSEvents
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>

#include <AvailabilityMacros.h>
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
// TODO: Figure out another API for ios as this is a private API and it will not be accepted on app store.
#include "FSEventsIOS.h"
#endif

struct SC::FileSystemWatcher::FolderWatcherInternal
{
    StringPath canonicalPath;
    bool       stopped       = false;
    bool       inStream      = false;
    bool       rescanPending = false;
};

struct SC::FileSystemWatcher::Internal
{
    FileSystemWatcher*      self          = nullptr;
    CFRunLoopRef            runLoop       = nullptr;
    CFRunLoopSourceRef      refreshSignal = nullptr;
    FSEventStreamRef        fsEventStream = nullptr;
    FSWThread               pollingThread;
    ResultFileSystemWatcher signalReturnCode = ResultFileSystemWatcher(true);
    FSWEventObject          refreshSignalFinished;
    FSWEventObject          refreshResume;
    FSWMutex                mutex;
    EventLoopRunner*        eventLoopRunner = nullptr;

    // Used to pass data from thread to async callback
    Notification   notification;
    FolderWatcher* watcher = nullptr;
    FSWAtomicBool  closing;
    FSWAtomicBool  refreshing;
    bool           deliveringRefreshHint = false;

    //...
    //! [OpaqueDefinition1Snippet]

    static ResultFileSystemWatcher prepareWatchPath(FolderWatcher& entry)
    {
        const int fd = ::open(entry.path.view().bytesIncludingTerminator(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd == -1)
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                           FileSystemWatcherErrorDetail::AppleOpenRootDirectory,
                                           static_cast<uint32_t>(errno));
        auto closeDirectory = MakeDeferred([fd] { (void)::close(fd); });
        char canonicalPath[PATH_MAX];
        if (::fcntl(fd, F_GETPATH, canonicalPath) == -1)
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                           FileSystemWatcherErrorDetail::AppleResolveWatchRoot,
                                           static_cast<uint32_t>(errno));
        auto& folder = entry.internal.get();
        if (not folder.canonicalPath.assign(StringSpan::fromNullTerminated(canonicalPath, StringEncoding::Utf8)))
            return ResultFileSystemWatcher(FileSystemWatcherError::PathPreparationFailed,
                                           FileSystemWatcherErrorDetail::AppleResolveWatchRoot);
        folder.stopped       = false;
        folder.inStream      = false;
        folder.rescanPending = false;
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher init(FileSystemWatcher& parent, ThreadRunner& runner)
    {
        (void)(runner);
        self = &parent;
        closing.store(false);
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher init(FileSystemWatcher& parent, EventLoopRunner& runner)
    {
        runner.internalInit(parent, 0);
        const Result result = runner.appleStartWakeUp();
        if (not result)
        {
            runner.fileSystemWatcher = nullptr;
            return result;
        }
        self            = &parent;
        eventLoopRunner = &runner;
        closing.store(false);
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher initThread()
    {
        closing.exchange(false);
        // Create Signal to go from Loop --> CFRunLoop
        CFRunLoopSourceContext signalContext;
        ::memset(&signalContext, 0, sizeof(signalContext));

        signalContext.info    = this;
        signalContext.perform = &Internal::threadExecuteRefresh;
        refreshSignal         = CFRunLoopSourceCreate(nullptr, 0, &signalContext);
        if (refreshSignal == nullptr)
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                           FileSystemWatcherErrorDetail::AppleCreateRunLoopSource);

        FSWEventObject eventObject;

        auto pollingFunction = [&](FSWThread& thread)
        {
            thread.setThreadName("FileSystemWatcher::init");
            threadInit(); // Obtain the CFRunLoop for this thread
            eventObject.signal();
            threadRun();
        };
        const auto started = pollingThread.start(pollingFunction);
        if (not started)
        {
            releaseResources();
            return started;
        }
        eventObject.wait();
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher close()
    {
        if (self == nullptr)
            return ResultFileSystemWatcher(FileSystemWatcherError::NotInitialized);
        if (onWorkerThread() or refreshing.load() or closing.load())
            return {FileSystemWatcherError::CloseFailed, FileSystemWatcherErrorDetail::PosixPthreadJoin, EDEADLK};
        closing.store(true);
        if (pollingThread.wasStarted())
        {
            if (eventLoopRunner)
            {
                eventLoopRunner->appleSignalEventObject();
            }

            // send close signal
            wakeUpFSEventThread();

            // Wait for thread to finish
            SC_TRY(pollingThread.join());
            releaseResources();
        }
        if (eventLoopRunner)
        {
            SC_TRY(eventLoopRunner->appleStopWakeUp());
            eventLoopRunner->fileSystemWatcher = nullptr;
            eventLoopRunner                    = nullptr;
        }
        while (FolderWatcher* entry = self->watchers.front)
        {
            self->watchers.remove(*entry);
            entry->parent = nullptr;
        }
        watcher = nullptr;
        self    = nullptr;
        return ResultFileSystemWatcher(true);
    }

    bool onWorkerThread() const
    {
        return pollingThread.wasStarted() and ::pthread_equal(::pthread_self(), pollingThread.thread);
    }

    void wakeUpFSEventThread()
    {
        CFRunLoopSourceSignal(refreshSignal);
        CFRunLoopWakeUp(runLoop);
        refreshSignalFinished.wait();
    }

    void releaseResources()
    {
        CFRelease(refreshSignal);
        refreshSignal = nullptr;
    }

    // This gets executed before Thread::start returns
    void threadInit()
    {
        runLoop = CFRunLoopGetCurrent();
        CFRunLoopAddSource(runLoop, refreshSignal, kCFRunLoopDefaultMode);
    }

    void threadRun()
    {
        CFRunLoopRef copyRunLoop = runLoop;
        CFRunLoopRun();
        CFRunLoopRemoveSource(copyRunLoop, refreshSignal, kCFRunLoopDefaultMode);
    }

    ResultFileSystemWatcher threadCreateFSEvent(FSEventStreamRef& replacement)
    {
        if (runLoop == nullptr)
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                           FileSystemWatcherErrorDetail::AppleRunLoop);
        const void* watchedPaths[ThreadRunnerDefinition::MaxWatchablePaths];
        size_t      numAllocatedPaths = 0;
        auto        deferDeletePaths  = MakeDeferred(
            [&]
            {
                for (size_t idx = 0; idx < numAllocatedPaths; ++idx)
                {
                    CFRelease(watchedPaths[idx]);
                }
            });
        mutex.lock();
        auto unlock = MakeDeferred([&] { mutex.unlock(); });
        for (FolderWatcher* it = self->watchers.front; it != nullptr; it = it->next)
        {
            const auto& folder = it->internal.get();
            if (folder.stopped)
                continue;
            if (numAllocatedPaths == ThreadRunnerDefinition::MaxWatchablePaths)
                return ResultFileSystemWatcher(FileSystemWatcherError::WatchLimitExceeded,
                                               FileSystemWatcherErrorDetail::WatchPathCapacity);
            watchedPaths[numAllocatedPaths] =
                CFStringCreateWithFileSystemRepresentation(nullptr, it->path.view().bytesIncludingTerminator());
            if (not watchedPaths[numAllocatedPaths])
                return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                               FileSystemWatcherErrorDetail::AppleCreateWatchPathString);
            numAllocatedPaths++;
        }
        if (numAllocatedPaths == 0)
        {
            return ResultFileSystemWatcher(true);
        }
        const CFArrayRef pathsArray =
            CFArrayCreate(nullptr, watchedPaths, static_cast<CFIndex>(numAllocatedPaths), &kCFTypeArrayCallBacks);
        if (not pathsArray)
        {
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                           FileSystemWatcherErrorDetail::AppleCreateWatchPathsArray);
        }
        auto releasePaths = MakeDeferred([pathsArray] { CFRelease(pathsArray); });

        // Create Stream
        constexpr CFAbsoluteTime           watchLatency = 0.2;
        constexpr FSEventStreamCreateFlags watchFlags =
            kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer | kFSEventStreamCreateFlagWatchRoot;
        FSEventStreamContext fsEventContext;
        ::memset(&fsEventContext, 0, sizeof(fsEventContext));
        fsEventContext.info = this;
        replacement         = FSEventStreamCreate(nullptr,                       //
                                                  &Internal::threadOnNewFSEvent, //
                                                  &fsEventContext,               //
                                                  pathsArray,                    //
                                                  kFSEventStreamEventIdSinceNow, //
                                                  watchLatency,                  //
                                                  watchFlags);
        if (replacement == nullptr)
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                           FileSystemWatcherErrorDetail::AppleCreateEventStream);

#if SC_COMPILER_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
        // Add it to runLoop
        FSEventStreamScheduleWithRunLoop(replacement, runLoop, kCFRunLoopDefaultMode);
#if SC_COMPILER_CLANG
#pragma clang diagnostic pop
#endif

        if (not FSEventStreamStart(replacement))
        {
            FSEventStreamInvalidate(replacement);
            FSEventStreamRelease(replacement);
            replacement = nullptr;
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                           FileSystemWatcherErrorDetail::AppleStartEventStream);
        }
        return ResultFileSystemWatcher(true);
    }

    void threadDestroyFSEvent()
    {
        FSEventStreamStop(fsEventStream);
        FSEventStreamInvalidate(fsEventStream);
        FSEventStreamRelease(fsEventStream);
        fsEventStream = nullptr;
    }

    ResultFileSystemWatcher stopWatching(FolderWatcher& folderWatcher)
    {
        if (onWorkerThread() or refreshing.load() or closing.load())
            return {FileSystemWatcherError::StopWatchingFailed, FileSystemWatcherErrorDetail::PosixPthreadJoin,
                    EDEADLK};
        refreshing.store(true);
        detach(folderWatcher);
        return refreshStream(nullptr);
    }

    void detach(FolderWatcher& folderWatcher)
    {
        mutex.lock();
        self->watchers.remove(folderWatcher);
        folderWatcher.parent = nullptr;
        mutex.unlock();
    }

    ResultFileSystemWatcher startWatching(FolderWatcher* entry)
    {
        if (onWorkerThread() or refreshing.load() or closing.load())
            return {FileSystemWatcherError::WatchSetupFailed, FileSystemWatcherErrorDetail::PosixPthreadJoin, EDEADLK};
        SC_TRY(prepareWatchPath(*entry));
        if (not pollingThread.wasStarted())
        {
            SC_TRY(initThread());
        }
        refreshing.store(true);
        mutex.lock();
        entry->parent = self;
        self->watchers.queueBack(*entry);
        mutex.unlock();
        return refreshStream(entry);
    }

    ResultFileSystemWatcher refreshStream(FolderWatcher* added)
    {
        // A pending notification may be waiting for the caller's event loop. Release that
        // handoff before synchronously waiting for the FSEvents thread to refresh its stream.
        if (eventLoopRunner)
        {
            eventLoopRunner->appleSignalEventObject();
        }
        wakeUpFSEventThread();
        const auto result = signalReturnCode;
        if (not result and added)
            detach(*added);
        if (eventLoopRunner)
            eventLoopRunner->appleResetHandoff();
        refreshing.store(false);
        refreshResume.signal();
        return result;
    }
    static constexpr int EVENT_MODIFIED = kFSEventStreamEventFlagItemChangeOwner |   //
                                          kFSEventStreamEventFlagItemFinderInfoMod | //
                                          kFSEventStreamEventFlagItemInodeMetaMod |  //
                                          kFSEventStreamEventFlagItemModified |      //
                                          kFSEventStreamEventFlagItemXattrMod;

    static constexpr int EVENT_RENAMED = kFSEventStreamEventFlagItemCreated | //
                                         kFSEventStreamEventFlagItemRemoved | //
                                         kFSEventStreamEventFlagItemRenamed;

    static constexpr int EVENT_GLOBAL_LOSS = kFSEventStreamEventFlagUserDropped | kFSEventStreamEventFlagKernelDropped |
                                             kFSEventStreamEventFlagEventIdsWrapped;
    static constexpr int EVENT_SUBTREE_LOSS =
        kFSEventStreamEventFlagMustScanSubDirs | kFSEventStreamEventFlagMount | kFSEventStreamEventFlagUnmount;
    static constexpr int EVENT_COVERAGE = EVENT_GLOBAL_LOSS | EVENT_SUBTREE_LOSS | kFSEventStreamEventFlagRootChanged;

    static void threadOnNewFSEvent(ConstFSEventStreamRef streamRef, void* info, size_t numEvents, void* eventPaths,
                                   const FSEventStreamEventFlags* eventFlags, const FSEventStreamEventId* eventIds)
    {
        (void)eventIds;
        Internal& internal = *static_cast<Internal*>(info);
        if (streamRef != internal.fsEventStream)
            return;
        const char** paths = static_cast<const char**>(eventPaths);
        for (size_t idx = 0; idx < numEvents and not internal.closing.load() and not internal.refreshing.load(); ++idx)
        {
            const auto flags = eventFlags[idx];
            if ((flags & kFSEventStreamEventFlagHistoryDone) and not(flags & EVENT_COVERAGE))
                continue;

            const StringSpan path      = StringSpan::fromNullTerminated(paths[idx], StringEncoding::Utf8);
            bool             duplicate = false;
            if (not(flags & EVENT_COVERAGE))
            {
                for (size_t prev = 0; prev < idx; ++prev)
                {
                    if ((eventFlags[prev] & (EVENT_COVERAGE | kFSEventStreamEventFlagHistoryDone)) == 0 and
                        path == StringSpan::fromNullTerminated(paths[prev], StringEncoding::Utf8))
                    {
                        duplicate = true;
                        break;
                    }
                }
            }
            if (not duplicate)
                internal.notify(path, flags);
        }
    }

    static bool containsPath(StringSpan root, StringSpan path)
    {
        const auto       bytes = path.toCharSpan();
        const size_t     size  = root.sizeInBytes();
        Span<const char> prefix;
        return size > 0 and bytes.sliceStartLength(0, size, prefix) and
               root == StringSpan(prefix, false, path.getEncoding()) and
               (size == bytes.sizeInBytes() or prefix[size - 1] == '/' or bytes[size] == '/');
    }

    void notify(StringSpan path, FSEventStreamEventFlags flags, bool refreshHint = false)
    {
        const bool rootChanged = flags & kFSEventStreamEventFlagRootChanged;
        const bool globalLoss  = flags & EVENT_GLOBAL_LOSS;
        const bool subtreeLoss = flags & EVENT_SUBTREE_LOSS;
        const bool renamed     = flags & EVENT_RENAMED;
        const bool modified    = flags & EVENT_MODIFIED;
        const bool directory   = flags & kFSEventStreamEventFlagItemIsDir;

        uintptr_t previous = 0;
        while (not refreshing.load() and not closing.load())
        {
            mutex.lock();
            FolderWatcher*   selected = nullptr;
            uintptr_t        address  = 0;
            NotificationKind kind     = NotificationKind::Change;
            for (auto* entry = self->watchers.front; entry != nullptr; entry = entry->next)
            {
                const auto  current = reinterpret_cast<uintptr_t>(entry);
                const auto& folder  = entry->internal.get();
                if (current <= previous or (selected != nullptr and current >= address) or folder.stopped)
                    continue;
                const StringSpan root = folder.canonicalPath.view();
                if (refreshHint)
                {
                    if (not folder.rescanPending)
                        continue;
                    kind = NotificationKind::RescanRequired;
                }
                else if (rootChanged and root == path)
                    kind = NotificationKind::WatchStopped;
                else if (globalLoss or (subtreeLoss and (containsPath(root, path) or containsPath(path, root))))
                    kind = NotificationKind::RescanRequired;
                else if (not rootChanged and not subtreeLoss and containsPath(root, path))
                    kind = NotificationKind::Change;
                else
                    continue;
                selected = entry;
                address  = current;
            }
            if (selected == nullptr)
            {
                mutex.unlock();
                return;
            }
            previous     = address;
            auto& folder = selected->internal.get();
            if (refreshHint and not eventLoopRunner)
                folder.rescanPending = false;

            notification           = {};
            notification.basePath  = selected->path.view();
            notification.fullPath  = notification.basePath;
            notification.operation = Operation::AddRemoveRename;
            notification.kind      = kind;
            if (kind == NotificationKind::WatchStopped)
            {
                notification.failure = {FileSystemWatcherError::MonitoringFailed,
                                        FileSystemWatcherErrorDetail::AppleWatchInvalidated};
                if (not eventLoopRunner)
                    folder.stopped = true;
            }
            else if (kind == NotificationKind::Change)
            {
                auto relative = path.toCharSpan();
                (void)relative.sliceStart(folder.canonicalPath.view().sizeInBytes(), relative);
                if (relative.sizeInBytes() != 0 and relative[0] == '/')
                    (void)relative.sliceStart(1, relative);
                notification.relativePath = {relative, true, path.getEncoding()};
                notification.fullPath     = path;
                if (not renamed and (modified or not directory))
                    notification.operation = Operation::Modified;
            }
            watcher               = selected;
            deliveringRefreshHint = refreshHint;
            mutex.unlock();
            if (eventLoopRunner)
                (void)eventLoopRunner->appleWakeUpAndWait();
            else
                selected->notifyCallback(notification);
        }
    }

    bool deliverOnEventLoop()
    {
        if (not refreshing.load() and not closing.load() and watcher != nullptr and watcher->parent == self)
        {
            auto         callback  = watcher->notifyCallback;
            Notification delivered = notification;
            // Refresh or close may release the native event batch and let the worker reuse the
            // handoff slot before this callback returns.
            StringPath basePath, fullPath;
            if (not basePath.assign(delivered.basePath) or not fullPath.assign(delivered.fullPath))
            {
                delivered = {};
                if (not basePath.isEmpty())
                    delivered.basePath = basePath.view();
                delivered.fullPath  = delivered.basePath;
                delivered.kind      = NotificationKind::RescanRequired;
                delivered.operation = Operation::AddRemoveRename;
            }
            else
            {
                if (delivered.kind == NotificationKind::Change)
                {
                    auto relative = fullPath.view().toCharSpan();
                    (void)relative.sliceStart(relative.sizeInBytes() - delivered.relativePath.sizeInBytes(), relative);
                    delivered.relativePath = {relative, true, fullPath.view().getEncoding()};
                }
                delivered.basePath = basePath.view();
                delivered.fullPath = fullPath.view();
            }
            if (deliveringRefreshHint)
                watcher->internal.get().rescanPending = false;
            if (delivered.kind == NotificationKind::WatchStopped)
                watcher->internal.get().stopped = true;
            watcher = nullptr;
            callback(delivered);
            return true;
        }
        return false;
    }

    static void threadExecuteRefresh(void* arg)
    {
        Internal& self = *static_cast<Internal*>(arg);
        if (self.closing.load())
        {
            if (self.fsEventStream)
                self.threadDestroyFSEvent();
            CFRunLoopStop(self.runLoop);
            self.runLoop = nullptr;
            self.refreshSignalFinished.signal();
            return;
        }

        FSEventStreamRef replacement = nullptr;
        self.signalReturnCode        = self.threadCreateFSEvent(replacement);
        if (self.signalReturnCode)
        {
            if (self.fsEventStream)
                self.threadDestroyFSEvent();
            self.fsEventStream = replacement;
        }

        // SinceNow cannot replay a discarded batch. Every surviving root may need a rescan
        // even if the old stream was retained after a failed replacement.
        self.mutex.lock();
        for (auto* entry = self.self->watchers.front; entry != nullptr; entry = entry->next)
        {
            auto& folder         = entry->internal.get();
            folder.rescanPending = folder.inStream and not folder.stopped;
            if (self.signalReturnCode)
                folder.inStream = not folder.stopped;
        }
        self.mutex.unlock();
        self.watcher = nullptr;
        self.refreshSignalFinished.signal();
        self.refreshResume.wait();
        self.notify({}, 0, true);
    }
};

SC::ResultFileSystemWatcher SC::FileSystemWatcher::Notification::getFullPath(StringPath& path) const
{
    if (not path.assign(fullPath))
        return ResultFileSystemWatcher(FileSystemWatcherError::PathPreparationFailed,
                                       FileSystemWatcherErrorDetail::BuildFullPath);
    return ResultFileSystemWatcher(true);
}
struct SC::FileSystemWatcher::ThreadRunnerInternal
{
};

bool SC::FileSystemWatcher::asyncNotify(FolderWatcher*, size_t) { return internal.get().deliverOnEventLoop(); }
