// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include <memory.h> // memcmp

#include "Libraries/Common/Deferred.h"
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
        failedRootCanBeReused(appDirectory);
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

        FileSystemWatcher                fileEventsWatcher;
        FileSystemWatcher::FolderWatcher watcher;
        watcher.notifyCallback = [](const FileSystemWatcher::Notification&) {};

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

        char longPath[StringPath::MaxPath + 2];
        memset(longPath, 'a', sizeof(longPath) - 1);
        longPath[sizeof(longPath) - 1] = '\0';
        const ResultFileSystemWatcher rejected =
            fileEventsWatcher.watch(watcher, StringSpan::fromNullTerminated(longPath, StringEncoding::Ascii));
        SC_TEST_EXPECT(rejected.isError(FileSystemWatcherError::PathPreparationFailed));
        SC_TEST_EXPECT(rejected.detail == FileSystemWatcherErrorDetail::AssignWatchPath);

        SC_TEST_EXPECT(fileEventsWatcher.watch(watcher, appDirectory));
        const ResultFileSystemWatcher alreadyWatching = fileEventsWatcher.watch(watcher, appDirectory);
        SC_TEST_EXPECT(alreadyWatching.isError(FileSystemWatcherError::AlreadyWatching));
        SC_TEST_EXPECT(fileEventsWatcher.close());
    }

    void failedRootCanBeReused(const StringView appDirectory)
    {
        if (not test_section("rejected root remains reusable"))
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
            if (not StringBuilder::format(directoryName, "__sc watcher root {}", attempt))
                break;
            created = static_cast<bool>(fs.makeDirectory(directoryName.view()));
        }
        SC_TEST_EXPECT(created);
        if (not created)
            return;
        auto removeDirectory = MakeDeferred([&] { (void)fs.removeDirectoryRecursive(directoryName.view()); });

        StringPath directoryPath;
        StringPath missingPath;
        StringPath filePath;
        const bool pathsReady = Path::join(directoryPath, {appDirectory, directoryName.view()}) and
                                Path::join(missingPath, {directoryPath.view(), "missing"}) and
                                Path::join(filePath, {directoryPath.view(), "file"});
        SC_TEST_EXPECT(pathsReady);
        if (not pathsReady)
            return;
        const ResultFileSystem written = fs.writeString(filePath.view(), "fixture");
        SC_TEST_EXPECT(written);
        if (not written)
            return;

        FileSystemWatcher::ThreadRunner  runner;
        FileSystemWatcher                fileEventsWatcher;
        FileSystemWatcher::FolderWatcher watcher;
        watcher.notifyCallback                    = [](const FileSystemWatcher::Notification&) {};
        const ResultFileSystemWatcher initialized = fileEventsWatcher.init(runner);
        SC_TEST_EXPECT(initialized);
        if (not initialized)
            return;
        auto closeWatcher = MakeDeferred([&] { (void)fileEventsWatcher.close(); });

        const ResultFileSystemWatcher missing = fileEventsWatcher.watch(watcher, missingPath.view());
        SC_TEST_EXPECT(missing.isError(FileSystemWatcherError::WatchSetupFailed));
        SC_TEST_EXPECT(watcher.stopWatching().isError(FileSystemWatcherError::NotWatching));
        const ResultFileSystemWatcher file = fileEventsWatcher.watch(watcher, filePath.view());
        SC_TEST_EXPECT(file.isError(FileSystemWatcherError::WatchSetupFailed));
        SC_TEST_EXPECT(watcher.stopWatching().isError(FileSystemWatcherError::NotWatching));
#if SC_PLATFORM_LINUX
        StringPath childPath;
        SC_TEST_EXPECT(Path::join(childPath, {directoryPath.view(), "child"}));
        SC_TEST_EXPECT(fs.makeDirectory(childPath.view()));
        StringPath grandchildPath;
        SC_TEST_EXPECT(Path::join(grandchildPath, {childPath.view(), "grandchild"}));
        SC_TEST_EXPECT(fs.makeDirectory(grandchildPath.view()));
        char                             pathStorage[8];
        FileSystemWatcher::FolderWatcher limitedWatcher(pathStorage);
        limitedWatcher.notifyCallback                 = [](const FileSystemWatcher::Notification&) {};
        const ResultFileSystemWatcher capacityFailure = fileEventsWatcher.watch(limitedWatcher, directoryPath.view());
        SC_TEST_EXPECT(capacityFailure.isError(FileSystemWatcherError::BufferTooSmall));
        SC_TEST_EXPECT(capacityFailure.detail == FileSystemWatcherErrorDetail::LinuxRelativePathStorage);
        const ResultFileSystemWatcher afterFailure = limitedWatcher.stopWatching();
        SC_TEST_EXPECT(afterFailure.isError(FileSystemWatcherError::NotWatching));
        SC_TEST_EXPECT(fs.removeDirectoryRecursive(childPath.view()));
        SC_TEST_EXPECT(fileEventsWatcher.watch(limitedWatcher, directoryPath.view()));
        SC_TEST_EXPECT(limitedWatcher.stopWatching());

        SC_TEST_EXPECT(fs.chmod(directoryPath.view(), 0444u));
        auto restorePermissions = MakeDeferred([&] { (void)fs.chmod(directoryPath.view(), 0755u); });

        FileSystem::FileStat             inaccessibleEntry;
        FileSystemWatcher::FolderWatcher inaccessibleWatcher;
        inaccessibleWatcher.notifyCallback = [](const FileSystemWatcher::Notification&) {};
        // Privileged users may still be able to inspect entries despite the permission bits.
        if (not fs.stat(filePath.view(), inaccessibleEntry))
        {
            const ResultFileSystemWatcher inaccessible =
                fileEventsWatcher.watch(inaccessibleWatcher, directoryPath.view());
            SC_TEST_EXPECT(inaccessible.isError(FileSystemWatcherError::WatchSetupFailed));
            SC_TEST_EXPECT(inaccessible.detail == FileSystemWatcherErrorDetail::LinuxInspectDirectoryEntry);
            SC_TEST_EXPECT(inaccessible.nativeError != 0);
            if (inaccessible)
                SC_TEST_EXPECT(inaccessibleWatcher.stopWatching());
            else
                SC_TEST_EXPECT(inaccessibleWatcher.stopWatching().isError(FileSystemWatcherError::NotWatching));
        }
        else
        {
            report.console.print("Skipping watcher permission check: directory entries remain accessible\n");
        }
        const ResultFileSystem restored = fs.chmod(directoryPath.view(), 0755u);
        SC_TEST_EXPECT(restored);
        if (restored)
            restorePermissions.disarm();
        StringPath danglingPath;
        SC_TEST_EXPECT(Path::join(danglingPath, {directoryPath.view(), "dangling"}));
        SC_TEST_EXPECT(fs.createSymbolicLink(missingPath.view(), danglingPath.view()));
        SC_TEST_EXPECT(fileEventsWatcher.watch(inaccessibleWatcher, directoryPath.view()));
        SC_TEST_EXPECT(inaccessibleWatcher.stopWatching());
        SC_TEST_EXPECT(fs.removeLinkIfExists(danglingPath.view()));
#endif
        SC_TEST_EXPECT(fs.removeFile(filePath.view()));
        SC_TEST_EXPECT(fs.writeString(filePath.view(), "replacement"));
        SC_TEST_EXPECT(fileEventsWatcher.watch(watcher, directoryPath.view()));
        const ResultFileSystemWatcher closed = fileEventsWatcher.close();
        SC_TEST_EXPECT(closed);
        if (closed)
            closeWatcher.disarm();
        const ResultFileSystem removed = fs.removeDirectoryRecursive(directoryName.view());
        SC_TEST_EXPECT(removed);
        if (removed)
            removeDirectory.disarm();
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
