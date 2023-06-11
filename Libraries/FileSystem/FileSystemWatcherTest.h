// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Testing/Test.h"
#include "FileSystem.h"
#include "FileSystemWatcher.h"

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
        if (test_section("Init/Close"))
        {
            FileSystemWatcher::ThreadRunner runner;

            FileSystemWatcher fileEventsWatcher;
            SC_TEST_EXPECT(fileEventsWatcher.init(runner));
            SC_TEST_EXPECT(fileEventsWatcher.close());
        }
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
                constexpr utf_char_t nativeSep = Path::Separator;

                StringNative<1024> fullPathBuffer = StringEncoding::Native;
                StringNative<1024> expectedBuffer = StringEncoding::Native;

                params.callbackThreadID = Thread::CurrentThreadID();
                params.changes++;
                SC_TEST_EXPECT(notification.operation == FileSystemWatcher::Operation::AddRemoveRename);
                SC_TEST_EXPECT(notification.basePath == params.appDirectory);
                // Comparisons must use the same encoding
                SC_TEST_EXPECT(notification.relativePath == "test.txt"_a8);
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
                SC_TEST_EXPECT(fs.removeFile("test.txt"_a8));
                Thread::Sleep(200);
            }

            StringNative<1024> path;
            SC_TEST_EXPECT(path.assign(appDirectory));
            FileSystemWatcher::FolderWatcher watcher;
            // We save the results and expect them after the wait to avoid Thread Sanitizer issues
            // due to the SC_TEST_EXPECT calls inside the labmda that runs in the thread
            const ReturnCode res        = fileEventsWatcher.watch(watcher, path, move(lambda));
            const bool       fsWriteRes = fs.write("test.txt", "content");
            params.eventObject.wait();
            SC_TEST_EXPECT(fsWriteRes);
            SC_TEST_EXPECT(res);
            SC_TEST_EXPECT(params.changes > 0);
            SC_TEST_EXPECT(fileEventsWatcher.close());
            SC_TEST_EXPECT(params.callbackThreadID != Thread::CurrentThreadID());
            SC_TEST_EXPECT(fs.removeFile({"test.txt"_a8}));
        }
        if (test_section("EventLoop"))
        {
            EventLoop eventLoop;
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
                constexpr utf_char_t nativeSep = Path::Separator;

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
            if (not fs.existsAndIsDirectory("dir"_a8))
            {
                SC_TEST_EXPECT(fs.makeDirectory({"dir"_a8}));
            }
            if (fs.existsAndIsFile("dir/test.txt"))
            {
                SC_TEST_EXPECT(fs.removeFile("dir/test.txt"_a8));
            }

            StringNative<1024> path;
            SC_TEST_EXPECT(path.assign(appDirectory));
            FileSystemWatcher::FolderWatcher watcher;
            Thread::Sleep(200); // on macOS watch latency is 500 ms, so we sleep to avoid report of 'dir' creation
            SC_TEST_EXPECT(fileEventsWatcher.watch(watcher, path, move(lambda)));
            SC_TEST_EXPECT(fs.write("dir/test.txt", "content"));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(params.changes == 1);
            SC_TEST_EXPECT(fileEventsWatcher.close());
            SC_TEST_EXPECT(params.callbackThreadID == Thread::CurrentThreadID());
            SC_TEST_EXPECT(fs.removeFile({"dir/test.txt"_a8}));
            SC_TEST_EXPECT(fs.removeEmptyDirectory({"dir"_a8}));
#if SC_PLATFORM_WINDOWS
            // We need sleep otherwise windows ReadDirectoryChangesW on the same directory
            // will report events for the two deletions above in the next test even
            // if we've just closed its handle with CloseHandle and issued a CancelIO! :-|
            Thread::Sleep(100);
#endif
        }
        if (test_section("EventLoop interrupt"))
        {
            EventLoop eventLoop;
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
            SC_TEST_EXPECT(fileEventsWatcher.watch(watcher, path, move(lambda)));
            SC_TEST_EXPECT(fs.write("salve.txt", "content"));
            SC_TEST_EXPECT(fs.write("atutti.txt", "content"));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(params.changes == 1);
            SC_TEST_EXPECT(fileEventsWatcher.close());
            SC_TEST_EXPECT(fs.removeFile({"salve.txt", "atutti.txt"}));
        }
        if (test_section("EventLoop watch/unwatch"))
        {
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            FileSystemWatcher                  fileEventsWatcher;
            FileSystemWatcher::EventLoopRunner runner{eventLoop};
            SC_TEST_EXPECT(fileEventsWatcher.init(runner));
            StringNative<1024> path1, path2;
            SC_TEST_EXPECT(Path::join(path1, {appDirectory, "__test1"_a8}));
            SC_TEST_EXPECT(Path::join(path2, {appDirectory, "__test2"_a8}));
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
            SC_TEST_EXPECT(fileEventsWatcher.watch(watcher1, path1, move(lambda1)));
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
            constexpr int waitForEventsTimeout = 100;
            SC_TEST_EXPECT(fileEventsWatcher.watch(watcher2, path2, move(lambda2)));
            FileSystem fs1;
            FileSystem fs2;
            SC_TEST_EXPECT(fs1.init(path1.view()));
            SC_TEST_EXPECT(fs2.init(path2.view()));

            SC_TEST_EXPECT(fs1.write("salve.txt", "content"));

            Thread::Sleep(waitForEventsTimeout);
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(fs2.write("atutti.txt", "content"));

            Thread::Sleep(waitForEventsTimeout);
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(params.changes1 == 1);
            SC_TEST_EXPECT(params.changes2 == 1);
            SC_TEST_EXPECT(watcher2.unwatch());
            SC_TEST_EXPECT(fs1.removeFile("salve.txt"));
            SC_TEST_EXPECT(fs2.removeFile("atutti.txt"));

            Thread::Sleep(waitForEventsTimeout);
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(params.changes1 == 2);
            SC_TEST_EXPECT(params.changes2 == 1);
            SC_TEST_EXPECT(watcher1.unwatch());
            SC_TEST_EXPECT(fs1.write("salve.txt", "content NEW YEAH"));
            SC_TEST_EXPECT(fs2.write("atutti.txt", "content NEW YEAH"));
            // TODO: we need to add a EventLoop::runNoWait as with no registered handle runOnce will block forever
            AsyncTimeout timeout;
            SC_TEST_EXPECT(eventLoop.startTimeout(timeout, 50_ms, [](AsyncResult&) {}));

            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(params.changes1 == 2);
            SC_TEST_EXPECT(params.changes2 == 1);

            SC_TEST_EXPECT(fileEventsWatcher.watch(watcher2, path2, move(lambda2)));
            SC_TEST_EXPECT(fs2.removeFile("atutti.txt"));
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
