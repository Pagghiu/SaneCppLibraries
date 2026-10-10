// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

//! [OpaqueDefinition1Snippet]
#include "../../Common/Deferred.h"
#include "../../FileSystemWatcher/FileSystemWatcher.h"
#include "FileSystemWatcherThreading.h"

#include <CoreServices/CoreServices.h> // FSEvents

#include <AvailabilityMacros.h>
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
// TODO: Figure out another API for ios as this is a private API and it will not be accepted on app store.
#include "FSEventsIOS.h"
#endif

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
    FolderWatcher* watcher;
    FSWAtomicBool  closing;
    FSWAtomicBool  refreshing;

    //...
    //! [OpaqueDefinition1Snippet]

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
        for (FolderWatcher* it = self->watchers.front; it != nullptr; it = it->next)
        {
            watchedPaths[numAllocatedPaths] =
                CFStringCreateWithFileSystemRepresentation(nullptr, it->path.view().bytesIncludingTerminator());
            if (not watchedPaths[numAllocatedPaths])
                return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                               FileSystemWatcherErrorDetail::AppleCreateWatchPathString);
            numAllocatedPaths++;
            if (numAllocatedPaths > ThreadRunnerDefinition::MaxWatchablePaths)
                return ResultFileSystemWatcher(FileSystemWatcherError::WatchLimitExceeded,
                                               FileSystemWatcherErrorDetail::WatchPathCapacity);
        }
        if (numAllocatedPaths == 0)
        {
            return ResultFileSystemWatcher(true);
        }
        pathsArray = CFArrayCreate(nullptr, reinterpret_cast<const void**>(watchedPaths),
                                   static_cast<CFIndex>(numAllocatedPaths), nullptr);
        if (not pathsArray)
        {
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                           FileSystemWatcherErrorDetail::AppleCreateWatchPathsArray);
        }
        deferDeletePaths.disarm();
        deferFreeMalloc.disarm();

        // Create Stream
        constexpr CFAbsoluteTime           watchLatency = 0.2;
        constexpr FSEventStreamCreateFlags watchFlags =
            kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer;
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

    static constexpr int EVENT_SYSTEM = kFSEventStreamEventFlagUserDropped |     //
                                        kFSEventStreamEventFlagKernelDropped |   //
                                        kFSEventStreamEventFlagEventIdsWrapped | //
                                        kFSEventStreamEventFlagHistoryDone |     //
                                        kFSEventStreamEventFlagMount |           //
                                        kFSEventStreamEventFlagUnmount |         //
                                        kFSEventStreamEventFlagRootChanged;

    static void threadOnNewFSEvent(ConstFSEventStreamRef          streamRef,  //
                                   void*                          info,       //
                                   size_t                         numEvents,  //
                                   void*                          eventPaths, //
                                   const FSEventStreamEventFlags* eventFlags, //
                                   const FSEventStreamEventId*    eventIds)
    {
        (void)(streamRef);
        (void)(eventIds);
        Internal&    internal = *reinterpret_cast<Internal*>(info);
        const char** paths    = reinterpret_cast<const char**>(eventPaths);
        for (size_t idx = 0; idx < numEvents; ++idx)
        {
            const FSEventStreamEventFlags flags = eventFlags[idx];
            if (flags & EVENT_SYSTEM)
                continue;

            const StringSpan path = StringSpan::fromNullTerminated(paths[idx], StringEncoding::Utf8);

            bool sendNotification = true;
            for (size_t prevIdx = 0; prevIdx < idx; ++prevIdx)
            {
                const StringSpan otherPath = StringSpan::fromNullTerminated(paths[prevIdx], StringEncoding::Utf8);
                if (path == otherPath)
                {
                    // Filter out multiple events for the same file in this batch
                    sendNotification = false;
                    break;
                }
            }
            if (sendNotification)
            {
                notify(path, internal, flags);
            }
            if (internal.closing.load())
            {
                break;
            }
        }
    }

    static void notify(const StringSpan path, Internal& internal, const FSEventStreamEventFlags flags)
    {
        if (internal.refreshing.load() or internal.closing.load())
        {
            return;
        }
        internal.notification.fullPath = path;

        const bool isDirectory = flags & kFSEventStreamEventFlagItemIsDir;
        const bool isRenamed   = flags & EVENT_RENAMED;
        const bool isModified  = flags & EVENT_MODIFIED;

        // FSEvent coalesces events in ways that makes it impossible to figure out exactly what happened
        // see https://github.com/atom/watcher/blob/master/docs/macos.md
        if (isRenamed)
        {
            internal.notification.operation = Operation::AddRemoveRename;
        }
        else
        {
            if (isModified or not isDirectory)
            {
                internal.notification.operation = Operation::Modified;
            }
            else
            {
                internal.notification.operation = Operation::AddRemoveRename;
            }
        }

        internal.mutex.lock();
        FolderWatcher* watcher = internal.self->watchers.front;
        internal.mutex.unlock();
        while (watcher != nullptr)
        {
            Span<const char> rootSpan;

            const bool sliceStart = path.toCharSpan().sliceStartLength(0, watcher->path.view().sizeInBytes(), rootSpan);
            internal.notification.basePath = {rootSpan, false, StringEncoding::Utf8};
            if (sliceStart and watcher->path.view() == internal.notification.basePath)
            {
                Span<const char> relativeSpan;
                (void)path.toCharSpan().sliceStartLength(
                    watcher->path.view().sizeInBytes(),
                    path.toCharSpan().sizeInBytes() - watcher->path.view().sizeInBytes(), relativeSpan);
                if (relativeSpan.data()[0] == '/')
                {
                    (void)relativeSpan.sliceStart(1, relativeSpan);
                }
                internal.notification.relativePath = {relativeSpan, true, path.getEncoding()};
                internal.notification.basePath     = watcher->path.view();

                if (internal.eventLoopRunner)
                {
                    EventLoopRunner& eventLoopRunner = *internal.eventLoopRunner;

                    internal.watcher = watcher;
                    const Result res = eventLoopRunner.appleWakeUpAndWait();
                    if (internal.closing.load() or internal.refreshing.load())
                    {
                        break;
                    }
                    if (not res)
                    {
                        // TODO: print error for wakeup
                    }
                }
                else
                {
                    watcher->notifyCallback(internal.notification);
                }
            }
            // TODO: If someone removes this watcher in the callback we will skip notifying remaining ones.
            internal.mutex.lock();
            watcher = watcher->next;
            internal.mutex.unlock();
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
struct SC::FileSystemWatcher::FolderWatcherInternal
{
};

void SC::FileSystemWatcher::asyncNotify(FolderWatcher*, size_t)
{
    Internal& impl = internal.get();
    if (not impl.refreshing.load() and not impl.closing.load() and impl.watcher->parent == this)
    {
        impl.watcher->notifyCallback(impl.notification);
    }
}
