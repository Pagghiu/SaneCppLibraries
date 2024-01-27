// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../FileSystemWatcher.h"
#include "../../FileSystem/FileSystem.h"
#include "../../FileSystem/Path.h"
#include "../../Strings/StringBuilder.h"
#include "../../Testing/Testing.h"

namespace SC
{
struct FileSystemWatcherTest;
}

struct SC::FileSystemWatcherTest : public SC::TestCase
{
    FileSystemWatcherTest(SC::TestReport& report) : TestCase(report, "FileSystemWatcherTest")
    {
        using namespace SC;
        const StringView appDirectory = report.applicationRootDirectory;
        initClose();
        threadRunner(appDirectory);
        eventLoopSubdirectory(appDirectory);
        eventLoopWatchClose(appDirectory);
        eventLoopWatchStop(appDirectory);
    }

    void initClose()
    {
        if (test_section("Init/Close"))
        {
            FileSystemWatcher::ThreadRunner runner;

            FileSystemWatcher fileEventsWatcher;
            SC_TEST_EXPECT(fileEventsWatcher.init(runner));
            SC_TEST_EXPECT(fileEventsWatcher.close());
        }
    }

    void threadRunner(const StringView appDirectory)
    {
        if (test_section("ThreadRunner"))
        {
            // We need to sleep to avoid getting notifications of file ops from prev tests
            Thread::Sleep(100);
            FileSystemWatcher fileEventsWatcher;

            FileSystemWatcher::ThreadRunner runner;
            SC_TEST_EXPECT(fileEventsWatcher.init(runner));
            struct Params
            {
                uint64_t    changes          = 0;
                uint64_t    callbackThreadID = 0;
                StringView  appDirectory;
                EventObject eventObject;
            } params;
            params.appDirectory = appDirectory;

            auto lambda = [&](const FileSystemWatcher::Notification& notification)
            {
                constexpr native_char_t nativeSep = Path::Separator;

                StringNative<1024> fullPathBuffer = StringEncoding::Native;
                StringNative<1024> expectedBuffer = StringEncoding::Native;

                params.callbackThreadID = Thread::CurrentThreadID();
                params.changes++;
                SC_TEST_EXPECT(notification.operation == FileSystemWatcher::Operation::AddRemoveRename);
                SC_TEST_EXPECT(notification.basePath == params.appDirectory);
                // Comparisons must use the same encoding
                SC_TEST_EXPECT(notification.relativePath == "test.txt");
                StringView fullPath;
                SC_TEST_EXPECT(notification.getFullPath(fullPathBuffer, fullPath));

                StringBuilder expected(expectedBuffer);
                SC_TEST_EXPECT(expected.format("{}{}{}", params.appDirectory, nativeSep, "test.txt"));
                SC_TEST_EXPECT(fullPath == expectedBuffer.view());
                params.eventObject.signal();
            };

            FileSystem fs;
            SC_TEST_EXPECT(fs.init(appDirectory));
            if (fs.existsAndIsFile("test.txt"))
            {
                SC_TEST_EXPECT(fs.removeFile("test.txt"));
                Thread::Sleep(200);
            }

            StringNative<1024> path;
            SC_TEST_EXPECT(path.assign(appDirectory));
            FileSystemWatcher::FolderWatcher watcher;
            // We save the results and expect them after the wait to avoid Thread Sanitizer issues
            // due to the SC_TEST_EXPECT calls inside the lambda that runs in the thread
            const Result res        = fileEventsWatcher.watch(watcher, path.view(), move(lambda));
            const bool   fsWriteRes = fs.write("test.txt", "content");
            params.eventObject.wait();
            SC_TEST_EXPECT(fsWriteRes);
            SC_TEST_EXPECT(res);
            SC_TEST_EXPECT(params.changes > 0);
            SC_TEST_EXPECT(fileEventsWatcher.close());
            SC_TEST_EXPECT(params.callbackThreadID != Thread::CurrentThreadID());
            SC_TEST_EXPECT(fs.removeFile({"test.txt"_a8}));
        }
    }

    void eventLoopSubdirectory(const StringView appDirectory)
    {
        if (test_section("AsyncEventLoop"))
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            FileSystemWatcher fileEventsWatcher;

            FileSystemWatcher::EventLoopRunner runner{eventLoop};
            SC_TEST_EXPECT(fileEventsWatcher.init(runner));

            struct Params
            {
                uint64_t   callbackThreadID = 0;
                uint64_t   changes          = 0;
                StringView appDirectory;
            } params;
            params.appDirectory = appDirectory;

            auto lambda = [&](const FileSystemWatcher::Notification& notification)
            {
                constexpr native_char_t nativeSep = Path::Separator;

                StringNative<255>  dirBuffer      = StringEncoding::Native;
                StringNative<1024> fullPathBuffer = StringEncoding::Native;
                StringNative<1024> expectedBuffer = StringEncoding::Native;

                params.callbackThreadID = Thread::CurrentThreadID();
                params.changes++;
                SC_TEST_EXPECT(notification.operation == FileSystemWatcher::Operation::AddRemoveRename);
                SC_TEST_EXPECT(notification.basePath == params.appDirectory);
                SC_TEST_EXPECT(StringBuilder(dirBuffer).format("{}{}{}", "dir", nativeSep, "test.txt"));
                SC_TEST_EXPECT(notification.relativePath == dirBuffer.view());

                StringView fullPath;
                SC_TEST_EXPECT(notification.getFullPath(fullPathBuffer, fullPath));

                StringBuilder expected(expectedBuffer);
                SC_TEST_EXPECT(expected.format("{}{}{}", params.appDirectory, nativeSep, dirBuffer.view()));
                SC_TEST_EXPECT(fullPath == expectedBuffer.view());
            };

            FileSystem fs;
            SC_TEST_EXPECT(fs.init(appDirectory));
            if (not fs.existsAndIsDirectory("dir"))
            {
                SC_TEST_EXPECT(fs.makeDirectory({"dir"}));
            }
            if (fs.existsAndIsFile("dir/test.txt"))
            {
                SC_TEST_EXPECT(fs.removeFile("dir/test.txt"));
            }

            StringNative<1024> path;
            SC_TEST_EXPECT(path.assign(appDirectory));
            FileSystemWatcher::FolderWatcher watcher;
            Thread::Sleep(200); // on macOS watch latency is 500 ms, so we sleep to avoid report of 'dir' creation
            SC_TEST_EXPECT(fileEventsWatcher.watch(watcher, path.view(), move(lambda)));
            SC_TEST_EXPECT(fs.write("dir/test.txt", "content"));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(params.changes == 1);
            SC_TEST_EXPECT(fileEventsWatcher.close());
            SC_TEST_EXPECT(params.callbackThreadID == Thread::CurrentThreadID());
            SC_TEST_EXPECT(fs.removeFile({"dir/test.txt"_a8}));
            SC_TEST_EXPECT(fs.removeEmptyDirectory({"dir"}));
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

            FileSystemWatcher::EventLoopRunner runner{eventLoop};
            SC_TEST_EXPECT(fileEventsWatcher.init(runner));
            StringNative<1024> path;
            SC_TEST_EXPECT(path.assign(appDirectory));
            FileSystemWatcher::FolderWatcher watcher;
            struct Params
            {
                int changes = 0;
            } params;
            auto lambda = [&](const FileSystemWatcher::Notification&) { params.changes++; };
            SC_TEST_EXPECT(fileEventsWatcher.watch(watcher, path.view(), move(lambda)));
            SC_TEST_EXPECT(fs.write("salve.txt", "content"));
            SC_TEST_EXPECT(fs.write("a_tutti.txt", "content"));
            // On different OS and FileSystems it's possible to get completely random number of changes
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(eventLoop.runNoWait());
            SC_TEST_EXPECT(params.changes >= 2);
            SC_TEST_EXPECT(fileEventsWatcher.close());
            SC_TEST_EXPECT(fs.removeFiles({"salve.txt", "a_tutti.txt"}));
        }
    }

    void eventLoopWatchStop(const StringView appDirectory)
    {
        if (test_section("AsyncEventLoop watch/stopWatching"))
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            FileSystemWatcher                  fileEventsWatcher;
            FileSystemWatcher::EventLoopRunner runner{eventLoop};
            SC_TEST_EXPECT(fileEventsWatcher.init(runner));
            StringNative<1024> path1, path2;
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
            Thread::Sleep(250); // avoid aggregation of previous events
            FileSystemWatcher::FolderWatcher watcher1, watcher2;
            struct Params
            {
                int changes1 = 0;
                int changes2 = 0;
            } params;
            auto lambda1 = [&](const FileSystemWatcher::Notification& notification)
            {
                if (notification.operation == FileSystemWatcher::Operation::AddRemoveRename)
                {
                    params.changes1++;
                }
            };
            SC_TEST_EXPECT(fileEventsWatcher.watch(watcher1, path1.view(), move(lambda1)));
            auto lambda2 = [&](const FileSystemWatcher::Notification& notification)
            {
                if (notification.operation == FileSystemWatcher::Operation::AddRemoveRename)
                {
                    params.changes2++;
                }
            };
// Sleeps exist because Windows does not recognize events properly if we're running too fast.
// Additionally we explicitly create and delete files and only listen for Operation::AddRemoveRename
// because in some cases we also get modified Operation::Modified
#if SC_PLATFORM_WINDOWS
            constexpr int waitForEventsTimeout = 200;
#else
            constexpr int waitForEventsTimeout = 100;
#endif
            SC_TEST_EXPECT(fileEventsWatcher.watch(watcher2, path2.view(), move(lambda2)));
            FileSystem fs1;
            FileSystem fs2;
            SC_TEST_EXPECT(fs1.init(path1.view()));
            SC_TEST_EXPECT(fs2.init(path2.view()));

            SC_TEST_EXPECT(fs1.write("salve.txt", "content"));

            Thread::Sleep(waitForEventsTimeout);
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(fs2.write("a_tutti.txt", "content"));

            Thread::Sleep(waitForEventsTimeout);
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(params.changes1 == 1);
            SC_TEST_EXPECT(params.changes2 == 1);
            SC_TEST_EXPECT(watcher2.stopWatching());
            SC_TEST_EXPECT(fs1.removeFile("salve.txt"));
            SC_TEST_EXPECT(fs2.removeFile("a_tutti.txt"));

            Thread::Sleep(waitForEventsTimeout);
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(params.changes1 == 2);
            SC_TEST_EXPECT(params.changes2 == 1);
            SC_TEST_EXPECT(watcher1.stopWatching());
            SC_TEST_EXPECT(fs1.write("salve.txt", "content NEW YEAH"));
            SC_TEST_EXPECT(fs2.write("a_tutti.txt", "content NEW YEAH"));

            SC_TEST_EXPECT(eventLoop.runNoWait());
            SC_TEST_EXPECT(params.changes1 == 2);
            SC_TEST_EXPECT(params.changes2 == 1);

            auto lambda3 = [&](const FileSystemWatcher::Notification& notification)
            {
                if (notification.operation == FileSystemWatcher::Operation::AddRemoveRename)
                {
                    params.changes2++;
                }
            };
            SC_TEST_EXPECT(fileEventsWatcher.watch(watcher2, path2.view(), move(lambda3)));
            SC_TEST_EXPECT(fs2.removeFile("a_tutti.txt"));
            Thread::Sleep(waitForEventsTimeout);
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(params.changes1 == 2);
            SC_TEST_EXPECT(params.changes2 == 2);

            SC_TEST_EXPECT(fileEventsWatcher.close());
            SC_TEST_EXPECT(fs1.removeFile("salve.txt"));
            SC_TEST_EXPECT(fs.removeEmptyDirectory(path1.view()));
            SC_TEST_EXPECT(fs.removeEmptyDirectory(path2.view()));
        }
    }
};

namespace SC
{
void runFileSystemWatcherTest(SC::TestReport& report) { FileSystemWatcherTest test(report); }
} // namespace SC

namespace SC
{
Result fileSystemWatcherEventLoopRunnerSnippet(AsyncEventLoop& eventLoop, Console& console)
{
    //! [fileSystemWatcherEventLoopRunnerSnippet]
    // Initialize the FileSystemWatcher
    FileSystemWatcher fileSystemWatcher;

    FileSystemWatcher::EventLoopRunner eventLoopRunner(eventLoop);
    SC_TRY(fileSystemWatcher.init(eventLoopRunner));

    // Setup notification callback
    auto onFileModified = [&](const FileSystemWatcher::Notification& notification)
    {
        // This callback will be called from the thread calling AsyncEventLoop::run
        SmallString<1024> buffer;
        StringView        fullPath;
        if (notification.getFullPath(buffer, fullPath))
        {
            switch (notification.operation)
            {
            case FileSystemWatcher::Operation::Modified: // File has been modified
                console.print("Modified {} {}\n", notification.relativePath, fullPath);
                break;
            case FileSystemWatcher::Operation::AddRemoveRename: // File was added / removed
                console.print("AddRemoveRename {} {}\n", notification.relativePath, fullPath);
                break;
            }
        }
    };

    // Start watching a specific folder
    FileSystemWatcher::FolderWatcher folderWatcher;
    SC_TRY(fileSystemWatcher.watch(folderWatcher, "/path/to/dir", onFileModified));

    // ...
    // At a later point when there is no more need of watching the folder
    SC_TRY(folderWatcher.stopWatching());

    // ...
    // When all watchers have been unwatched and to dispose all system resources
    SC_TRY(fileSystemWatcher.close());
    //! [fileSystemWatcherEventLoopRunnerSnippet]
    return Result(true);
}

Result fileSystemWatcherThreadRunnerSnippet(Console& console)
{
    //! [fileSystemWatcherThreadRunnerSnippet]
    // Initialize the FileSystemWatcher
    FileSystemWatcher::ThreadRunner threadRunner; // <--- The thread runner

    FileSystemWatcher fileSystemWatcher;
    SC_TRY(fileSystemWatcher.init(threadRunner));

    // Setup notification callback
    auto onFileModified = [&](const FileSystemWatcher::Notification& notification)
    {
        // Warning! This callback is called from a background thread!
        // Make sure to do proper synchronization!
        SmallString<1024> buffer;
        StringView        fullPath;
        if (notification.getFullPath(buffer, fullPath))
        {
            switch (notification.operation)
            {
            case FileSystemWatcher::Operation::Modified: // File has been modified
                console.print("Modified {} {}\n", notification.relativePath, fullPath);
                break;
            case FileSystemWatcher::Operation::AddRemoveRename: // File was added / removed
                console.print("AddRemoveRename {} {}\n", notification.relativePath, fullPath);
                break;
            }
        }
    };

    // Start watching a specific folder
    FileSystemWatcher::FolderWatcher folderWatcher;
    SC_TRY(fileSystemWatcher.watch(folderWatcher, "/path/to/dir", onFileModified));

    // ...
    // At a later point when there is no more need of watching the folder
    SC_TRY(folderWatcher.stopWatching());

    // ...
    // When all watchers have been unwatched and to dispose all system resources
    SC_TRY(fileSystemWatcher.close());
    //! [fileSystemWatcherThreadRunnerSnippet]
    return Result(true);
}
} // namespace SC
