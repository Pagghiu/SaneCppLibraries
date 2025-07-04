// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../FileSystemWatcher/FileSystemWatcher.h"

#include "../../Async/Async.h"
#include "../../Foundation/Deferred.h"
#include "../../Threading/Threading.h"
#include <CoreServices/CoreServices.h> // FSEvents

#include <AvailabilityMacros.h>
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
// TODO: Figure out another API for ios as this is a private API and it will not be accepted on app store.
#include "FSEventsIOS.h"
#endif

struct SC::FileSystemWatcher::ThreadRunnerInternal
{
};
struct SC::FileSystemWatcher::FolderWatcherInternal
{
};

struct SC::FileSystemWatcher::Internal
{
    FileSystemWatcher* self          = nullptr;
    CFRunLoopRef       runLoop       = nullptr;
    CFRunLoopSourceRef refreshSignal = nullptr;
    FSEventStreamRef   fsEventStream = nullptr;
    Thread             pollingThread;
    Result             signalReturnCode = Result(false);
    EventObject        refreshSignalFinished;
    Mutex              mutex;
    EventLoopRunner*   eventLoopRunner = nullptr;

    // Used to pass data from thread to async callback
    Notification   notification;
    FolderWatcher* watcher;
    Atomic<bool>   closing = false;

    Result init(FileSystemWatcher& parent, ThreadRunner& runner)
    {
        SC_COMPILER_UNUSED(runner);
        self = &parent;
        return Result(true);
    }

    Result init(FileSystemWatcher& parent, EventLoopRunner& runner)
    {
        self            = &parent;
        eventLoopRunner = &runner;

        AsyncLoopWakeUp& wakeUp = eventLoopRunner->asyncWakeUp;
        wakeUp.callback.bind<Internal, &Internal::onMainLoop>(*this);
        return wakeUp.start(*eventLoopRunner->eventLoop, eventLoopRunner->eventObject);
    }

    Result initThread()
    {
        closing.exchange(false);
        // Create Signal to go from Loop --> CFRunLoop
        CFRunLoopSourceContext signalContext;
        memset(&signalContext, 0, sizeof(signalContext));

        signalContext.info    = this;
        signalContext.perform = &Internal::threadExecuteRefresh;
        refreshSignal         = CFRunLoopSourceCreate(nullptr, 0, &signalContext);
        SC_TRY_MSG(refreshSignal != nullptr, "CFRunLoopSourceCreate failed");

        EventObject eventObject;
        auto        pollingFunction = [&](Thread& thread)
        {
            thread.setThreadName("FileSystemWatcher::init");
            threadInit(); // Obtain the CFRunLoop for this thread
            eventObject.signal();
            threadRun();
        };
        SC_TRY(pollingThread.start(pollingFunction));
        eventObject.wait();
        return Result(true);
    }

    Result close()
    {
        if (pollingThread.wasStarted())
        {
            closing.exchange(true);
            if (eventLoopRunner)
            {
                eventLoopRunner->eventObject.signal();
            }

            // send close signal
            wakeUpFSEventThread();

            // Wait for thread to finish
            SC_TRY(pollingThread.join());
            releaseResources();
        }
        return Result(true);
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

    Result threadCreateFSEvent()
    {
        SC_TRY(runLoop);
        CFArrayRef   pathsArray = nullptr;
        CFStringRef* watchedPaths =
            (CFStringRef*)malloc(sizeof(CFStringRef) * ThreadRunnerDefinition::MaxWatchablePaths);
        SC_TRY_MSG(watchedPaths != nullptr, "Cannot allocate paths");
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
            watchedPaths[numAllocatedPaths] = CFStringCreateWithFileSystemRepresentation(nullptr, it->path.path);
            if (not watchedPaths[numAllocatedPaths])
                return Result::Error("CFStringCreateWithFileSystemRepresentation failed");
            numAllocatedPaths++;
            SC_TRY_MSG(numAllocatedPaths <= ThreadRunnerDefinition::MaxWatchablePaths,
                       "Exceeded max size of 1024 paths to watch");
        }
        if (numAllocatedPaths == 0)
        {
            return Result(true);
        }
        pathsArray = CFArrayCreate(nullptr, reinterpret_cast<const void**>(watchedPaths),
                                   static_cast<CFIndex>(numAllocatedPaths), nullptr);
        if (not pathsArray)
        {
            return Result::Error("CFArrayCreate failed");
        }
        deferDeletePaths.disarm();
        deferFreeMalloc.disarm();

        // Create Stream
        constexpr CFAbsoluteTime           watchLatency = 0.2;
        constexpr FSEventStreamCreateFlags watchFlags =
            kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer;
        FSEventStreamContext fsEventContext;
        memset(&fsEventContext, 0, sizeof(fsEventContext));
        fsEventContext.info = this;
        fsEventStream       = FSEventStreamCreate(nullptr,                       //
                                                  &Internal::threadOnNewFSEvent, //
                                                  &fsEventContext,               //
                                                  pathsArray,                    //
                                                  kFSEventStreamEventIdSinceNow, //
                                                  watchLatency,                  //
                                                  watchFlags);
        SC_TRY_MSG(fsEventStream != nullptr, "FSEventStreamCreate failed");

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
            return Result::Error("FSEventStreamStart failed");
        }
        return Result(true);
    }

    void threadDestroyFSEvent()
    {
        FSEventStreamStop(fsEventStream);
        FSEventStreamInvalidate(fsEventStream);
        FSEventStreamRelease(fsEventStream);
        fsEventStream = nullptr;
    }

    Result stopWatching(FolderWatcher& folderWatcher)
    {
        mutex.lock();
        folderWatcher.parent->watchers.remove(folderWatcher);
        folderWatcher.parent = nullptr;
        mutex.unlock();
        return startWatching(nullptr);
    }

    Result startWatching(FolderWatcher*)
    {
        if (not pollingThread.wasStarted())
        {
            SC_TRY(initThread());
        }
        wakeUpFSEventThread();
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
        SC_COMPILER_UNUSED(streamRef);
        SC_COMPILER_UNUSED(eventIds);
        Internal&    internal = *reinterpret_cast<Internal*>(info);
        const char** paths    = reinterpret_cast<const char**>(eventPaths);
        for (size_t idx = 0; idx < numEvents; ++idx)
        {
            const FSEventStreamEventFlags flags = eventFlags[idx];
            if (flags & EVENT_SYSTEM)
                continue;

            const StringViewData path = StringViewData::fromNullTerminated(paths[idx], StringEncoding::Utf8);

            bool sendNotification = true;
            for (size_t prevIdx = 0; prevIdx < idx; ++prevIdx)
            {
                const StringViewData otherPath =
                    StringViewData::fromNullTerminated(paths[prevIdx], StringEncoding::Utf8);
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

    static void notify(const StringViewData path, Internal& internal, const FSEventStreamEventFlags flags)
    {
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
                internal.notification.basePath     = watcher->path;

                if (internal.eventLoopRunner)
                {
                    EventLoopRunner& eventLoopRunner = *internal.eventLoopRunner;

                    internal.watcher = watcher;
                    const Result res = eventLoopRunner.asyncWakeUp.wakeUp(*eventLoopRunner.eventLoop);
                    eventLoopRunner.eventObject.wait();
                    if (internal.closing.load())
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

    void onMainLoop(AsyncLoopWakeUp::Result& result)
    {
        watcher->notifyCallback(notification);
        result.reactivateRequest(true);
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

SC::Result SC::FileSystemWatcher::Notification::getFullPath(StringPath& path) const
{
    return Result(path.assign(fullPath));
}
