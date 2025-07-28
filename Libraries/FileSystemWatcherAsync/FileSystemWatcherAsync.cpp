// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../FileSystemWatcherAsync/FileSystemWatcherAsync.h"
#include "../Foundation/Deferred.h"

#if SC_PLATFORM_WINDOWS
#include "../Async/Internal/AsyncWindows.h" // AsyncWinOverlapped
#endif

#if SC_PLATFORM_APPLE
SC::Result SC::FileSystemWatcherAsync::appleStartWakeUp()
{
    SC_TRY_MSG(eventLoop != nullptr and fileSystemWatcher != nullptr, "FileSystemWatcherAsync not initialized");
    AsyncLoopWakeUp& wakeUp = asyncWakeUp;
    wakeUp.callback.bind<FileSystemWatcherAsync, &FileSystemWatcherAsync::onEventLoopNotification>(*this);
    return wakeUp.start(*eventLoop, eventObject);
}

SC::Result SC::FileSystemWatcherAsync::appleWakeUpAndWait()
{
    const Result res = asyncWakeUp.wakeUp(*eventLoop);
    eventObject.wait();
    return res;
}

void SC::FileSystemWatcherAsync::onEventLoopNotification(AsyncLoopWakeUp::Result& result)
{
    fileSystemWatcher->asyncNotify(nullptr);
    result.reactivateRequest(true);
}

void SC::FileSystemWatcherAsync::appleSignalEventObject() { eventObject.signal(); }
#elif SC_PLATFORM_LINUX

SC::Result SC::FileSystemWatcherAsync::linuxStartSharedFilePoll()
{
    SC_TRY_MSG(eventLoop != nullptr and fileSystemWatcher != nullptr, "FileSystemWatcherAsync not initialized");
    FileDescriptor notifyHandle;
    SC_TRY(notifyHandle.assign(notifyFd))
    auto deferDetach = MakeDeferred([&notifyHandle] { notifyHandle.detach(); });
    SC_TRY(eventLoop->associateExternallyCreatedFileDescriptor(notifyHandle));
    asyncPoll.callback.bind<FileSystemWatcherAsync, &FileSystemWatcherAsync::onEventLoopNotification>(*this);
    return asyncPoll.start(*eventLoop, notifyFd);
}

void SC::FileSystemWatcherAsync::onEventLoopNotification(AsyncFilePoll::Result& result)
{
    fileSystemWatcher->asyncNotify(nullptr);
    result.reactivateRequest(true);
}

SC::Result SC::FileSystemWatcherAsync::linuxStopSharedFilePoll() { return asyncPoll.stop(*eventLoop); }

#else

SC::Result SC::FileSystemWatcherAsync::windowsStartFolderFilePoll(FolderWatcher& watcher, void* handle)
{
    SC_TRY_MSG(eventLoop != nullptr and fileSystemWatcher != nullptr, "FileSystemWatcherAsync not initialized");
    // TODO: Consider associating / removing file handle with IOCP directly inside AsyncFilePoll
    FileDescriptor fileHandle;
    SC_TRY(fileHandle.assign(handle));
    Result res = eventLoop->associateExternallyCreatedFileDescriptor(fileHandle);
    fileHandle.detach();
    SC_TRY(res);
    AsyncFilePoll& asyncPoll = watcher.asyncStorage.reinterpret_as<AsyncFilePoll>();
    placementNew(asyncPoll);
    asyncPoll.callback.bind<FileSystemWatcherAsync, &FileSystemWatcherAsync::onEventLoopNotification>(*this);
    return asyncPoll.start(*eventLoop, handle);
}

SC::Result SC::FileSystemWatcherAsync::windowsStopFolderFilePoll(FolderWatcher& watcher)
{
    // This is not strictly needed as file handle is being closed soon after anyway
    // SC_TRUST_RESULT(eventLoop->removeAllAssociationsFor(fwi.fileHandle));
    AsyncFilePoll& asyncPoll = watcher.asyncStorage.reinterpret_as<AsyncFilePoll>();

    onClose = [&watcher](AsyncResult&)
    {
        AsyncFilePoll& asyncPoll = watcher.asyncStorage.reinterpret_as<AsyncFilePoll>();
        asyncPoll.~AsyncFilePoll();
    };
    return asyncPoll.stop(*eventLoop, &onClose);
}

void* SC::FileSystemWatcherAsync::windowsGetOverlapped(FolderWatcher& watcher)
{
    AsyncFilePoll& asyncPoll = watcher.asyncStorage.reinterpret_as<AsyncFilePoll>();
    return &asyncPoll.getOverlappedOpaque().get().overlapped;
}

void SC::FileSystemWatcherAsync::onEventLoopNotification(AsyncFilePoll::Result& result)
{
    SC_COMPILER_WARNING_PUSH_OFFSETOF;
    auto&          storage = reinterpret_cast<decltype(FolderWatcher::asyncStorage)&>(result.getAsync());
    FolderWatcher& watcher = SC_COMPILER_FIELD_OFFSET(FolderWatcher, asyncStorage, storage);
    fileSystemWatcher->asyncNotify(&watcher);
    result.reactivateRequest(true);
    SC_COMPILER_WARNING_POP;
}

#endif
