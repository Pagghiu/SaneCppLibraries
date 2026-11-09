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
    bool       stopped = false;
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
    FSWMutex                mutex;
    EventLoopRunner*        eventLoopRunner = nullptr;

    // Used to pass data from thread to async callback
    Notification   notification;
    FolderWatcher* watcher = nullptr;
    FSWAtomicBool  closing;
    FSWAtomicBool  refreshing;

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
        folder.stopped = false;
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher init(FileSystemWatcher& parent, ThreadRunner& runner)
    {
        (void)(runner);
        self = &parent;
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher init(FileSystemWatcher& parent, EventLoopRunner& runner)
    {
        self            = &parent;
        eventLoopRunner = &runner;
        eventLoopRunner->internalInit(parent, 0);
        return eventLoopRunner->appleStartWakeUp();
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
        SC_TRY(pollingThread.start(pollingFunction));
        eventObject.wait();
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher close()
    {
        if (self == nullptr)
            return ResultFileSystemWatcher(FileSystemWatcherError::NotInitialized);
        if (pollingThread.wasStarted())
        {
            closing.exchange(true);
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
        }
        return ResultFileSystemWatcher(true);
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

    ResultFileSystemWatcher threadCreateFSEvent()
    {
        if (runLoop == nullptr)
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                           FileSystemWatcherErrorDetail::AppleRunLoop);
        CFArrayRef   pathsArray = nullptr;
        CFStringRef* watchedPaths =
            (CFStringRef*)malloc(sizeof(CFStringRef) * ThreadRunnerDefinition::MaxWatchablePaths);
        if (watchedPaths == nullptr)
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                           FileSystemWatcherErrorDetail::AppleAllocateWatchPaths);
        // TODO: Loop to convert paths
        auto   deferFreeMalloc   = MakeDeferred([&] { free(watchedPaths); });
        size_t numAllocatedPaths = 0;
        auto   deferDeletePaths  = MakeDeferred(
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
        pathsArray = CFArrayCreate(nullptr, reinterpret_cast<const void**>(watchedPaths),
                                   static_cast<CFIndex>(numAllocatedPaths), &kCFTypeArrayCallBacks);
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
        fsEventStream       = FSEventStreamCreate(nullptr,                       //
                                                  &Internal::threadOnNewFSEvent, //
                                                  &fsEventContext,               //
                                                  pathsArray,                    //
                                                  kFSEventStreamEventIdSinceNow, //
                                                  watchLatency,                  //
                                                  watchFlags);
        if (fsEventStream == nullptr)
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                           FileSystemWatcherErrorDetail::AppleCreateEventStream);

#if SC_COMPILER_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
        // Add it to runLoop
        FSEventStreamScheduleWithRunLoop(fsEventStream, runLoop, kCFRunLoopDefaultMode);
#if SC_COMPILER_CLANG
#pragma clang diagnostic pop
#endif

        if (not FSEventStreamStart(fsEventStream))
        {
            FSEventStreamInvalidate(fsEventStream);
            FSEventStreamRelease(fsEventStream);
            fsEventStream = nullptr;
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
        mutex.lock();
        folderWatcher.parent->watchers.remove(folderWatcher);
        folderWatcher.parent = nullptr;
        mutex.unlock();
        return startWatching(nullptr);
    }

    ResultFileSystemWatcher startWatching(FolderWatcher*)
    {
        if (not pollingThread.wasStarted())
        {
            SC_TRY(initThread());
        }
        // A pending notification may be waiting for the caller's event loop. Release that
        // handoff before synchronously waiting for the FSEvents thread to refresh its stream.
        refreshing.exchange(true);
        if (eventLoopRunner)
        {
            eventLoopRunner->appleSignalEventObject();
        }
        wakeUpFSEventThread();
        if (eventLoopRunner)
        {
            SC_TRY(eventLoopRunner->appleStopWakeUp());
            SC_TRY(eventLoopRunner->appleStartWakeUp());
        }
        refreshing.exchange(false);
        return signalReturnCode;
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
        (void)streamRef;
        (void)eventIds;
        Internal&    internal = *static_cast<Internal*>(info);
        const char** paths    = static_cast<const char**>(eventPaths);
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

    void notify(StringSpan path, FSEventStreamEventFlags flags)
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
                if (rootChanged and root == path)
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

            notification           = {};
            notification.basePath  = selected->path.view();
            notification.fullPath  = notification.basePath;
            notification.operation = Operation::AddRemoveRename;
            notification.kind      = kind;
            if (kind == NotificationKind::WatchStopped)
            {
                notification.failure = {FileSystemWatcherError::MonitoringFailed,
                                        FileSystemWatcherErrorDetail::AppleWatchInvalidated};
                folder.stopped       = true;
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
            watcher = selected;
            mutex.unlock();
            if (eventLoopRunner)
                (void)eventLoopRunner->appleWakeUpAndWait();
            else
                selected->notifyCallback(notification);
        }
    }

    static void threadExecuteRefresh(void* arg)
    {
        Internal& self = *static_cast<Internal*>(arg);
        if (self.fsEventStream)
        {
            self.threadDestroyFSEvent();
        }
        if (self.closing.load())
        {
            CFRunLoopStop(self.runLoop);
            self.runLoop = nullptr;
        }
        else
        {
            self.signalReturnCode = self.threadCreateFSEvent();
        }
        self.refreshSignalFinished.signal();
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

bool SC::FileSystemWatcher::asyncNotify(FolderWatcher*, size_t)
{
    Internal& impl = internal.get();
    if (not impl.refreshing.load() and not impl.closing.load() and impl.watcher != nullptr and
        impl.watcher->parent == this)
    {
        impl.watcher->notifyCallback(impl.notification);
    }
    return true;
}
