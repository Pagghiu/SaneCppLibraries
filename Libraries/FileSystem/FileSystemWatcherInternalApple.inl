// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.

#include "../Async/EventLoop.h"
#include "../Foundation/String.h"
#include "../Foundation/StringConverter.h"
#include "../Threading/Threading.h"
#include <CoreServices/CoreServices.h> // FSEvents

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
    Action             pollingFunction;
    ReturnCode         signalReturnCode = false;
    EventObject        refreshSignalFinished;
    Mutex              mutex;
    EventLoopRunner*   eventLoopRunner = nullptr;

    // Used to pass data from thread to async callback
    Notification   notification;
    FolderWatcher* watcher;
    Atomic<bool>   closing = false;

    [[nodiscard]] ReturnCode init(FileSystemWatcher& parent, ThreadRunner& runner)
    {
        SC_UNUSED(runner);
        self = &parent;
        return true;
    }

    [[nodiscard]] ReturnCode init(FileSystemWatcher& parent, EventLoopRunner& runner)
    {
        self            = &parent;
        eventLoopRunner = &runner;
        auto& async     = eventLoopRunner->eventLoopAsync;
        async.callback.bind<Internal, &Internal::onMainLoop>(this);
        return async.start(eventLoopRunner->eventLoop, &eventLoopRunner->eventObject);
    }

    [[nodiscard]] ReturnCode initThread()
    {
        closing.exchange(false);
        // Create Signal to go from Loop --> CFRunLoop
        CFRunLoopSourceContext signalContext;
        memset(&signalContext, 0, sizeof(signalContext));

        signalContext.info    = this;
        signalContext.perform = &Internal::threadExecuteRefresh;
        refreshSignal         = CFRunLoopSourceCreate(nullptr, 0, &signalContext);
        SC_TRY_MSG(refreshSignal != nullptr, "CFRunLoopSourceCreate failed"_a8);

        // Create and start the thread
        pollingFunction.bind<Internal, &Internal::threadRun>(this);

        // Use the syncFunc that is guaranteed to run before start returns to obtain
        // the CFRunLoop allocated into that thread
        Action initFunction;
        initFunction.bind<Internal, &Internal::threadInit>(this);
        SC_TRY_IF(pollingThread.start("FileSystemWatcher::init", &pollingFunction, &initFunction));
        return true;
    }

    [[nodiscard]] ReturnCode close()
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
            SC_TRY_IF(pollingThread.join());
            releaseResources();
        }
        return true;
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
        refreshSignal   = nullptr;
        pollingFunction = Action();
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

    [[nodiscard]] ReturnCode threadCreateFSEvent()
    {
        SC_TRY_IF(runLoop);
        CFArrayRef   pathsArray   = nullptr;
        CFStringRef* watchedPaths = (CFStringRef*)malloc(sizeof(CFStringRef) * ThreadRunnerSizes::MaxWatchablePaths);
        SC_TRY_MSG(watchedPaths != nullptr, "Cannot allocate paths"_a8);
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
            StringNative<1024> buffer;
            StringConverter    converter(buffer);
            StringView         encodedPath;
            SC_TRY_IF(converter.convertNullTerminateFastPath(it->path->view(), encodedPath));
            watchedPaths[numAllocatedPaths] =
                CFStringCreateWithFileSystemRepresentation(nullptr, encodedPath.getNullTerminatedNative());
            if (not watchedPaths[numAllocatedPaths])
                return "CFStringCreateWithFileSystemRepresentation failed"_a8;
            numAllocatedPaths++;
            SC_TRY_MSG(numAllocatedPaths <= ThreadRunnerSizes::MaxWatchablePaths,
                       "Exceeded max size of 1024 paths to watch"_a8);
        }
        if (numAllocatedPaths == 0)
        {
            return true;
        }
        pathsArray = CFArrayCreate(nullptr, reinterpret_cast<const void**>(watchedPaths),
                                   static_cast<CFIndex>(numAllocatedPaths), nullptr);
        if (not pathsArray)
        {
            return "CFArrayCreate failed"_a8;
        }
        deferDeletePaths.disarm();
        deferFreeMalloc.disarm();

        // Create Stream
        constexpr CFAbsoluteTime           watchLatency = 0.5;
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
        SC_TRY_MSG(fsEventStream != nullptr, "FSEventStreamCreate failed"_a8);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        // Add it to runLoop
        FSEventStreamScheduleWithRunLoop(fsEventStream, runLoop, kCFRunLoopDefaultMode);
#pragma clang diagnostic pop

        if (not FSEventStreamStart(fsEventStream))
        {
            FSEventStreamInvalidate(fsEventStream);
            FSEventStreamRelease(fsEventStream);
            return "FSEventStreamStart failed"_a8;
        }
        return true;
    }

    void threadDestroyFSEvent()
    {
        FSEventStreamStop(fsEventStream);
        FSEventStreamInvalidate(fsEventStream);
        FSEventStreamRelease(fsEventStream);
        fsEventStream = nullptr;
    }

    [[nodiscard]] ReturnCode stopWatching(FolderWatcher& folderWatcher)
    {
        mutex.lock();
        folderWatcher.parent->watchers.remove(folderWatcher);
        folderWatcher.parent = nullptr;
        mutex.unlock();
        return startWatching(nullptr);
    }

    [[nodiscard]] ReturnCode startWatching(FolderWatcher*)
    {
        if (not pollingThread.wasStarted())
        {
            SC_TRY_IF(initThread());
        }
        wakeUpFSEventThread();
        return signalReturnCode;
    }

    static void threadOnNewFSEvent(ConstFSEventStreamRef          streamRef,  //
                                   void*                          info,       //
                                   size_t                         numEvents,  //
                                   void*                          eventPaths, //
                                   const FSEventStreamEventFlags* eventFlags, //
                                   const FSEventStreamEventId*    eventIds)
    {
        SC_UNUSED(streamRef);
        SC_UNUSED(eventIds);
        Internal&    internal = *reinterpret_cast<Internal*>(info);
        const char** paths    = reinterpret_cast<const char**>(eventPaths);
        for (size_t idx = 0; idx < numEvents; ++idx)
        {

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

            const FSEventStreamEventFlags flags = eventFlags[idx];
            if (flags & EVENT_SYSTEM)
                continue;

            const StringView path(paths[idx], strlen(paths[idx]), true, StringEncoding::Utf8);
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
                if (path.startsWith(watcher->path->view())) // TODO: This works only if encodings are the same
                {
                    internal.notification.basePath = watcher->path->view();
                    StringView relativePath        = path.sliceStartBytes(watcher->path->view().sizeInBytes());

                    // TODO: Refactor into a 'trimEnd'
                    while (relativePath.sizeInBytes() > 1 and relativePath.startsWithChar('/'))
                    {
                        // Remove initial '/'
                        relativePath = relativePath.sliceStart(1);
                    }
                    internal.notification.relativePath = relativePath;

                    if (internal.eventLoopRunner)
                    {
                        internal.watcher     = watcher;
                        const ReturnCode res = internal.eventLoopRunner->eventLoopAsync.wakeUp();
                        internal.eventLoopRunner->eventObject.wait();
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
                // TODO: This is not right. If someone removes this watcher in the callback we will skip notifying
                // remaining ones.
                internal.mutex.lock();
                watcher = watcher->next;
                internal.mutex.unlock();
            }
            if (internal.closing.load())
            {
                break;
            }
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

SC::ReturnCode SC::FileSystemWatcher::Notification::getFullPath(String&, StringView& view) const
{
    view = fullPath;
    return true;
}
