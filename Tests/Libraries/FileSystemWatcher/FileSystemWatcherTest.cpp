// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/FileSystemWatcher/FileSystemWatcher.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/String.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"

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
                uint64_t changes          = 0;
                uint64_t callbackThreadID = 0;

                StringView  appDirectory;
                EventObject eventObject;
            } params;
            params.appDirectory = appDirectory;

            auto lambda = [&](const FileSystemWatcher::Notification& notification)
            {
                SmallStringNative<1024> expectedBuffer = StringEncoding::Native;

                params.callbackThreadID = Thread::CurrentThreadID();
                params.changes++;
                if (params.changes == 1)
                {
                    SC_TEST_EXPECT(notification.operation == FileSystemWatcher::Operation::AddRemoveRename);
                }
                else
                {
                    SC_TEST_EXPECT(notification.operation == FileSystemWatcher::Operation::Modified);
                }
                SC_TEST_EXPECT(params.appDirectory == notification.basePath);
                // Comparisons must use the same encoding
                SC_TEST_EXPECT("test.txt"_a8 == notification.relativePath);
                StringPath fullPath;
                SC_TEST_EXPECT(notification.getFullPath(fullPath));

                StringBuilder           expected(expectedBuffer);
                constexpr native_char_t nativeSep = Path::Separator;
                SC_TEST_EXPECT(expected.format("{}{}{}", params.appDirectory, nativeSep, "test.txt"));
                SC_TEST_EXPECT(fullPath.view() == expectedBuffer.view());
                params.eventObject.signal();
            };

            FileSystem fs;
            SC_TEST_EXPECT(fs.init(appDirectory));
            if (fs.existsAndIsFile("test.txt"))
            {
                SC_TEST_EXPECT(fs.removeFile("test.txt"));
                Thread::Sleep(200);
            }

            SmallStringNative<1024> path;
            SC_TEST_EXPECT(path.assign(appDirectory));
            FileSystemWatcher::FolderWatcher watcher;
            // We save the results and expect them after the wait to avoid Thread Sanitizer issues
            // due to the SC_TEST_EXPECT calls inside the lambda that runs in the thread
            watcher.notifyCallback  = lambda;
            const Result res        = fileEventsWatcher.watch(watcher, path.view());
            const bool   fsWriteRes = fs.write("test.txt", "content");
            SC_TEST_EXPECT(fsWriteRes);
            SC_TEST_EXPECT(res);
            params.eventObject.wait();
            SC_TEST_EXPECT(params.changes > 0);
            SC_TEST_EXPECT(fileEventsWatcher.close());
            SC_TEST_EXPECT(params.callbackThreadID != Thread::CurrentThreadID());
            SC_TEST_EXPECT(fs.removeFile({"test.txt"_a8}));
        }
    }
};

namespace SC
{
void runFileSystemWatcherTest(SC::TestReport& report) { FileSystemWatcherTest test(report); }
} // namespace SC

namespace SC
{

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
    //! [fileSystemWatcherThreadRunnerSnippet]
    return Result(true);
}
} // namespace SC
