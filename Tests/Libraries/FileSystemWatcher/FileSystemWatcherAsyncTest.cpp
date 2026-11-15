// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Async/Async.h"
#include "Libraries/Common/Deferred.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/FileSystemWatcher/FileSystemWatcher.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/Console.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/Threading.h"

#if SC_PLATFORM_LINUX
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#elif SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace SC
{
struct FileSystemWatcherAsyncTest;
}

struct SC::FileSystemWatcherAsyncTest : public SC::TestCase
{
    FileSystemWatcherAsyncTest(SC::TestReport& report) : TestCase(report, "FileSystemWatcherAsyncTest")
    {
        using namespace SC;
        const StringView appDirectory = report.applicationRootDirectory.view();
        structuredNotInitializedError();
        eventLoopRejectedRoot(appDirectory);
        eventLoopSubdirectory(appDirectory);
        eventLoopWatchClose(appDirectory);
        eventLoopWaitTimeout();
        eventLoopWatchStop(appDirectory);
        eventLoopStopPendingNotification(appDirectory);
        eventLoopCloseMultipleWatchers(appDirectory);
        eventLoopOverlappingWatchers(appDirectory);
#if SC_PLATFORM_WINDOWS
        windowsCompletionCoverage(appDirectory);
#endif
#if SC_PLATFORM_APPLE
        appleRootChanged(appDirectory);
        appleRefreshOverlappingWatchers(appDirectory);
#endif
#if SC_PLATFORM_LINUX
        eventLoopWatchInvalidation(appDirectory);
        eventLoopOverflow(appDirectory);
        eventLoopLongNotificationPath();
        eventLoopReadFailure(appDirectory);
#endif
    }

    void structuredNotInitializedError()
    {
        if (not test_section("structured not-initialized error"))
            return;

        struct UninitializedRunner : FileSystemWatcherAsyncT<AsyncEventLoop>
        {
            Result startWithoutInitialization()
            {
#if SC_PLATFORM_APPLE
                return appleStartWakeUp();
#elif SC_PLATFORM_LINUX
                return linuxStartSharedFileReadiness();
#else
                FileSystemWatcher::FolderWatcher watcher;
                return windowsStartFolderExternalCompletion(watcher, nullptr);
#endif
            }
        } runner;

        const Result result = runner.startWithoutInitialization();
        SC_TEST_EXPECT(result.isError(FileSystemWatcherResultCategory, FileSystemWatcherError::NotInitialized));
    }

    void submitQueuedWatcher(AsyncEventLoop& eventLoop)
    {
        for (int idx = 0; idx < 8 and eventLoop.getNumberOfSubmittedRequests() > 0; ++idx)
        {
            SC_TEST_EXPECT(eventLoop.runNoWait());
        }
        SC_TEST_EXPECT(eventLoop.getNumberOfSubmittedRequests() == 0);
    }

    bool runUntil(AsyncEventLoop& eventLoop, Function<bool()> predicate, TimeMs timeoutDuration = TimeMs{5000})
    {
        bool             timedOut = false;
        AsyncLoopTimeout timeout;
        timeout.callback = [&timedOut](AsyncLoopTimeout::Result&) { timedOut = true; };
        SC_TEST_EXPECT(timeout.start(eventLoop, timeoutDuration));

        while (not predicate() and not timedOut)
        {
            SC_TEST_EXPECT(eventLoop.runOnce());
        }
        if (not timedOut)
        {
            SC_TEST_EXPECT(timeout.stop(eventLoop));
            SC_TEST_EXPECT(eventLoop.runNoWait());
            SC_TEST_EXPECT(timeout.isFree());
        }
        return predicate() and not timedOut;
    }

    void eventLoopWaitTimeout()
    {
        if (test_section("AsyncEventLoop wait timeout"))
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            SC_TEST_EXPECT(not runUntil(eventLoop, [] { return false; }, TimeMs{1}));
        }
    }

    void eventLoopRejectedRoot(const StringView appDirectory)
    {
        if (not test_section("AsyncEventLoop rejected root remains reusable"))
            return;

        FileSystem             fs;
        const ResultFileSystem fsResult = fs.init(appDirectory);
        SC_TEST_EXPECT(fsResult);
        if (not fsResult)
            return;
        StringPath directoryName;
        bool       created = false;
        for (size_t attempt = 0; attempt < 128 and not created; ++attempt)
        {
            if (not StringBuilder::format(directoryName, "__sc watcher async root {}", attempt))
                break;
            created = static_cast<bool>(fs.makeDirectory(directoryName.view()));
        }
        SC_TEST_EXPECT(created);
        if (not created)
            return;
        auto       removeDirectory = MakeDeferred([&] { (void)fs.removeDirectoryRecursive(directoryName.view()); });
        StringPath directoryPath;
        StringPath missingPath;
        const bool pathsReady = Path::join(directoryPath, {appDirectory, directoryName.view()}) and
                                Path::join(missingPath, {directoryPath.view(), "missing"});
        SC_TEST_EXPECT(pathsReady);
        if (not pathsReady)
            return;

        AsyncEventLoop eventLoop;
        const Result   createdLoop = eventLoop.create();
        SC_TEST_EXPECT(createdLoop);
        if (not createdLoop)
            return;
        FileSystemWatcherAsyncT<AsyncEventLoop> runner;
        runner.init(eventLoop);
        FileSystemWatcher             fileEventsWatcher;
        const ResultFileSystemWatcher initialized = fileEventsWatcher.init(runner);
        SC_TEST_EXPECT(initialized);
        if (not initialized)
            return;
        FileSystemWatcher::FolderWatcher watcher;
        watcher.notifyCallback                     = [](const FileSystemWatcher::Notification&) {};
        auto                          closeWatcher = MakeDeferred([&] { (void)fileEventsWatcher.close(); });
        const ResultFileSystemWatcher missing      = fileEventsWatcher.watch(watcher, missingPath.view());
        SC_TEST_EXPECT(missing.isError(FileSystemWatcherError::WatchSetupFailed));
        SC_TEST_EXPECT(watcher.stopWatching().isError(FileSystemWatcherError::NotWatching));
        StringPath childPath;
        SC_TEST_EXPECT(Path::join(childPath, {directoryPath.view(), "child"}));
        SC_TEST_EXPECT(fs.makeDirectory(childPath.view()));
        SC_TEST_EXPECT(fileEventsWatcher.watch(watcher, directoryPath.view()));
        SC_TEST_EXPECT(fs.removeDirectoryRecursive(childPath.view()));
        const ResultFileSystemWatcher stopped = watcher.stopWatching();
        SC_TEST_EXPECT(stopped);
        if (stopped)
            SC_TEST_EXPECT(fileEventsWatcher.watch(watcher, directoryPath.view()));
        submitQueuedWatcher(eventLoop);
        const ResultFileSystemWatcher closed = fileEventsWatcher.close();
        SC_TEST_EXPECT(closed);
        if (closed)
            closeWatcher.disarm();
        const ResultFileSystem removed = fs.removeDirectoryRecursive(directoryName.view());
        SC_TEST_EXPECT(removed);
        if (removed)
            removeDirectory.disarm();
    }

    void eventLoopSubdirectory(const StringView appDirectory)
    {
        if (test_section("AsyncEventLoop"))
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            FileSystemWatcher fileEventsWatcher;

            FileSystemWatcherAsyncT<AsyncEventLoop> runner;
            runner.init(eventLoop);
            SC_TEST_EXPECT(fileEventsWatcher.init(runner));

            struct Params
            {
                uint64_t callbackThreadID       = 0;
                uint64_t addRemoveRenameChanges = 0;
                uint64_t modifiedChanges        = 0;
                uint64_t observedChanges        = 0;
                uint64_t matchingChanges        = 0;

                StringPath lastRelativePath;

                StringView appDirectory;

                SmallStringNative<255>  expectedRelativePath = StringEncoding::Native;
                SmallStringNative<1024> expectedFullPath     = StringEncoding::Native;
            } params;
            params.appDirectory = appDirectory;

            constexpr native_char_t nativeSep = Path::Separator;
            SC_TEST_EXPECT(StringBuilder::format(params.expectedRelativePath, "{}{}{}{}{}", "dir", nativeSep, "subdir2",
                                                 nativeSep, "test.txt"));
            SC_TEST_EXPECT(StringBuilder::format(params.expectedFullPath, "{}{}{}", params.appDirectory, nativeSep,
                                                 params.expectedRelativePath.view()));

            auto lambda = [&](const FileSystemWatcher::Notification& notification)
            {
                ++params.observedChanges;
                SC_TEST_EXPECT(params.lastRelativePath.assign(notification.relativePath));
                if (notification.relativePath != params.expectedRelativePath.view())
                {
                    return;
                }

                ++params.matchingChanges;
                params.callbackThreadID = Thread::CurrentThreadID();
                if (notification.operation == FileSystemWatcher::Operation::AddRemoveRename)
                {
                    params.addRemoveRenameChanges++;
                }
                else
                {
                    SC_TEST_EXPECT(notification.operation == FileSystemWatcher::Operation::Modified);
                    params.modifiedChanges++;
                }
                SC_TEST_EXPECT(params.appDirectory == notification.basePath);

                StringPath fullPath;
                SC_TEST_EXPECT(notification.getFullPath(fullPath));
                SC_TEST_EXPECT(fullPath.view() == params.expectedFullPath.view());
            };

            FileSystem fs;
            SC_TEST_EXPECT(fs.init(appDirectory));
            SC_TEST_EXPECT(fs.makeDirectoryIfNotExists({"dir"}));
            SC_TEST_EXPECT(fs.makeDirectoryIfNotExists({"dir/subdir1"}));
            SC_TEST_EXPECT(fs.makeDirectoryIfNotExists({"dir/subdir2"}));
            SC_TEST_EXPECT(fs.makeDirectoryIfNotExists({"dir2"}));
            if (fs.existsAndIsFile("dir/subdir2/test.txt"))
            {
                SC_TEST_EXPECT(fs.removeFile("dir/subdir2/test.txt"));
            }

            SmallStringNative<1024> path;
            SC_TEST_EXPECT(path.assign(appDirectory));
            FileSystemWatcher::FolderWatcher watcher;
            watcher.notifyCallback = lambda;
            SC_TEST_EXPECT(fileEventsWatcher.watch(watcher, path.view()));
            submitQueuedWatcher(eventLoop);
            SC_TEST_EXPECT(fs.write("dir/subdir2/test.txt", "content"));
            const bool notified = runUntil(eventLoop, [&] { return params.addRemoveRenameChanges > 0; });
            if (not notified)
            {
                report.console.print("Watcher timeout: observed={}, matching={}\n", size_t(params.observedChanges),
                                     size_t(params.matchingChanges));
                report.console.print("modified={}\n", size_t(params.modifiedChanges));
                report.console.print("expected={}, last={}\n", params.expectedRelativePath.view(),
                                     params.lastRelativePath.view());
            }
            SC_TEST_EXPECT(notified);
            SC_TEST_EXPECT(fileEventsWatcher.close());
            SC_TEST_EXPECT(params.callbackThreadID == Thread::CurrentThreadID());
            SC_TEST_EXPECT(fs.removeFile({"dir/subdir2/test.txt"_a8}));
            SC_TEST_EXPECT(fs.removeEmptyDirectories({"dir/subdir1", "dir/subdir2", "dir", "dir2"}));
#if SC_PLATFORM_WINDOWS
            // We need sleep otherwise windows ReadDirectoryChangesW on the same directory
            // will report events for the two deletions above in the next test even
            // if we've just closed its handle with CloseHandle and issued a CancelIO! :-|
            Thread::Sleep(100);
#endif
        }
    }

    void eventLoopWatchClose(const StringView appDirectory)
    {
        if (test_section("AsyncEventLoop close"))
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            FileSystemWatcher fileEventsWatcher;
            FileSystem        fs;
            SC_TEST_EXPECT(fs.init(appDirectory));

            FileSystemWatcherAsyncT<AsyncEventLoop> runner;
            runner.init(eventLoop);
            SC_TEST_EXPECT(fileEventsWatcher.init(runner));
            SmallStringNative<1024> path;
            SC_TEST_EXPECT(path.assign(appDirectory));
            FileSystemWatcher::FolderWatcher watcher;
            struct Params
            {
                int changes = 0;
            } params;
            watcher.notifyCallback = [&](const FileSystemWatcher::Notification&) { params.changes++; };
            SC_TEST_EXPECT(fileEventsWatcher.watch(watcher, path.view()));
            submitQueuedWatcher(eventLoop);
            SC_TEST_EXPECT(fs.write("salve2.txt", "content"));
            SC_TEST_EXPECT(fs.write("a_tutti2.txt", "content"));
            // On different OS and FileSystems it's possible to get completely random number of changes
            SC_TEST_EXPECT(runUntil(eventLoop, [&] { return params.changes > 0; }));
            SC_TEST_EXPECT(fileEventsWatcher.close());
            SC_TEST_EXPECT(fs.removeFiles({"salve2.txt", "a_tutti2.txt"}));
        }
    }

    void eventLoopWatchStop(const StringView appDirectory)
    {
        if (test_section("AsyncEventLoop watch/stopWatching"))
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            FileSystemWatcherAsyncT<AsyncEventLoop> runner;
            runner.init(eventLoop);
            FileSystemWatcher fileEventsWatcher;
            SC_TEST_EXPECT(fileEventsWatcher.init(runner));
            SmallStringNative<1024> path1, path2;
            SC_TEST_EXPECT(Path::join(path1, {appDirectory, "__test1"}));
            SC_TEST_EXPECT(Path::join(path2, {appDirectory, "__test2"}));
            FileSystem fs;
            SC_TEST_EXPECT(fs.init(appDirectory));
            if (fs.existsAndIsDirectory(path1.view()))
            {
                SC_TEST_EXPECT(fs.removeDirectoryRecursive(path1.view()));
            }
            if (fs.existsAndIsDirectory(path2.view()))
            {
                SC_TEST_EXPECT(fs.removeDirectoryRecursive(path2.view()));
            }
            SC_TEST_EXPECT(fs.makeDirectory(path1.view()));
            SC_TEST_EXPECT(fs.makeDirectory(path2.view()));
            FileSystemWatcher::FolderWatcher watcher1, watcher2;
            watcher1.setDebugName("Watcher1");
            watcher2.setDebugName("Watcher2");
            struct Params
            {
                int addRemoveRename1 = 0;
                int addRemoveRename2 = 0;
                int modified1        = 0;
                int modified2        = 0;
            } params;
            auto lambda1 = [&](const FileSystemWatcher::Notification& notification)
            {
                if (notification.relativePath == "salve.txt")
                {
                    if (notification.operation == FileSystemWatcher::Operation::AddRemoveRename)
                    {
                        params.addRemoveRename1++;
                    }
                    else
                    {
                        SC_TEST_EXPECT(notification.operation == FileSystemWatcher::Operation::Modified);
                        params.modified1++;
                    }
                }
            };
            watcher1.notifyCallback = lambda1;
            SC_TEST_EXPECT(fileEventsWatcher.watch(watcher1, path1.view()));
            auto lambda2 = [&](const FileSystemWatcher::Notification& notification)
            {
                if (notification.relativePath == "a_tutti.txt")
                {
                    if (notification.operation == FileSystemWatcher::Operation::AddRemoveRename)
                    {
                        params.addRemoveRename2++;
                    }
                    else
                    {
                        SC_TEST_EXPECT(notification.operation == FileSystemWatcher::Operation::Modified);
                        params.modified2++;
                    }
                }
            };
            watcher2.notifyCallback = lambda2;
            SC_TEST_EXPECT(fileEventsWatcher.watch(watcher2, path2.view()));
            SC_TEST_EXPECT(eventLoop.runNoWait());
            FileSystem fs1;
            FileSystem fs2;
            SC_TEST_EXPECT(fs1.init(path1.view()));
            SC_TEST_EXPECT(fs2.init(path2.view()));

            SC_TEST_EXPECT(fs1.write("salve.txt", "content"));
            SC_TEST_EXPECT(runUntil(eventLoop, [&] { return params.addRemoveRename1 > 0; }));
            SC_TEST_EXPECT(fs2.write("a_tutti.txt", "content"));
            SC_TEST_EXPECT(runUntil(eventLoop, [&] { return params.addRemoveRename2 > 0; }));
            SC_TEST_EXPECT(eventLoop.runNoWait());
            SC_TEST_EXPECT(watcher2.stopWatching());
            const int addRemoveRename1BeforeRemove = params.addRemoveRename1;
            const int addRemoveRename2AfterStop    = params.addRemoveRename2;
            SC_TEST_EXPECT(fs1.removeFile("salve.txt"));
            SC_TEST_EXPECT(fs2.removeFile("a_tutti.txt"));

            SC_TEST_EXPECT(runUntil(eventLoop, [&] { return params.addRemoveRename1 > addRemoveRename1BeforeRemove; }));
            SC_TEST_EXPECT(params.addRemoveRename2 == addRemoveRename2AfterStop);
            SC_TEST_EXPECT(watcher1.stopWatching());
            const int addRemoveRename1AfterStop = params.addRemoveRename1;
            SC_TEST_EXPECT(fs1.write("salve.txt", "content NEW YEAH"));
            SC_TEST_EXPECT(fs2.write("a_tutti.txt", "content NEW YEAH"));

            SC_TEST_EXPECT(eventLoop.runNoWait());
            SC_TEST_EXPECT(params.addRemoveRename1 == addRemoveRename1AfterStop);
            SC_TEST_EXPECT(params.addRemoveRename2 == addRemoveRename2AfterStop);

            auto lambda3 = [&](const FileSystemWatcher::Notification& notification)
            {
                if (notification.relativePath == "a_tutti.txt")
                {
                    if (notification.operation == FileSystemWatcher::Operation::AddRemoveRename)
                    {
                        params.addRemoveRename2++;
                    }
                    else
                    {
                        SC_TEST_EXPECT(notification.operation == FileSystemWatcher::Operation::Modified);
                        params.modified2++;
                    }
                }
            };
            watcher2.notifyCallback = lambda3;
            SC_TEST_EXPECT(fileEventsWatcher.watch(watcher2, path2.view()));
            SC_TEST_EXPECT(eventLoop.runNoWait());
            SC_TEST_EXPECT(fs2.removeFile("a_tutti.txt"));
            SC_TEST_EXPECT(runUntil(eventLoop, [&] { return params.addRemoveRename2 > addRemoveRename2AfterStop; }));
            SC_TEST_EXPECT(params.addRemoveRename1 == addRemoveRename1AfterStop);

            SC_TEST_EXPECT(fileEventsWatcher.close());
            SC_TEST_EXPECT(fs1.removeFile("salve.txt"));
            SC_TEST_EXPECT(fs.removeEmptyDirectory(path1.view()));
            SC_TEST_EXPECT(fs.removeEmptyDirectory(path2.view()));
        }
    }

    void eventLoopStopPendingNotification(const StringView appDirectory)
    {
#if SC_PLATFORM_APPLE
        if (not test_section("AsyncEventLoop stop with pending notification"))
            return;

        struct Runner : FileSystemWatcherAsyncT<AsyncEventLoop>
        {
            Atomic<bool> notificationEntered = false;

            Result appleWakeUpAndWait() override
            {
                notificationEntered.store(true);
                return FileSystemWatcherAsyncT<AsyncEventLoop>::appleWakeUpAndWait();
            }
        } runner;
        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());
        runner.init(eventLoop);
        FileSystemWatcher fileEventsWatcher;
        SC_TEST_EXPECT(fileEventsWatcher.init(runner));
        FileSystem fs;
        SC_TEST_EXPECT(fs.init(appDirectory));
        SC_TEST_EXPECT(fs.makeDirectoryIfNotExists({"__pending_notification"}));
        StringPath path;
        SC_TEST_EXPECT(Path::join(path, {appDirectory, "__pending_notification"}));
        FileSystemWatcher::FolderWatcher watcher;
        int                              callbacks = 0;
        watcher.notifyCallback                     = [&](const FileSystemWatcher::Notification&) { callbacks++; };
        SC_TEST_EXPECT(fileEventsWatcher.watch(watcher, path.view()));
        submitQueuedWatcher(eventLoop);
        SC_TEST_EXPECT(fs.write("__pending_notification/test.txt", "content"));
        for (int idx = 0; idx < 5000 and not runner.notificationEntered.load(); ++idx)
        {
            Thread::Sleep(1);
        }
        SC_TEST_EXPECT(runner.notificationEntered.load());
        SC_TEST_EXPECT(watcher.stopWatching());
        SC_TEST_EXPECT(eventLoop.runNoWait());
        SC_TEST_EXPECT(callbacks == 0);

        runner.notificationEntered.store(false);
        SC_TEST_EXPECT(fileEventsWatcher.watch(watcher, path.view()));
        submitQueuedWatcher(eventLoop);
        SC_TEST_EXPECT(fs.write("__pending_notification/rewatched.txt", "content"));
        SC_TEST_EXPECT(runUntil(eventLoop, [&] { return callbacks > 0; }));
        struct CallbackStop
        {
            FileSystemWatcher::FolderWatcher& watcher;

            int& callbacks;
            bool stopped = false;
        } callbackStop{watcher, callbacks};
        watcher.notifyCallback = [this, &callbackStop](const FileSystemWatcher::Notification&)
        {
            callbackStop.callbacks++;
            SC_TEST_EXPECT(callbackStop.watcher.stopWatching());
            callbackStop.stopped = true;
        };
        SC_TEST_EXPECT(fs.write("__pending_notification/callback_stop.txt", "content"));
        SC_TEST_EXPECT(runUntil(eventLoop, [&] { return callbackStop.stopped; }));
        watcher.notifyCallback = [&](const FileSystemWatcher::Notification&) { callbacks++; };
        SC_TEST_EXPECT(fileEventsWatcher.watch(watcher, path.view()));
        submitQueuedWatcher(eventLoop);
        const int callbacksBeforeRestart = callbacks;
        SC_TEST_EXPECT(fs.write("__pending_notification/restarted.txt", "content"));
        SC_TEST_EXPECT(runUntil(eventLoop, [&] { return callbacks > callbacksBeforeRestart; }));
        const int callbacksBeforeClose = callbacks;
        runner.notificationEntered.store(false);
        SC_TEST_EXPECT(fs.write("__pending_notification/close.txt", "content"));
        for (int idx = 0; idx < 5000 and not runner.notificationEntered.load(); ++idx)
        {
            Thread::Sleep(1);
        }
        SC_TEST_EXPECT(runner.notificationEntered.load());
        SC_TEST_EXPECT(fileEventsWatcher.close());
        SC_TEST_EXPECT(eventLoop.runNoWait());
        SC_TEST_EXPECT(callbacks == callbacksBeforeClose);
        SC_TEST_EXPECT(eventLoop.close());
        SC_TEST_EXPECT(fs.removeFiles({"__pending_notification/test.txt", "__pending_notification/rewatched.txt",
                                       "__pending_notification/callback_stop.txt",
                                       "__pending_notification/restarted.txt", "__pending_notification/close.txt"}));
        SC_TEST_EXPECT(fs.removeEmptyDirectory("__pending_notification"));
#else
        (void)appDirectory;
#endif
    }

    void eventLoopCloseMultipleWatchers(const StringView appDirectory)
    {
        if (test_section("AsyncEventLoop close multiple watchers"))
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            FileSystemWatcherAsyncT<AsyncEventLoop> runner;
            runner.init(eventLoop);
            FileSystemWatcher fileEventsWatcher;
            SC_TEST_EXPECT(fileEventsWatcher.init(runner));

            SmallStringNative<1024> path1, path2;
            SC_TEST_EXPECT(Path::join(path1, {appDirectory, "__close_multiple_1"}));
            SC_TEST_EXPECT(Path::join(path2, {appDirectory, "__close_multiple_2"}));

            FileSystem fs;
            SC_TEST_EXPECT(fs.init(appDirectory));
            if (fs.existsAndIsDirectory(path1.view()))
            {
                SC_TEST_EXPECT(fs.removeDirectoryRecursive(path1.view()));
            }
            if (fs.existsAndIsDirectory(path2.view()))
            {
                SC_TEST_EXPECT(fs.removeDirectoryRecursive(path2.view()));
            }
            SC_TEST_EXPECT(fs.makeDirectory(path1.view()));
            SC_TEST_EXPECT(fs.makeDirectory(path2.view()));

            FileSystemWatcher::FolderWatcher watcher1, watcher2;
            watcher1.notifyCallback = [](const FileSystemWatcher::Notification&) {};
            watcher2.notifyCallback = [](const FileSystemWatcher::Notification&) {};
            SC_TEST_EXPECT(fileEventsWatcher.watch(watcher1, path1.view()));
            SC_TEST_EXPECT(fileEventsWatcher.watch(watcher2, path2.view()));
            submitQueuedWatcher(eventLoop);

            SC_TEST_EXPECT(fileEventsWatcher.close());
            SC_TEST_EXPECT(eventLoop.close());

            SC_TEST_EXPECT(fs.removeEmptyDirectory(path1.view()));
            SC_TEST_EXPECT(fs.removeEmptyDirectory(path2.view()));
        }
    }

    void eventLoopOverlappingWatchers(const StringView appDirectory)
    {
        if (not test_section("AsyncEventLoop overlapping watchers"))
            return;

        FileSystem             fs;
        const ResultFileSystem fsReady = fs.init(appDirectory);
        SC_TEST_EXPECT(fsReady);
        if (not fsReady)
            return;
        StringPath directoryName;
        bool       created = false;
        for (size_t attempt = 0; attempt < 128 and not created; ++attempt)
        {
            if (not StringBuilder::format(directoryName, "__sc overlap {}", attempt))
                break;
            created = static_cast<bool>(fs.makeDirectory(directoryName.view()));
        }
        SC_TEST_EXPECT(created);
        if (not created)
            return;
        auto       removeDirectory = MakeDeferred([&] { (void)fs.removeDirectoryRecursive(directoryName.view()); });
        StringPath directoryPath;
        StringPath childPath;
        StringPath grandchildPath;
        const bool pathsReady = Path::join(directoryPath, {appDirectory, directoryName.view()}) and
                                Path::join(childPath, {directoryPath.view(), "child"}) and
                                Path::join(grandchildPath, {childPath.view(), "grandchild"}) and
                                fs.makeDirectory(childPath.view()) and fs.makeDirectory(grandchildPath.view());
        SC_TEST_EXPECT(pathsReady);
        if (not pathsReady)
            return;

        AsyncEventLoop eventLoop;
        const Result   loopReady = eventLoop.create();
        SC_TEST_EXPECT(loopReady);
        if (not loopReady)
            return;
        FileSystemWatcherAsyncT<AsyncEventLoop> runner;
        runner.init(eventLoop);
        FileSystemWatcher fileEventsWatcher;

        struct OverlapState
        {
            StringView expected;
            unsigned   observed = 0;
            unsigned   required = 0;
        } state;
        FileSystemWatcher::FolderWatcher watcher1, watcher2;
        watcher1.notifyCallback = [&state](const FileSystemWatcher::Notification& notification)
        {
            const StringView path = notification.relativePath;
            if (path.endsWith(state.expected))
                state.observed |= 1;
        };
        watcher2.notifyCallback = [&state](const FileSystemWatcher::Notification& notification)
        {
            const StringView path = notification.relativePath;
            if (path.endsWith(state.expected))
                state.observed |= 2;
        };

        auto closeLoop    = MakeDeferred([&] { (void)eventLoop.close(); });
        auto closeWatcher = MakeDeferred([&] { (void)fileEventsWatcher.close(); });

        const ResultFileSystemWatcher watcherReady = fileEventsWatcher.init(runner);
        SC_TEST_EXPECT(watcherReady);
        if (not watcherReady)
            return;

        auto writeFile = [&](StringView parent, StringView name)
        {
            StringPath path;
            return Path::join(path, {parent, name}) and fs.writeString(path.view(), "content");
        };
        auto writeAndObserve = [&](StringView parent, StringView name, unsigned required)
        {
            state.expected = name;
            state.observed = 0;
            state.required = required;
            return writeFile(parent, name) and
                   runUntil(eventLoop, [&state] { return (state.observed & state.required) == state.required; });
        };
        constexpr unsigned firstWatcher  = 1;
        constexpr unsigned secondWatcher = 2;
        constexpr unsigned bothWatchers  = firstWatcher | secondWatcher;

        SC_TEST_EXPECT(writeFile(directoryPath.view(), "modify-only.txt"));
        SC_TEST_EXPECT(fileEventsWatcher.watch(watcher1, directoryPath.view()));
        SC_TEST_EXPECT(fileEventsWatcher.watch(watcher2, directoryPath.view()));
        submitQueuedWatcher(eventLoop);
        SC_TEST_EXPECT(writeAndObserve(directoryPath.view(), "modify-only.txt", bothWatchers));
#if SC_PLATFORM_LINUX
        char                             limitedPaths[8];
        FileSystemWatcher::FolderWatcher limitedWatcher(limitedPaths);
        limitedWatcher.notifyCallback                = [](const FileSystemWatcher::Notification&){};
        const ResultFileSystemWatcher limitedFailure = fileEventsWatcher.watch(limitedWatcher, directoryPath.view());
        SC_TEST_EXPECT(limitedFailure.isError(FileSystemWatcherError::BufferTooSmall));
        SC_TEST_EXPECT(limitedFailure.detail == FileSystemWatcherErrorDetail::LinuxRelativePathStorage);
        SC_TEST_EXPECT(limitedWatcher.stopWatching().isError(FileSystemWatcherError::NotWatching));
        SC_TEST_EXPECT(writeAndObserve(directoryPath.view(), "rollback-root.txt", bothWatchers));
        SC_TEST_EXPECT(writeAndObserve(childPath.view(), "rollback-child.txt", bothWatchers));
#endif
        SC_TEST_EXPECT(writeAndObserve(directoryPath.view(), "same-first.txt", bothWatchers));
        SC_TEST_EXPECT(watcher1.stopWatching());
        SC_TEST_EXPECT(writeAndObserve(directoryPath.view(), "same-second.txt", secondWatcher));

        SC_TEST_EXPECT(fileEventsWatcher.watch(watcher1, directoryPath.view()));
        SC_TEST_EXPECT(watcher2.stopWatching());
        submitQueuedWatcher(eventLoop);
        SC_TEST_EXPECT(writeAndObserve(directoryPath.view(), "same-third.txt", firstWatcher));

        SC_TEST_EXPECT(fileEventsWatcher.watch(watcher2, childPath.view()));
        submitQueuedWatcher(eventLoop);
        SC_TEST_EXPECT(writeAndObserve(childPath.view(), "child-first.txt", bothWatchers));
        SC_TEST_EXPECT(watcher1.stopWatching());
        SC_TEST_EXPECT(writeAndObserve(childPath.view(), "child-second.txt", secondWatcher));
        SC_TEST_EXPECT(fileEventsWatcher.watch(watcher1, directoryPath.view()));
        SC_TEST_EXPECT(watcher2.stopWatching());
        submitQueuedWatcher(eventLoop);
        SC_TEST_EXPECT(writeAndObserve(childPath.view(), "child-third.txt", firstWatcher));

        const ResultFileSystemWatcher closed = fileEventsWatcher.close();
        SC_TEST_EXPECT(closed);
        if (not closed)
            return;
        closeWatcher.disarm();
        const Result loopClosed = eventLoop.close();
        SC_TEST_EXPECT(loopClosed);
        if (not loopClosed)
            return;
        closeLoop.disarm();
        const ResultFileSystem removed = fs.removeDirectoryRecursive(directoryName.view());
        SC_TEST_EXPECT(removed);
        if (removed)
            removeDirectory.disarm();
    }

#if SC_PLATFORM_WINDOWS
    void windowsCompletionCoverage(StringView appDirectory)
    {
        if (not test_section("AsyncEventLoop Windows completion coverage"))
            return;

        FileSystem fs;
        SC_TEST_EXPECT(fs.init(appDirectory));
        StringPath fixture;
        bool       created = false;
        for (size_t attempt = 0; attempt < 128 and not created; ++attempt)
        {
            if (not StringBuilder::format(fixture, "__sc windows coverage {}", attempt))
                break;
            created = static_cast<bool>(fs.makeDirectory(fixture.view()));
        }
        SC_TEST_EXPECT(created);
        if (not created)
            return;
        auto       cleanup = MakeDeferred([&] { (void)fs.removeDirectoryRecursive(fixture.view()); });
        StringPath root;
        SC_TEST_EXPECT(Path::join(root, {appDirectory, fixture.view()}));

        struct Runner : FileSystemWatcherAsyncT<AsyncEventLoop>
        {
            HANDLE                  directory        = INVALID_HANDLE_VALUE;
            bool                    rejectSubmission = false;
            static constexpr Result rejected() { return Result::Error(AsyncResultCategory, AsyncError::RequestInUse); }

            Result windowsStartFolderExternalCompletion(FileSystemWatcher::FolderWatcher& watcher,
                                                        void*                             handle) override
            {
                directory = handle;
                return FileSystemWatcherAsyncT::windowsStartFolderExternalCompletion(watcher, handle);
            }

            Result windowsMarkFolderExternalCompletionPending(FileSystemWatcher::FolderWatcher& watcher) override
            {
                if (rejectSubmission)
                    return rejected();
                return FileSystemWatcherAsyncT::windowsMarkFolderExternalCompletionPending(watcher);
            }
        } runner;
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());
        runner.init(loop);
        FileSystemWatcher                owner;
        FileSystemWatcher::FolderWatcher watch;
        auto                             closeOwner = MakeDeferred([&] { (void)owner.close(); });
        SC_TEST_EXPECT(owner.init(runner));
        AsyncEventLoop uninitializedLoop;
        runner.init(uninitializedLoop);
        const auto registration = owner.watch(watch, root.view());
        SC_TEST_EXPECT(registration.result.isError(AsyncResultCategory, AsyncError::NotInitialized));
        SC_TEST_EXPECT(watch.stopWatching().isError(FileSystemWatcherError::NotWatching));
        runner.init(loop);

        runner.rejectSubmission = true;
        const auto submission   = owner.watch(watch, root.view());
        SC_TEST_EXPECT(submission.result.isError(AsyncResultCategory, AsyncError::RequestInUse));
        SC_TEST_EXPECT(watch.stopWatching().isError(FileSystemWatcherError::NotWatching));
        SC_TEST_EXPECT(loop.getNumberOfSubmittedRequests() == 0);
        runner.rejectSubmission = false;

        struct State
        {
            size_t rescans         = 0;
            size_t changes         = 0;
            size_t stopped         = 0;
            bool   overflowContext = true;
            bool   cancelled       = false;
            bool   failedRearm     = false;
            bool   marker          = false;
        } state;
        using Kind           = FileSystemWatcher::NotificationKind;
        watch.notifyCallback = [&state](const FileSystemWatcher::Notification& event)
        {
            if (event.kind == Kind::RescanRequired)
            {
                ++state.rescans;
                state.overflowContext = state.overflowContext and event.failure and event.relativePath.isEmpty();
            }
            else if (event.kind == Kind::WatchStopped)
            {
                ++state.stopped;
                state.cancelled |=
                    event.failure.isError(FileSystemWatcherError::MonitoringFailed) and
                    event.failure.detail == FileSystemWatcherErrorDetail::WindowsCompleteDirectoryChanges and
                    event.failure.nativeError == ERROR_OPERATION_ABORTED and event.relativePath.isEmpty();
                state.failedRearm |= event.failure.result.isError(AsyncResultCategory, AsyncError::RequestInUse);
            }
            else
            {
                ++state.changes;
                state.marker |= StringView(event.relativePath) == "coverage-marker.txt";
            }
        };
        SC_TEST_EXPECT(owner.watch(watch, root.view()));
        submitQueuedWatcher(loop);

        // The first completion occupies the outstanding read while the remaining events fill the kernel buffer.
        StringPath path;
        bool       filled = true;
        for (size_t index = 0; index < 1024 and filled; ++index)
        {
            filled = StringBuilder::format(path, "{}/watcher-overflow-entry-{}.txt", root.view(), index) and
                     fs.writeString(path.view(), "entry");
        }
        SC_TEST_EXPECT(filled);
        SC_TEST_EXPECT(runUntil(loop, [&state] { return state.rescans > 0; }));
        SC_TEST_EXPECT(state.overflowContext);
        SC_TEST_EXPECT(state.stopped == 0);
        SC_TEST_EXPECT(Path::join(path, {root.view(), "coverage-marker.txt"}));
        SC_TEST_EXPECT(fs.writeString(path.view(), "after overflow"));
        SC_TEST_EXPECT(runUntil(loop, [&state] { return state.marker; }));

        SC_TEST_EXPECT(::CancelIoEx(runner.directory, nullptr) != FALSE);
        SC_TEST_EXPECT(runUntil(loop, [&state] { return state.stopped > 0; }));
        SC_TEST_EXPECT(state.stopped == 1);
        SC_TEST_EXPECT(state.cancelled);
        SC_TEST_EXPECT(watch.stopWatching());

        SC_TEST_EXPECT(owner.watch(watch, root.view()));
        runner.rejectSubmission = true;
        SC_TEST_EXPECT(fs.writeString(path.view(), "rearm failure"));
        SC_TEST_EXPECT(runUntil(loop, [&state] { return state.stopped == 2; }));
        SC_TEST_EXPECT(state.failedRearm);
        SC_TEST_EXPECT(watch.stopWatching());

        runner.rejectSubmission = false;
        SC_TEST_EXPECT(owner.watch(watch, root.view()));
        SC_TEST_EXPECT(fs.writeString(path.view(), "old request"));
        SC_TEST_EXPECT(watch.stopWatching());
        SC_TEST_EXPECT(owner.watch(watch, root.view()));
        state.marker = false;
        SC_TEST_EXPECT(fs.writeString(path.view(), "new request"));
        SC_TEST_EXPECT(runUntil(loop, [&state] { return state.marker; }));
        SC_TEST_EXPECT(watch.stopWatching());
        SC_TEST_EXPECT(state.stopped == 2);
        SC_TEST_EXPECT(owner.close());
        closeOwner.disarm();
        SC_TEST_EXPECT(loop.close());
    }
#endif

#if SC_PLATFORM_APPLE
    void appleRefreshOverlappingWatchers(StringView appDirectory)
    {
        if (not test_section("AsyncEventLoop FSEvents refresh overlapping watchers"))
            return;

        FileSystem fs;
        SC_TEST_EXPECT(fs.init(appDirectory));
        StringPath root, child, trigger, afterRefresh;
        SC_TEST_EXPECT(Path::join(root, {appDirectory, "__sc fsevents refresh"}));
        const ResultFileSystem created = fs.makeDirectory(root.view());
        SC_TEST_EXPECT(created);
        if (not created)
            return;
        auto cleanup = MakeDeferred([&] { (void)fs.removeDirectoryRecursive(root.view()); });
        SC_TEST_EXPECT(Path::join(child, {root.view(), "child"}));
        SC_TEST_EXPECT(Path::join(trigger, {child.view(), "trigger.txt"}));
        SC_TEST_EXPECT(Path::join(afterRefresh, {child.view(), "after-refresh.txt"}));
        SC_TEST_EXPECT(fs.makeDirectory(child.view()));

        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());
        auto                                    closeLoop = MakeDeferred([&] { (void)loop.close(); });
        FileSystemWatcherAsyncT<AsyncEventLoop> runner;
        runner.init(loop);
        FileSystemWatcher                owner;
        FileSystemWatcher::FolderWatcher refreshing, remaining;
        auto                             closeOwner = MakeDeferred([&] { (void)owner.close(); });
        SC_TEST_EXPECT(owner.init(runner));

        using Kind = FileSystemWatcher::NotificationKind;
        struct State
        {
            FileSystemWatcher&                owner;
            FileSystemWatcher::FolderWatcher& refreshing;
            StringSpan                        root;
            StringSpan                        child;
            bool                              refreshed       = false;
            bool                              rescan          = false;
            bool                              remainingChange = false;
            bool                              restartedChange = false;
        } state{owner, refreshing, root.view(), child.view()};
        refreshing.notifyCallback = [this, &state](const FileSystemWatcher::Notification& event)
        {
            if (event.kind != Kind::Change)
                return;
            if (not state.refreshed and event.relativePath == "child/trigger.txt")
            {
                SC_TEST_EXPECT(state.refreshing.stopWatching());
                SC_TEST_EXPECT(state.owner.watch(state.refreshing, state.root));
                state.refreshed = true;
                StringPath full;
                SC_TEST_EXPECT(event.basePath == state.root);
                SC_TEST_EXPECT(event.relativePath == "child/trigger.txt");
                SC_TEST_EXPECT(event.getFullPath(full));
                SC_TEST_EXPECT(StringView(full.view()).endsWith("/child/trigger.txt"));
            }
            state.restartedChange |= event.relativePath == "child/after-refresh.txt";
        };
        remaining.notifyCallback = [&state](const FileSystemWatcher::Notification& event)
        {
            if (state.refreshed and event.kind == Kind::RescanRequired)
                state.rescan |= event.basePath == state.child and event.relativePath.isEmpty() and event.failure;
            state.remainingChange |= event.kind == Kind::Change and event.relativePath == "after-refresh.txt";
        };

        SC_TEST_EXPECT(owner.watch(refreshing, root.view()));
        SC_TEST_EXPECT(owner.watch(remaining, child.view()));
        submitQueuedWatcher(loop);
        SC_TEST_EXPECT(fs.writeString(trigger.view(), "marker"));
        const bool recovered = runUntil(loop, [&] { return state.refreshed and state.rescan; });
        SC_TEST_EXPECT(state.refreshed);
        SC_TEST_EXPECT(state.rescan);
        SC_TEST_EXPECT(recovered);
        SC_TEST_EXPECT(fs.writeString(afterRefresh.view(), "marker"));
        SC_TEST_EXPECT(runUntil(loop, [&] { return state.remainingChange and state.restartedChange; }));
    }

    void appleRootChanged(StringView appDirectory)
    {
        if (not test_section("AsyncEventLoop FSEvents root change"))
            return;

        FileSystem fs;
        SC_TEST_EXPECT(fs.init(appDirectory));
        StringPath fixture;
        bool       created = false;
        for (size_t attempt = 0; attempt < 128 and not created; ++attempt)
        {
            if (not StringBuilder::format(fixture, "__sc fsevents root {}", attempt))
                break;
            created = static_cast<bool>(fs.makeDirectory(fixture.view()));
        }
        SC_TEST_EXPECT(created);
        if (not created)
            return;
        auto cleanup = MakeDeferred([&] { (void)fs.removeDirectoryRecursive(fixture.view()); });

        StringPath root, sibling, moved, alias;
        SC_TEST_EXPECT(Path::join(root, {appDirectory, fixture.view(), "tree"}));
        SC_TEST_EXPECT(Path::join(sibling, {appDirectory, fixture.view(), "tree-other"}));
        SC_TEST_EXPECT(Path::join(moved, {appDirectory, fixture.view(), "moved"}));
        SC_TEST_EXPECT(Path::join(alias, {appDirectory, fixture.view(), "alias"}));
        SC_TEST_EXPECT(fs.makeDirectory(root.view()));
        SC_TEST_EXPECT(fs.makeDirectory(sibling.view()));
        SC_TEST_EXPECT(fs.createSymbolicLink(root.view(), alias.view()));
        SC_TEST_EXPECT(alias.append("/"));

        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());
        FileSystemWatcherAsyncT<AsyncEventLoop> runner;
        runner.init(loop);
        FileSystemWatcher                owner;
        FileSystemWatcher::FolderWatcher lost, remaining;
        auto                             closeOwner = MakeDeferred([&] { (void)owner.close(); });
        SC_TEST_EXPECT(owner.init(runner));

        using Kind = FileSystemWatcher::NotificationKind;
        struct State
        {
            StringSpan alias;
            bool       ready         = false;
            bool       remainingSeen = false;
            bool       restarted     = false;
            bool       validStop     = false;
            unsigned   stops         = 0;
            unsigned   siblingStops  = 0;
        } state{alias.view()};
        lost.notifyCallback = [&state](const FileSystemWatcher::Notification& event)
        {
            if (event.kind == Kind::WatchStopped)
            {
                ++state.stops;
                StringPath full;
                state.validStop = event.basePath == state.alias and event.relativePath.isEmpty() and
                                  event.failure.isError(FileSystemWatcherError::MonitoringFailed) and
                                  event.failure.detail == FileSystemWatcherErrorDetail::AppleWatchInvalidated and
                                  event.getFullPath(full) and full.view() == state.alias;
            }
            if (event.kind == Kind::Change)
            {
                state.ready |= event.relativePath == "ready.txt";
                state.restarted |= event.relativePath == "restart.txt";
            }
        };
        remaining.notifyCallback = [&state](const FileSystemWatcher::Notification& event)
        {
            if (event.kind == Kind::WatchStopped)
                ++state.siblingStops;
            state.remainingSeen |= event.kind == Kind::Change and event.relativePath == "marker.txt";
        };
        auto write = [&](StringView directory, StringView name)
        {
            StringPath file;
            return Path::join(file, {directory, name}) and fs.writeString(file.view(), "marker");
        };

        SC_TEST_EXPECT(owner.watch(lost, alias.view()));
        SC_TEST_EXPECT(owner.watch(remaining, sibling.view()));
        SC_TEST_EXPECT(write(root.view(), "ready.txt"));
        SC_TEST_EXPECT(runUntil(loop, [&state] { return state.ready; }));
        SC_TEST_EXPECT(fs.rename(root.view(), moved.view()));
        SC_TEST_EXPECT(write(sibling.view(), "marker.txt"));
        SC_TEST_EXPECT(runUntil(loop, [&state] { return state.stops != 0 and state.remainingSeen; }));
        SC_TEST_EXPECT(state.validStop);
        SC_TEST_EXPECT(state.stops == 1);
        SC_TEST_EXPECT(state.siblingStops == 0);
        SC_TEST_EXPECT(lost.stopWatching());
        SC_TEST_EXPECT(owner.watch(lost, moved.view()));
        SC_TEST_EXPECT(write(moved.view(), "restart.txt"));
        SC_TEST_EXPECT(runUntil(loop, [&state] { return state.restarted; }));
        SC_TEST_EXPECT(state.stops == 1);
        SC_TEST_EXPECT(lost.stopWatching());
        SC_TEST_EXPECT(remaining.stopWatching());
        SC_TEST_EXPECT(owner.close());
        closeOwner.disarm();
        SC_TEST_EXPECT(loop.close());
    }
#endif

#if SC_PLATFORM_LINUX
    void eventLoopLongNotificationPath()
    {
        if (not test_section("AsyncEventLoop inotify long notification path"))
            return;

        char       root[]  = "/tmp/sc-watcher-path.XXXXXXXX";
        const bool created = ::mkdtemp(root) != nullptr;
        SC_TEST_EXPECT(created);
        if (not created)
            return;

        char component[251];
        for (size_t index = 0; index + 1 < sizeof(component); ++index)
            component[index] = 'd';
        component[sizeof(component) - 1] = '\0';
        char fileName[151];
        for (size_t index = 0; index + 1 < sizeof(fileName); ++index)
            fileName[index] = 'f';
        fileName[sizeof(fileName) - 1] = '\0';

        constexpr size_t depth = 16;
        int              descriptors[depth + 1];
        size_t           opened  = 0;
        auto             cleanup = MakeDeferred(
            [&]
            {
                if (opened == depth + 1)
                {
                    (void)::unlinkat(descriptors[depth], fileName, 0);
                    (void)::unlinkat(descriptors[depth], "marker", 0);
                }
                while (opened > 0)
                {
                    (void)::close(descriptors[--opened]);
                    if (opened != 0)
                        (void)::unlinkat(descriptors[opened - 1], component, AT_REMOVEDIR);
                }
                (void)::rmdir(root);
            });
        descriptors[0] = ::open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        SC_TEST_EXPECT(descriptors[0] != -1);
        if (descriptors[0] == -1)
            return;
        opened = 1;
        while (opened <= depth)
        {
            const int  parent = descriptors[opened - 1];
            const bool made   = ::mkdirat(parent, component, 0700) == 0;
            SC_TEST_EXPECT(made);
            if (not made)
                return;
            const int child = ::openat(parent, component, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
            SC_TEST_EXPECT(child != -1);
            if (child == -1)
            {
                (void)::unlinkat(parent, component, AT_REMOVEDIR);
                return;
            }
            descriptors[opened++] = child;
        }

        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());
        auto                                    closeLoop = MakeDeferred([&] { (void)loop.close(); });
        FileSystemWatcherAsyncT<AsyncEventLoop> runner;
        runner.init(loop);
        FileSystemWatcher                owner;
        char                             paths[40 * 1024];
        FileSystemWatcher::FolderWatcher watcher(paths);
        auto                             closeOwner = MakeDeferred([&] { (void)owner.close(); });
        SC_TEST_EXPECT(owner.init(runner));

        struct State
        {
            StringSpan root;
            bool       rescan = false;
            bool       marker = false;
        } state{StringSpan::fromNullTerminated(root, StringEncoding::Utf8)};
        using Kind             = FileSystemWatcher::NotificationKind;
        watcher.notifyCallback = [&state](const FileSystemWatcher::Notification& event)
        {
            if (event.kind == Kind::RescanRequired)
                state.rescan |= event.basePath == state.root and event.relativePath.isEmpty() and event.failure and
                                event.operation == FileSystemWatcher::Operation::AddRemoveRename;
            if (event.kind == Kind::Change)
                state.marker |= StringView(event.relativePath).endsWith("/marker");
        };
        SC_TEST_EXPECT(owner.watch(watcher, state.root));
        submitQueuedWatcher(loop);
        auto createFile = [&](const char* name)
        {
            const int fd = ::openat(descriptors[depth], name, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
            return fd != -1 and ::close(fd) == 0;
        };
        SC_TEST_EXPECT(createFile(fileName));
        const bool delivered = runUntil(loop, [&state] { return state.rescan; });
        SC_TEST_EXPECT(delivered);
        SC_TEST_EXPECT(createFile("marker"));
        SC_TEST_EXPECT(runUntil(loop, [&state] { return state.marker; }));
        SC_TEST_EXPECT(owner.close());
        closeOwner.disarm();
        SC_TEST_EXPECT(loop.close());
        closeLoop.disarm();
    }

    void eventLoopWatchInvalidation(const StringView appDirectory)
    {
        if (not test_section("AsyncEventLoop inotify watch invalidation"))
            return;

        FileSystem fs;
        SC_TEST_EXPECT(fs.init(appDirectory));
        StringPath fixture;
        bool       created = false;
        for (size_t attempt = 0; attempt < 128 and not created; ++attempt)
        {
            if (not StringBuilder::format(fixture, "__sc inotify invalidation {}", attempt))
                break;
            created = static_cast<bool>(fs.makeDirectory(fixture.view()));
        }
        SC_TEST_EXPECT(created);
        if (not created)
            return;
        auto cleanup = MakeDeferred([&] { (void)fs.removeDirectoryRecursive(fixture.view()); });

        StringPath tree, child, grandchild, sibling, moved, movedChild;
        SC_TEST_EXPECT(Path::join(tree, {appDirectory, fixture.view(), "tree"}));
        SC_TEST_EXPECT(Path::join(child, {tree.view(), "child"}));
        SC_TEST_EXPECT(Path::join(grandchild, {child.view(), "grandchild"}));
        SC_TEST_EXPECT(Path::join(sibling, {tree.view(), "child-other"}));
        SC_TEST_EXPECT(Path::join(moved, {appDirectory, fixture.view(), "moved"}));
        SC_TEST_EXPECT(Path::join(movedChild, {moved.view(), "grandchild"}));
        SC_TEST_EXPECT(fs.makeDirectoryRecursive(grandchild.view()));
        SC_TEST_EXPECT(fs.makeDirectory(sibling.view()));

        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());
        FileSystemWatcherAsyncT<AsyncEventLoop> runner;
        runner.init(loop);
        FileSystemWatcher                owner;
        FileSystemWatcher::FolderWatcher outer, inner;
        auto                             closeOwner = MakeDeferred([&] { (void)owner.close(); });
        SC_TEST_EXPECT(owner.init(runner));

        using Kind = FileSystemWatcher::NotificationKind;
        struct State
        {
            unsigned rescans      = 0;
            unsigned innerRescans = 0;
            unsigned outerStopped = 0;
            unsigned innerStopped = 0;
            bool     validStatus  = true;
            bool     stalePath    = false;
            bool     siblingSeen  = false;
            bool     movedSeen    = false;
        } state;
        outer.notifyCallback = [&state](const FileSystemWatcher::Notification& event)
        {
            if (event.kind == Kind::RescanRequired)
            {
                ++state.rescans;
                state.validStatus = state.validStatus and event.failure and event.relativePath.isEmpty();
            }
            if (event.kind == Kind::WatchStopped)
            {
                ++state.outerStopped;
                state.validStatus = state.validStatus and event.relativePath.isEmpty() and
                                    event.failure.isError(FileSystemWatcherError::MonitoringFailed) and
                                    event.failure.detail == FileSystemWatcherErrorDetail::LinuxWatchInvalidated;
            }
            const StringView path = event.relativePath;
            state.stalePath |= path.endsWith("stale.txt");
            state.siblingSeen |= path == "child-other/marker.txt";
        };
        inner.notifyCallback = [&state](const FileSystemWatcher::Notification& event)
        {
            if (event.kind == Kind::RescanRequired)
            {
                ++state.innerRescans;
                state.validStatus = state.validStatus and event.failure and event.relativePath.isEmpty();
            }
            if (event.kind == Kind::WatchStopped)
            {
                ++state.innerStopped;
                state.validStatus = state.validStatus and event.relativePath.isEmpty() and
                                    event.failure.isError(FileSystemWatcherError::MonitoringFailed) and
                                    event.failure.detail == FileSystemWatcherErrorDetail::LinuxWatchInvalidated;
            }
            const StringView path = event.relativePath;
            state.stalePath |= path.endsWith("stale.txt");
            state.movedSeen |= path.endsWith("restarted.txt");
        };
        const auto write = [&](StringView directory, StringView name)
        {
            StringPath path;
            return Path::join(path, {directory, name}) and fs.writeString(path.view(), "event");
        };

        SC_TEST_EXPECT(owner.watch(outer, tree.view()));
        SC_TEST_EXPECT(owner.watch(inner, child.view()));
        submitQueuedWatcher(loop);
        SC_TEST_EXPECT(fs.rename(child.view(), moved.view()));
        SC_TEST_EXPECT(write(movedChild.view(), "stale.txt"));
        SC_TEST_EXPECT(write(sibling.view(), "marker.txt"));
        SC_TEST_EXPECT(runUntil(loop, [&state] { return state.siblingSeen; }));
        SC_TEST_EXPECT(state.rescans == 1);
        SC_TEST_EXPECT(state.innerStopped == 1);
        SC_TEST_EXPECT(not state.stalePath);

        SC_TEST_EXPECT(inner.stopWatching());
        SC_TEST_EXPECT(owner.watch(inner, moved.view()));
        SC_TEST_EXPECT(fs.removeDirectoryRecursive(tree.view()));
        SC_TEST_EXPECT(write(movedChild.view(), "restarted.txt"));
        SC_TEST_EXPECT(runUntil(loop, [&state] { return state.movedSeen; }));
        SC_TEST_EXPECT(state.outerStopped == 1);
        SC_TEST_EXPECT(state.innerStopped == 1);
        SC_TEST_EXPECT(outer.stopWatching());

        StringPath createdDirectory, incomingDirectory, importedDirectory;
        SC_TEST_EXPECT(Path::join(createdDirectory, {moved.view(), "created"}));
        SC_TEST_EXPECT(Path::join(incomingDirectory, {appDirectory, fixture.view(), "incoming"}));
        SC_TEST_EXPECT(Path::join(importedDirectory, {movedChild.view(), "imported"}));
        SC_TEST_EXPECT(fs.makeDirectory(createdDirectory.view()));
        SC_TEST_EXPECT(fs.makeDirectory(incomingDirectory.view()));
        SC_TEST_EXPECT(write(incomingDirectory.view(), "already-present.txt"));
        SC_TEST_EXPECT(fs.rename(incomingDirectory.view(), importedDirectory.view()));
        state.movedSeen = false;
        SC_TEST_EXPECT(write(moved.view(), "restarted.txt"));
        SC_TEST_EXPECT(runUntil(loop, [&state] { return state.movedSeen; }));
        SC_TEST_EXPECT(state.innerRescans == 2);
        SC_TEST_EXPECT(state.validStatus);

        // The structural notifications run after directory registration. Later changes must be visible on
        // the original watch, including in a populated subtree moved into the tree.
        state.movedSeen = false;
        SC_TEST_EXPECT(write(createdDirectory.view(), "restarted.txt"));
        SC_TEST_EXPECT(runUntil(loop, [&state] { return state.movedSeen; }));
        state.movedSeen = false;
        SC_TEST_EXPECT(write(importedDirectory.view(), "restarted.txt"));
        SC_TEST_EXPECT(runUntil(loop, [&state] { return state.movedSeen; }));
        SC_TEST_EXPECT(state.innerRescans == 2);
        SC_TEST_EXPECT(inner.stopWatching());
        SC_TEST_EXPECT(owner.close());
        closeOwner.disarm();
        SC_TEST_EXPECT(loop.close());
    }

    void eventLoopOverflow(const StringView appDirectory)
    {
        if (not test_section("AsyncEventLoop inotify overflow"))
            return;

        FileDescriptor limitFile;
        if (not limitFile.open("/proc/sys/fs/inotify/max_queued_events", FileOpen{}))
        {
            report.console.print("Skipping inotify overflow test: queue limit is unavailable\n");
            return;
        }
        char             limitBuffer[32];
        Span<char>       limitBytes;
        const ResultFile readLimit = limitFile.read(limitBuffer, limitBytes);
        SC_TEST_EXPECT(readLimit);
        SC_TEST_EXPECT(limitFile.close());
        if (not readLimit)
            return;
        int32_t          queueLimit = 0;
        const StringView limitText({limitBuffer, limitBytes.sizeInBytes()}, false, StringEncoding::Ascii);
        const bool       parsed = limitText.trimWhiteSpaces().parseInt32(queueLimit) and queueLimit > 0;
        SC_TEST_EXPECT(parsed);
        if (not parsed)
            return;
        if (queueLimit > 100000)
        {
            report.console.print("Skipping inotify overflow test: queue limit is {}\n",
                                 static_cast<size_t>(queueLimit));
            return;
        }

        FileSystem             fs;
        const ResultFileSystem fsReady = fs.init(appDirectory);
        SC_TEST_EXPECT(fsReady);
        if (not fsReady)
            return;
        StringPath directoryName;
        bool       created = false;
        for (size_t attempt = 0; attempt < 128 and not created; ++attempt)
        {
            if (not StringBuilder::format(directoryName, "__sc overflow {}", attempt))
                break;
            created = static_cast<bool>(fs.makeDirectory(directoryName.view()));
        }
        SC_TEST_EXPECT(created);
        if (not created)
            return;
        auto       removeDirectory = MakeDeferred([&] { (void)fs.removeDirectoryRecursive(directoryName.view()); });
        StringPath directoryPath;
        StringPath firstPath;
        StringPath secondPath;
        StringPath victimPath;
        StringPath eventPath;
        StringPath afterPath;
        const bool pathsReady = Path::join(directoryPath, {appDirectory, directoryName.view()}) and
                                Path::join(firstPath, {directoryPath.view(), "first"}) and
                                Path::join(secondPath, {directoryPath.view(), "second"}) and
                                Path::join(victimPath, {directoryPath.view(), "victim"}) and
                                Path::join(eventPath, {firstPath.view(), "event"}) and
                                Path::join(afterPath, {secondPath.view(), "after"}) and
                                fs.makeDirectory(firstPath.view()) and fs.makeDirectory(secondPath.view()) and
                                fs.makeDirectory(victimPath.view());
        SC_TEST_EXPECT(pathsReady);
        if (not pathsReady)
            return;

        AsyncEventLoop eventLoop;
        const Result   loopReady = eventLoop.create();
        SC_TEST_EXPECT(loopReady);
        if (not loopReady)
            return;
        FileSystemWatcherAsyncT<AsyncEventLoop> runner;
        runner.init(eventLoop);
        FileSystemWatcher fileEventsWatcher;
        struct OverflowState
        {
            FileSystemWatcher::FolderWatcher* first  = nullptr;
            FileSystemWatcher::FolderWatcher* victim = nullptr;

            StringView expectedFirst;
            StringView expectedSecond;

            unsigned observed      = 0;
            bool     stopped       = false;
            bool     victimStopped = false;
            bool     victimCalled  = false;
            bool     firstBase     = false;
            bool     secondBase    = false;
            bool     firstStatus   = false;
            bool     secondStatus  = false;
            bool     after         = false;
        } state;
        // Register the folders in reverse address order. Callbacks may discard later registrations.
        FileSystemWatcher::FolderWatcher watchers[3];
        state.first                = &watchers[2];
        state.victim               = &watchers[1];
        state.expectedFirst        = firstPath.view();
        state.expectedSecond       = secondPath.view();
        watchers[2].notifyCallback = [&state](const FileSystemWatcher::Notification& notification)
        {
            if (notification.relativePath.isEmpty() and
                notification.operation == FileSystemWatcher::Operation::AddRemoveRename)
            {
                state.observed |= 1;
                state.firstBase   = notification.basePath == state.expectedFirst;
                state.firstStatus = notification.kind == FileSystemWatcher::NotificationKind::RescanRequired and
                                    static_cast<bool>(notification.failure);
                state.victimStopped = static_cast<bool>(state.victim->stopWatching());
                state.stopped       = static_cast<bool>(state.first->stopWatching());
            }
        };
        watchers[1].notifyCallback = [&state](const FileSystemWatcher::Notification&) { state.victimCalled = true; };
        watchers[0].notifyCallback = [&state](const FileSystemWatcher::Notification& notification)
        {
            if (notification.relativePath.isEmpty() and
                notification.operation == FileSystemWatcher::Operation::AddRemoveRename)
            {
                state.observed |= 2;
                state.secondBase   = notification.basePath == state.expectedSecond;
                state.secondStatus = notification.kind == FileSystemWatcher::NotificationKind::RescanRequired and
                                     static_cast<bool>(notification.failure);
            }
            if (notification.relativePath == "after")
                state.after = true;
        };
        auto closeLoop    = MakeDeferred([&] { (void)eventLoop.close(); });
        auto closeWatcher = MakeDeferred([&] { (void)fileEventsWatcher.close(); });

        const ResultFileSystemWatcher watcherReady = fileEventsWatcher.init(runner);
        SC_TEST_EXPECT(watcherReady);
        if (not watcherReady)
            return;
        SC_TEST_EXPECT(fileEventsWatcher.watch(watchers[2], firstPath.view()));
        SC_TEST_EXPECT(fileEventsWatcher.watch(watchers[1], victimPath.view()));
        SC_TEST_EXPECT(fileEventsWatcher.watch(watchers[0], secondPath.view()));
        submitQueuedWatcher(eventLoop);

        bool generated = true;
        for (int32_t idx = 0; idx < queueLimit / 2 + 32; ++idx)
        {
            if (not fs.writeString(eventPath.view(), "") or not fs.removeFile(eventPath.view()))
            {
                generated = false;
                break;
            }
        }
        SC_TEST_EXPECT(generated);
        if (not generated)
            return;
        const bool observed = runUntil(eventLoop, [&state] { return state.observed == 3; });
        SC_TEST_EXPECT(observed);
        if (not observed)
            return;
        SC_TEST_EXPECT(state.stopped and state.victimStopped and not state.victimCalled and state.firstBase and
                       state.secondBase and state.firstStatus and state.secondStatus);
        SC_TEST_EXPECT(fs.writeString(afterPath.view(), "after"));
        SC_TEST_EXPECT(runUntil(eventLoop, [&state] { return state.after; }));

        const ResultFileSystemWatcher closed = fileEventsWatcher.close();
        SC_TEST_EXPECT(closed);
        if (not closed)
            return;
        closeWatcher.disarm();
        const Result loopClosed = eventLoop.close();
        SC_TEST_EXPECT(loopClosed);
        if (not loopClosed)
            return;
        closeLoop.disarm();
        const ResultFileSystem removed = fs.removeDirectoryRecursive(directoryName.view());
        SC_TEST_EXPECT(removed);
        if (removed)
            removeDirectory.disarm();
    }

    void eventLoopReadFailure(const StringView appDirectory)
    {
        if (not test_section("AsyncEventLoop inotify read failure"))
            return;
        FileSystem             fs;
        const ResultFileSystem fsReady = fs.init(appDirectory);
        SC_TEST_EXPECT(fsReady);
        if (not fsReady)
            return;
        StringPath directoryName;
        bool       created = false;
        for (size_t attempt = 0; attempt < 128 and not created; ++attempt)
        {
            if (not StringBuilder::format(directoryName, "__sc read failure {}", attempt))
                break;
            created = static_cast<bool>(fs.makeDirectory(directoryName.view()));
        }
        SC_TEST_EXPECT(created);
        if (not created)
            return;
        auto       removeDirectory = MakeDeferred([&] { (void)fs.removeDirectoryRecursive(directoryName.view()); });
        StringPath directoryPath;
        StringPath normalPath;
        StringPath failurePath;
        const bool pathsReady = Path::join(directoryPath, {appDirectory, directoryName.view()}) and
                                Path::join(normalPath, {directoryPath.view(), "normal"}) and
                                Path::join(failurePath, {directoryPath.view(), "failure"});
        SC_TEST_EXPECT(pathsReady);
        if (not pathsReady)
            return;

        struct Runner : FileSystemWatcherAsyncT<AsyncEventLoop>
        {
            int descriptor() const { return notifyFd; }

            void injectReadinessFailure(Result failure)
            {
                AsyncFileReadiness::Result result(*eventLoop, asyncPoll, failure);
                onEventLoopNotification(result);
            }
        } runner;
        AsyncEventLoop          eventLoop;
        AsyncEventLoop::Options options;
        options.apiType        = AsyncEventLoop::Options::ApiType::ForceUseEpoll;
        const Result loopReady = eventLoop.create(options);
        SC_TEST_EXPECT(loopReady);
        if (not loopReady)
            return;
        runner.init(eventLoop);
        FileSystemWatcher             fileEventsWatcher;
        const ResultFileSystemWatcher watcherReady = fileEventsWatcher.init(runner);
        SC_TEST_EXPECT(watcherReady);
        if (not watcherReady)
        {
            (void)eventLoop.close();
            return;
        }
        FileSystemWatcher::FolderWatcher watchers[3];
        struct FailureState
        {
            FileSystemWatcher* owner = nullptr;

            FileSystemWatcher::FolderWatcher* first  = nullptr;
            FileSystemWatcher::FolderWatcher* victim = nullptr;

            StringView path;

            int fd       = -1;
            int backupFd = -1;

            unsigned normal   = 0;
            unsigned failures = 0;

            bool normalStatus   = true;
            bool restored       = false;
            bool stopped        = false;
            bool victimStopped  = false;
            bool victimCalled   = false;
            bool rejected       = false;
            bool closed         = false;
            bool failureDetails = true;
        } state;
        state.owner  = &fileEventsWatcher;
        state.first  = &watchers[0];
        state.victim = &watchers[1];
        state.path   = directoryPath.view();
        state.fd     = runner.descriptor();

        watchers[0].notifyCallback = [&state](const FileSystemWatcher::Notification& notification)
        {
            if (notification.kind == FileSystemWatcher::NotificationKind::Change)
            {
                ++state.normal;
                state.normalStatus = state.normalStatus and static_cast<bool>(notification.failure);
                return;
            }
            if (notification.kind != FileSystemWatcher::NotificationKind::WatchStopped)
                return;
            ++state.failures;
            state.failureDetails = notification.basePath == state.path and notification.relativePath.isEmpty() and
                                   notification.operation == FileSystemWatcher::Operation::AddRemoveRename and
                                   notification.failure.isError(FileSystemWatcherError::MonitoringFailed) and
                                   notification.failure.detail == FileSystemWatcherErrorDetail::LinuxReadEvents and
                                   notification.failure.nativeError == EBADF;
            if (::dup2(state.backupFd, state.fd) == state.fd)
            {
                state.restored = ::close(state.backupFd) == 0;
                state.backupFd = -1;
            }
            FileSystemWatcher::FolderWatcher retry;
            retry.notifyCallback = [](const FileSystemWatcher::Notification&){};
            state.rejected = state.owner->watch(retry, state.path).isError(FileSystemWatcherError::MonitoringFailed);
            state.victimStopped = static_cast<bool>(state.victim->stopWatching());
            state.stopped       = static_cast<bool>(state.first->stopWatching());
        };
        watchers[1].notifyCallback = [&state](const FileSystemWatcher::Notification& notification)
        {
            if (notification.kind == FileSystemWatcher::NotificationKind::WatchStopped)
                state.victimCalled = true;
        };
        watchers[2].notifyCallback = [&state](const FileSystemWatcher::Notification& notification)
        {
            if (notification.kind == FileSystemWatcher::NotificationKind::WatchStopped)
            {
                ++state.failures;
                state.failureDetails = state.failureDetails and notification.basePath == state.path and
                                       notification.failure.isError(FileSystemWatcherError::MonitoringFailed) and
                                       notification.failure.detail == FileSystemWatcherErrorDetail::LinuxReadEvents and
                                       notification.failure.nativeError == EBADF;
                state.closed = static_cast<bool>(state.owner->close());
            }
        };
        auto closeLoop    = MakeDeferred([&] { (void)eventLoop.close(); });
        auto closeWatcher = MakeDeferred([&] { (void)fileEventsWatcher.close(); });
        auto restoreFd    = MakeDeferred(
            [&]
            {
                if (state.backupFd != -1)
                {
                    (void)::dup2(state.backupFd, state.fd);
                    (void)::close(state.backupFd);
                }
            });
        bool allWatching = true;
        for (auto& watcher : watchers)
        {
            const ResultFileSystemWatcher watched = fileEventsWatcher.watch(watcher, directoryPath.view());
            SC_TEST_EXPECT(watched);
            allWatching = allWatching and static_cast<bool>(watched);
        }
        if (not allWatching)
            return;
        submitQueuedWatcher(eventLoop);
        SC_TEST_EXPECT(fileEventsWatcher.asyncNotify(nullptr)); // A nonblocking read with no events is not a failure.
        SC_TEST_EXPECT(state.normal == 0 and state.failures == 0);
        SC_TEST_EXPECT(fs.writeString(normalPath.view(), "normal"));
        SC_TEST_EXPECT(runUntil(eventLoop, [&state] { return state.normal != 0; }));
        SC_TEST_EXPECT(state.normalStatus);
        SC_TEST_EXPECT(fs.writeString(failurePath.view(), "failure"));
        state.backupFd = ::dup(state.fd);
        SC_TEST_EXPECT(state.backupFd != -1);
        if (state.backupFd == -1)
            return;
        const bool invalidated = ::close(state.fd) == 0;
        SC_TEST_EXPECT(invalidated);
        if (not invalidated)
            return;
        SC_TEST_EXPECT(runUntil(eventLoop, [&state] { return state.failures == 2; }));
        SC_TEST_EXPECT(state.restored and state.stopped and state.victimStopped and not state.victimCalled and
                       state.rejected and state.closed and state.failureDetails);
        if (not state.closed)
            return;
        closeWatcher.disarm();
        SC_TEST_EXPECT(eventLoop.runNoWait());

        struct AsyncFailureState
        {
            FileSystemWatcher* owner = nullptr;

            bool delivered = false;
            bool closed    = false;
        } asyncFailure;
        asyncFailure.owner = &fileEventsWatcher;

        watchers[2].notifyCallback = [&asyncFailure](const FileSystemWatcher::Notification& notification)
        {
            asyncFailure.delivered =
                notification.kind == FileSystemWatcher::NotificationKind::WatchStopped and
                notification.failure.result.isError(AsyncResultCategory, AsyncError::WatcherRegistrationFailed) and
                notification.failure.detail == FileSystemWatcherErrorDetail::None and
                notification.failure.nativeError == 0;
            asyncFailure.closed = static_cast<bool>(asyncFailure.owner->close());
        };
        const ResultFileSystemWatcher reinitialized = fileEventsWatcher.init(runner);
        SC_TEST_EXPECT(reinitialized);
        if (not reinitialized)
            return;
        auto closeReinitializedWatcher = MakeDeferred([&] { (void)fileEventsWatcher.close(); });

        const ResultFileSystemWatcher rewatched = fileEventsWatcher.watch(watchers[2], directoryPath.view());
        SC_TEST_EXPECT(rewatched);
        if (not rewatched)
            return;
        runner.injectReadinessFailure(Result::Error(AsyncResultCategory, AsyncError::WatcherRegistrationFailed));
        SC_TEST_EXPECT(asyncFailure.delivered and asyncFailure.closed);
        if (not asyncFailure.closed)
            return;
        closeReinitializedWatcher.disarm();
        SC_TEST_EXPECT(eventLoop.runNoWait());
        const Result loopClosed = eventLoop.close();
        SC_TEST_EXPECT(loopClosed);
        if (not loopClosed)
            return;
        closeLoop.disarm();
    }
#endif
};

namespace SC
{
void runFileSystemWatcherAsyncTest(SC::TestReport& report) { FileSystemWatcherAsyncTest test(report); }
} // namespace SC

namespace SC
{
Result fileSystemWatcherAsyncSnippet(AsyncEventLoop& eventLoop, Console& console)
{
    //! [fileSystemWatcherAsyncSnippet]
    // Initialize the FileSystemWatcher
    FileSystemWatcher fileSystemWatcher;

    FileSystemWatcherAsyncT<AsyncEventLoop> eventLoopRunner;
    eventLoopRunner.init(eventLoop);
    SC_TRY(fileSystemWatcher.init(eventLoopRunner));

    // Setup notification callback
    auto onFileModified = [&](const FileSystemWatcher::Notification& notification)
    {
        // This callback will be called from the thread calling AsyncEventLoop::run
        StringPath fullPath;
        if (notification.getFullPath(fullPath))
        {
            switch (notification.operation)
            {
            case FileSystemWatcher::Operation::Modified: // File has been modified
                console.print("Modified {} {}\n", notification.relativePath, fullPath.view());
                break;
            case FileSystemWatcher::Operation::AddRemoveRename: // File was added / removed
                console.print("AddRemoveRename {} {}\n", notification.relativePath, fullPath.view());
                break;
            }
        }
    };

    // Start watching a specific folder
    FileSystemWatcher::FolderWatcher folderWatcher;
    folderWatcher.notifyCallback = onFileModified;
    SC_TRY(fileSystemWatcher.watch(folderWatcher, "/path/to/dir"));

    // ...
    // At a later point when there is no more need of watching the folder
    SC_TRY(folderWatcher.stopWatching());

    // ...
    // When all watchers have been unwatched and to dispose all system resources
    SC_TRY(fileSystemWatcher.close());
    //! [fileSystemWatcherAsyncSnippet]
    return Result(true);
}

} // namespace SC
