// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Async/Async.h"
#include "../FileSystemWatcher/FileSystemWatcher.h"

namespace SC
{

//! @defgroup group_file_system_watcher_async FileSystem Watcher Async
//! @copybrief library_file_system_watcher_async (see @ref library_file_system_watcher_async for more details)

//! @addtogroup group_file_system_watcher_async
//! @{

/// @brief FileSystemWatcherAsync is an implementation of SC::FileSystemWatcher that uses SC::Async.
///
/// The main reason for this class to exist in a dedicated library is to break the dependency of
/// SC::FileSystemWatcher from SC::AsyncEventLoop.
///
/// Example:
/// \snippet Tests/Libraries/FileSystemWatcherAsync/FileSystemWatcherAsyncTest.cpp fileSystemWatcherAsyncSnippet
///
/// @note
/// This class has been designed to be implemented with SC::AsyncEventLoop but it's probably possible
/// implementing another backend using a different event loop library that is capable of providing
/// similar abstractions for file polling and event loop wake-up from a foreign thread.
struct FileSystemWatcherAsync : public FileSystemWatcher::EventLoopRunner
{
    void init(AsyncEventLoop& loop) { eventLoop = &loop; }

  protected:
    AsyncEventLoop* eventLoop = nullptr;

#if SC_PLATFORM_APPLE
    virtual Result appleStartWakeUp() override;
    virtual void   appleSignalEventObject() override;
    virtual Result appleWakeUpAndWait() override;

    void onEventLoopNotification(AsyncLoopWakeUp::Result& result);

    AsyncLoopWakeUp asyncWakeUp = {};
    EventObject     eventObject = {};
#elif SC_PLATFORM_LINUX
    virtual Result linuxStartSharedFilePoll() override;
    virtual Result linuxStopSharedFilePoll() override;

    void onEventLoopNotification(AsyncFilePoll::Result& result);

    AsyncFilePoll asyncPoll = {};

#else
    using FolderWatcher = FileSystemWatcher::FolderWatcher;
    virtual Result windowsStartFolderFilePoll(FolderWatcher& watcher, void* handle) override;
    virtual Result windowsStopFolderFilePoll(FolderWatcher& watcher) override;
    virtual void*  windowsGetOverlapped(FolderWatcher& watcher) override;

    void                         onEventLoopNotification(AsyncFilePoll::Result& result);
    Function<void(AsyncResult&)> onClose;
#endif
};
//! @}

} // namespace SC
