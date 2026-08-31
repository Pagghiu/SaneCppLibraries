// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Async/Async.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/FileSystemWatcher/FileSystemWatcher.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/Console.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/Threading.h"

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
        eventLoopSubdirectory(appDirectory);
        eventLoopWatchClose(appDirectory);
        eventLoopWaitTimeout();
        eventLoopWatchStop(appDirectory);
        eventLoopCloseMultipleWatchers(appDirectory);
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
                if (notification.relativePath != params.expectedRelativePath.view())
                {
                    return;
                }

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
            SC_TEST_EXPECT(runUntil(eventLoop, [&] { return params.addRemoveRenameChanges > 0; }));
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
