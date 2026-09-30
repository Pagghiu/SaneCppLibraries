// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include <memory.h> // memcmp

#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/FileSystemWatcher/FileSystemWatcher.h"
#include "Libraries/FileSystemWatcher/FileSystemWatcherErrorFormatter.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/Console.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/Threading.h"

namespace SC
{
struct FileSystemWatcherTest;
}

struct SC::FileSystemWatcherTest : public SC::TestCase
{
    FileSystemWatcherTest(SC::TestReport& report) : TestCase(report, "FileSystemWatcherTest")
    {
        using namespace SC;
        const StringView appDirectory = report.applicationRootDirectory.view();
        initClose();
        structuredErrorsAndFormatter(appDirectory);
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

    void structuredErrorsAndFormatter(const StringView appDirectory)
    {
        if (not test_section("structured errors and formatter"))
            return;

        static_assert(sizeof(ResultFileSystemWatcher) == 16, "ResultFileSystemWatcher must remain 16 bytes");
        static_assert(__is_standard_layout(ResultFileSystemWatcher),
                      "ResultFileSystemWatcher must remain standard-layout");
        static_assert(TypeTraits::IsTriviallyCopyable<ResultFileSystemWatcher>::value,
                      "ResultFileSystemWatcher must remain trivially copyable");
        static_assert(static_cast<uint32_t>(FileSystemWatcherError::NotInitialized) == 1,
                      "FileSystemWatcher error values are append-only");
        static_assert(static_cast<uint32_t>(FileSystemWatcherErrorDetail::None) == 0,
                      "FileSystemWatcher detail zero is reserved for no detail");
        static_assert(FileSystemWatcherResultCategory.value == 6, "FileSystemWatcher category is registry value 6");

        FileSystemWatcher                fileEventsWatcher;
        FileSystemWatcher::FolderWatcher watcher;

        const ResultFileSystemWatcher notInitialized = fileEventsWatcher.watch(watcher, appDirectory);
        SC_TEST_EXPECT(notInitialized.isError(FileSystemWatcherError::NotInitialized));
        SC_TEST_EXPECT(notInitialized.detail == FileSystemWatcherErrorDetail::None);
        SC_TEST_EXPECT(notInitialized.nativeError == 0);

        const ResultFileSystemWatcher notWatching = watcher.stopWatching();
        SC_TEST_EXPECT(notWatching.isError(FileSystemWatcherError::NotWatching));

        ResultFileSystemWatcher detailed(FileSystemWatcherError::WatchSetupFailed,
                                         FileSystemWatcherErrorDetail::LinuxAddRootWatch, 12345);
        const Result            plain = detailed;
        SC_TEST_EXPECT(plain.isError(FileSystemWatcherResultCategory, FileSystemWatcherError::WatchSetupFailed));

        const ResultFileSystemWatcher fromPlain(plain);
        SC_TEST_EXPECT(fromPlain.detail == FileSystemWatcherErrorDetail::None);
        SC_TEST_EXPECT(fromPlain.nativeError == 0);

        const Result                  foreignResult = Result::Error(ResultCategory(99), 17);
        const ResultFileSystemWatcher foreign(foreignResult);
        SC_TEST_EXPECT(not foreign);
        SC_TEST_EXPECT(foreign.toResult().category() == ResultCategory(99));
        SC_TEST_EXPECT(foreign.detail == FileSystemWatcherErrorDetail::None);
        SC_TEST_EXPECT(foreign.nativeError == 0);

        char                    formattedMessage[128];
        const ResultErrorFormat formatted = formatFileSystemWatcherError(detailed, formattedMessage);
        SC_TEST_EXPECT(formatted);
        SC_TEST_EXPECT(formatted.requiredCapacity <= sizeof(formattedMessage));
        SC_TEST_EXPECT(StringView::fromNullTerminated(formattedMessage, StringEncoding::Ascii).containsString("12345"));

        const ResultErrorFormat foreignFormat = formatFileSystemWatcherError(foreignResult, {});
        SC_TEST_EXPECT(foreignFormat.status == ResultErrorFormatStatus::ForeignCategory);

        FileSystemWatcher::ThreadRunner runner;
        SC_TEST_EXPECT(fileEventsWatcher.init(runner));
        SC_TEST_EXPECT(fileEventsWatcher.watch(watcher, appDirectory));
        const ResultFileSystemWatcher alreadyWatching = fileEventsWatcher.watch(watcher, appDirectory);
        SC_TEST_EXPECT(alreadyWatching.isError(FileSystemWatcherError::AlreadyWatching));
        SC_TEST_EXPECT(fileEventsWatcher.close());
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
            FileSystem fs;
            SC_TEST_EXPECT(fs.init(appDirectory));

            constexpr StringView watchDirectoryName = "FileSystemWatcherThreadRunner";
            SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(watchDirectoryName));
            SC_TEST_EXPECT(fs.removeFileIfExists("FileSystemWatcherThreadRunner/test.txt"));

            SmallStringNative<1024> path;
            SC_TEST_EXPECT(Path::join(path, {appDirectory, watchDirectoryName}));
            params.appDirectory = path.view();

            auto lambda = [&](const FileSystemWatcher::Notification& notification)
            {
                SmallStringNative<1024> expectedBuffer = StringEncoding::Native;

                // Some backends can report directory-level notifications before the file event.
                if ("test.txt"_a8 != notification.relativePath)
                {
                    return;
                }

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
                StringPath fullPath;
                SC_TEST_EXPECT(notification.getFullPath(fullPath));

                constexpr native_char_t nativeSep = Path::Separator;
                SC_TEST_EXPECT(
                    StringBuilder::format(expectedBuffer, "{}{}{}", params.appDirectory, nativeSep, "test.txt"));
                SC_TEST_EXPECT(fullPath.view() == expectedBuffer.view());
                params.eventObject.signal();
            };

            // Coverage and package caches can create deep folder trees under appDirectory.
            // Provide a larger Linux sub-folder relative paths buffer to avoid spurious overflows.
            char subFolderRelativePathsBuffer[32 * 1024];

            FileSystemWatcher::FolderWatcher watcher(subFolderRelativePathsBuffer);
            // We save the results and expect them after the wait to avoid Thread Sanitizer issues
            // due to the SC_TEST_EXPECT calls inside the lambda that runs in the thread
            watcher.notifyCallback  = lambda;
            const Result res        = fileEventsWatcher.watch(watcher, path.view());
            const bool   fsWriteRes = static_cast<bool>(fs.write("FileSystemWatcherThreadRunner/test.txt", "content"));
            SC_TEST_EXPECT(fsWriteRes);
            SC_TEST_EXPECT(res);
            params.eventObject.wait();
            SC_TEST_EXPECT(params.changes > 0);
            SC_TEST_EXPECT(fileEventsWatcher.close());
            SC_TEST_EXPECT(params.callbackThreadID != Thread::CurrentThreadID());
            SC_TEST_EXPECT(fs.removeFile("FileSystemWatcherThreadRunner/test.txt"));
            SC_TEST_EXPECT(fs.removeEmptyDirectory(watchDirectoryName));
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
