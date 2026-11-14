// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include <memory.h> // memcmp

#include "Libraries/Common/Deferred.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/FileSystemWatcher/FileSystemWatcher.h"
#include "Libraries/FileSystemWatcher/FileSystemWatcherErrorFormatter.h"
#include "Libraries/FileSystemWatcher/Internal/FileSystemWatcherBSDPathState.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/Console.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/Threading.h"

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

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
        boundedPathState();
        boundedPathMoves();
        structuredErrorsAndFormatter(appDirectory);
        failedRootCanBeReused(appDirectory);
        threadRunner(appDirectory);
#if SC_PLATFORM_WINDOWS
        windowsThreadRunnerMultipleRoots(appDirectory);
#endif
#if SC_PLATFORM_NETBSD || SC_PLATFORM_FREEBSD
        bsdThreadCallbackLifecycle(appDirectory);
#endif
    }

    void boundedPathMoves()
    {
        if (not test_section("bounded watcher path moves"))
            return;

        using State = FileSystemWatcherBSDPathState;
        using Error = State::Error;
        State::Entry records[8];
        char         names[24];
        State        state;
        auto         name = [](const char* value) { return Span<const char>{value, strlen(value)}; };
        auto         info = [](int descriptor, uintptr_t token, bool directory)
        { return State::NativeInfo{descriptor, token, 1, static_cast<uint64_t>(descriptor), directory}; };
        size_t root = State::InvalidIndex, left = root, right = root, branch = root, leaf = root, sibling = root;
        SC_TEST_EXPECT(state.reset(records, names) == Error::None);
        SC_TEST_EXPECT(state.add(State::InvalidIndex, {}, info(20, 10, true), root) == Error::None);
        SC_TEST_EXPECT(state.add(root, name("left"), info(21, 11, true), left) == Error::None);
        SC_TEST_EXPECT(state.add(root, name("right"), info(22, 12, true), right) == Error::None);
        SC_TEST_EXPECT(state.add(left, name("src"), info(23, 13, true), branch) == Error::None);
        SC_TEST_EXPECT(state.add(branch, name("inside"), info(24, 14, false), leaf) == Error::None);
        SC_TEST_EXPECT(state.add(right, name("other"), info(25, 15, false), sibling) == Error::None);

        char path[32] = {};
        SC_TEST_EXPECT(state.updatePath(branch, left, name("longer")) == Error::NameCapacity);
        SC_TEST_EXPECT(state.copyRelativePath(leaf, path) == Error::None and strcmp(path, "left/src/inside") == 0);
        SC_TEST_EXPECT(state.updatePath(branch, left, name("s")) == Error::None);
        SC_TEST_EXPECT(state.copyRelativePath(leaf, path) == Error::None and strcmp(path, "left/s/inside") == 0);
        SC_TEST_EXPECT(state.copyRelativePath(sibling, path) == Error::None and strcmp(path, "right/other") == 0);
        SC_TEST_EXPECT(state.updatePath(branch, right, name("foo")) == Error::None);
        SC_TEST_EXPECT(state.copyRelativePath(leaf, path) == Error::None and strcmp(path, "right/foo/inside") == 0);
        SC_TEST_EXPECT(state.findChild(left, name("s")) == State::InvalidIndex);
        SC_TEST_EXPECT(state.findChild(right, name("foo")) == branch);
        SC_TEST_EXPECT(state.findToken(13) == branch and state.findToken(14) == leaf);
        SC_TEST_EXPECT(state.get(branch) != nullptr and state.get(branch)->native.descriptor == 23);
        SC_TEST_EXPECT(state.updatePath(branch, right, name("other")) == Error::DuplicateName);
        SC_TEST_EXPECT(state.updatePath(right, branch, name("cycle")) == Error::InvalidParent);
        SC_TEST_EXPECT(state.updatePath(root, right, name("root")) == Error::InvalidParent);
        SC_TEST_EXPECT(state.updatePath(branch, leaf, name("child")) == Error::InvalidParent);
        SC_TEST_EXPECT(state.updatePath(branch, right, name(".")) == Error::InvalidName);
        SC_TEST_EXPECT(state.updatePath(branch, right, {names, 4}) == Error::InvalidName);
        SC_TEST_EXPECT(state.updatePath(State::InvalidIndex, right, name("bad")) == Error::NotFound);
        SC_TEST_EXPECT(state.copyRelativePath(leaf, path) == Error::None and strcmp(path, "right/foo/inside") == 0);
        SC_TEST_EXPECT(state.updatePath(branch, right, name("foo")) == Error::None);
        SC_TEST_EXPECT(state.updatePath(branch, right, name("bar")) == Error::None);
        SC_TEST_EXPECT(state.copyRelativePath(leaf, path) == Error::None and strcmp(path, "right/bar/inside") == 0);
        SC_TEST_EXPECT(state.copyRelativePath(sibling, path) == Error::None and strcmp(path, "right/other") == 0);
        SC_TEST_EXPECT(state.size() == 6);
    }

    void boundedPathState()
    {
        if (not test_section("bounded watcher path state"))
            return;

        using State = FileSystemWatcherBSDPathState;
        using Error = State::Error;
        State::Entry records[5];
        char         names[14];
        State        state;
        auto         name = [](const char* value) { return Span<const char>{value, strlen(value)}; };
        auto         info = [](int descriptor, uintptr_t token, bool directory)
        { return State::NativeInfo{descriptor, token, 1, static_cast<uint64_t>(descriptor), directory}; };

        size_t root = State::InvalidIndex, alpha = root, directory = root, nested = root, duplicate = root;
        size_t untouched = 123;
        SC_TEST_EXPECT(state.add(root, {}, info(10, 1, true), untouched) == Error::NotInitialized);
        SC_TEST_EXPECT(untouched == 123);
        SC_TEST_EXPECT(state.reset({nullptr, 5}, names) == Error::InvalidStorage);
        SC_TEST_EXPECT(state.reset(records, {nullptr, 1}) == Error::InvalidStorage);
        SC_TEST_EXPECT(state.reset(records, names) == Error::None);
        SC_TEST_EXPECT(state.add(State::InvalidIndex, {}, info(10, 1, true), root) == Error::None);
        SC_TEST_EXPECT(state.add(root, name("alpha"), info(11, 2, false), alpha) == Error::None);
        SC_TEST_EXPECT(state.add(root, name("dir"), info(12, 3, true), directory) == Error::None);
        SC_TEST_EXPECT(state.add(directory, name("x"), info(13, 4, false), nested) == Error::None);
        SC_TEST_EXPECT(state.add(root, name("x"), info(14, 5, false), duplicate) == Error::None);
        SC_TEST_EXPECT(state.findChild(root, name("x")) == duplicate);
        SC_TEST_EXPECT(state.findChild(directory, name("x")) == nested);
        SC_TEST_EXPECT(state.findToken(4) == nested);
        SC_TEST_EXPECT(state.findToken(0) == State::InvalidIndex);
        SC_TEST_EXPECT(state.add(root, name("x"), info(15, 6, false), untouched) == Error::DuplicateName);
        SC_TEST_EXPECT(state.add(root, name("z"), info(15, 4, false), untouched) == Error::DuplicateToken);
        SC_TEST_EXPECT(state.add(root, name("../bad"), info(15, 6, false), untouched) == Error::InvalidName);
        SC_TEST_EXPECT(state.add(root, name("."), info(15, 6, false), untouched) == Error::InvalidName);
        const char embeddedNul[] = {'a', '\0', 'b'};
        SC_TEST_EXPECT(state.add(root, {embeddedNul, sizeof(embeddedNul)}, info(15, 6, false), untouched) ==
                       Error::InvalidName);
        SC_TEST_EXPECT(state.add(alpha, name("z"), info(15, 6, false), untouched) == Error::InvalidParent);
        SC_TEST_EXPECT(state.add(root, name("z"), info(15, 6, false), untouched) == Error::RecordCapacity);
        SC_TEST_EXPECT(untouched == 123 and state.size() == 5);
        SC_TEST_EXPECT(state.reset(records, names) == Error::NotEmpty);
        SC_TEST_EXPECT(state.release() == Error::NotEmpty);

        char path[16] = {};
        SC_TEST_EXPECT(state.copyRelativePath(root, path) == Error::None and strcmp(path, "") == 0);
        SC_TEST_EXPECT(state.copyRelativePath(nested, path) == Error::None);
        SC_TEST_EXPECT(strcmp(path, "dir/x") == 0);
        char small[] = "keep";
        SC_TEST_EXPECT(state.copyRelativePath(nested, {small, 3}) == Error::OutputTooSmall);
        SC_TEST_EXPECT(strcmp(small, "keep") == 0);
        State::NativeInfo retired;
        SC_TEST_EXPECT(state.removeLeaf(directory, retired) == Error::HasChildren);
        SC_TEST_EXPECT(state.removeLeaf(alpha, retired) == Error::None);
        SC_TEST_EXPECT(retired.descriptor == 11 and retired.token == 2);
        SC_TEST_EXPECT(state.findToken(2) == State::InvalidIndex);
        SC_TEST_EXPECT(state.copyRelativePath(nested, path) == Error::None and strcmp(path, "dir/x") == 0);
        SC_TEST_EXPECT(state.add(root, name("a very long name"), info(16, 6, false), untouched) == Error::NameCapacity);
        size_t replacement = State::InvalidIndex;
        SC_TEST_EXPECT(state.add(root, name("omega"), info(16, 6, false), replacement) == Error::None);
        SC_TEST_EXPECT(replacement == alpha and state.findToken(6) == replacement);
        SC_TEST_EXPECT(state.copyRelativePath(replacement, path) == Error::None and strcmp(path, "omega") == 0);
        SC_TEST_EXPECT(state.removeLeaf(nested, retired) == Error::None);
        SC_TEST_EXPECT(state.removeLeaf(directory, retired) == Error::None);
        SC_TEST_EXPECT(state.removeLeaf(duplicate, retired) == Error::None);
        SC_TEST_EXPECT(state.removeLeaf(replacement, retired) == Error::None);
        SC_TEST_EXPECT(state.removeLeaf(root, retired) == Error::None and state.size() == 0);
        SC_TEST_EXPECT(state.reset(records, names) == Error::None);
        SC_TEST_EXPECT(state.release() == Error::None);
        SC_TEST_EXPECT(state.findToken(1) == State::InvalidIndex and state.get(0) == nullptr);
        SC_TEST_EXPECT(state.add(State::InvalidIndex, {}, info(10, 1, true), untouched) == Error::NotInitialized);

        FileSystemWatcherBSDRegistrationTokens tokens(UINTPTR_MAX - 1);

        uintptr_t token = 0;
        SC_TEST_EXPECT(tokens.next(token) and token == UINTPTR_MAX);
        SC_TEST_EXPECT(not tokens.next(token) and token == 0);
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

        FileSystemWatcher::Notification notification;
        SC_TEST_EXPECT(notification.kind == FileSystemWatcher::NotificationKind::Change);
        SC_TEST_EXPECT(notification.failure);
        const ResultFileSystemWatcher monitoringFailure(FileSystemWatcherError::MonitoringFailed,
                                                        FileSystemWatcherErrorDetail::LinuxReadEvents, 5);
        const ResultErrorFormat monitoringFormat = formatFileSystemWatcherError(monitoringFailure, formattedMessage);
        SC_TEST_EXPECT(monitoringFormat);
        SC_TEST_EXPECT(StringView::fromNullTerminated(formattedMessage, StringEncoding::Ascii)
                           .containsString("Linux read events"));
        const ResultFileSystemWatcher waitFailure(FileSystemWatcherError::MonitoringFailed,
                                                  FileSystemWatcherErrorDetail::LinuxWaitForEvents, 5);
        const ResultErrorFormat       waitFormat = formatFileSystemWatcherError(waitFailure, formattedMessage);
        SC_TEST_EXPECT(waitFormat);
        SC_TEST_EXPECT(StringView::fromNullTerminated(formattedMessage, StringEncoding::Ascii)
                           .containsString("Linux wait for events"));
        const ResultFileSystemWatcher limitedCoverage(FileSystemWatcherError::CoverageLimited,
                                                      FileSystemWatcherErrorDetail::BSDPassiveEntries);
        SC_TEST_EXPECT(formatFileSystemWatcherError(limitedCoverage, formattedMessage));
        SC_TEST_EXPECT(StringView::fromNullTerminated(formattedMessage, StringEncoding::Ascii)
                           .containsString("BSD passive entries"));

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
#if SC_PLATFORM_WINDOWS
        // A leaked directory handle also prevents an exclusive read of the rejected file.
        const HANDLE exclusiveHandle = ::CreateFileW(filePath.view().getNullTerminatedNative(), GENERIC_READ, 0,
                                                     nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        SC_TEST_EXPECT(exclusiveHandle != INVALID_HANDLE_VALUE);
        if (exclusiveHandle != INVALID_HANDLE_VALUE)
            SC_TEST_EXPECT(::CloseHandle(exclusiveHandle) != FALSE);
#endif
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

#if SC_PLATFORM_WINDOWS
    void windowsThreadRunnerMultipleRoots(StringView appDirectory)
    {
        if (not test_section("Windows ThreadRunner multiple roots"))
            return;

        FileSystem fs;
        SC_TEST_EXPECT(fs.init(appDirectory));
        StringPath fixture;
        bool       created = false;
        for (size_t attempt = 0; attempt < 128 and not created; ++attempt)
        {
            if (not StringBuilder::format(fixture, "__sc thread watcher {}", attempt))
                break;
            created = static_cast<bool>(fs.makeDirectory(fixture.view()));
        }
        SC_TEST_EXPECT(created);
        if (not created)
            return;
        auto cleanup = MakeDeferred([&] { (void)fs.removeDirectoryRecursive(fixture.view()); });

        struct Root
        {
            StringPath                       path;
            FileSystemWatcher::FolderWatcher watch;
            HANDLE                           notification = nullptr;
        } roots[2];
        auto closeEvents = MakeDeferred(
            [&]
            {
                for (auto& root : roots)
                    if (root.notification != nullptr)
                        ::CloseHandle(root.notification);
            });
        for (size_t index = 0; index < 2; ++index)
        {
            auto& root = roots[index];
            SC_TEST_EXPECT(StringBuilder::format(root.path, "{}/{}/root{}", appDirectory, fixture.view(), index));
            SC_TEST_EXPECT(fs.makeDirectory(root.path.view()));
            root.notification = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
            SC_TEST_EXPECT(root.notification != nullptr);
            root.watch.notifyCallback = [&root](const FileSystemWatcher::Notification& event)
            {
                if (event.kind == FileSystemWatcher::NotificationKind::Change and
                    event.operation == FileSystemWatcher::Operation::AddRemoveRename and
                    StringView(event.relativePath).startsWith("marker"))
                    ::SetEvent(root.notification);
            };
        }

        FileSystemWatcher::ThreadRunner runner;
        FileSystemWatcher               owner;
        SC_TEST_EXPECT(owner.init(runner));
        auto closeOwner = MakeDeferred([&] { (void)owner.close(); });
        auto write      = [&](Root& root, StringView name)
        {
            StringPath path;
            return Path::join(path, {root.path.view(), name}) and fs.writeString(path.view(), "marker");
        };

        SC_TEST_EXPECT(owner.watch(roots[0].watch, roots[0].path.view()));
        SC_TEST_EXPECT(owner.watch(roots[1].watch, roots[1].path.view()));
        SC_TEST_EXPECT(write(roots[1], "marker-second.txt"));
        SC_TEST_EXPECT(::WaitForSingleObject(roots[1].notification, 5000) == WAIT_OBJECT_0);
        SC_TEST_EXPECT(write(roots[0], "marker-first.txt"));
        SC_TEST_EXPECT(::WaitForSingleObject(roots[0].notification, 5000) == WAIT_OBJECT_0);

        SC_TEST_EXPECT(roots[0].watch.stopWatching());
        SC_TEST_EXPECT(write(roots[1], "marker-remaining.txt"));
        SC_TEST_EXPECT(::WaitForSingleObject(roots[1].notification, 5000) == WAIT_OBJECT_0);
        SC_TEST_EXPECT(owner.watch(roots[0].watch, roots[0].path.view()));
        SC_TEST_EXPECT(write(roots[0], "marker-reused.txt"));
        SC_TEST_EXPECT(::WaitForSingleObject(roots[0].notification, 5000) == WAIT_OBJECT_0);
        SC_TEST_EXPECT(owner.close());
        SC_TEST_EXPECT(owner.init(runner));
        SC_TEST_EXPECT(owner.watch(roots[1].watch, roots[1].path.view()));
        SC_TEST_EXPECT(write(roots[1], "marker-reopened.txt"));
        SC_TEST_EXPECT(::WaitForSingleObject(roots[1].notification, 5000) == WAIT_OBJECT_0);
        SC_TEST_EXPECT(owner.close());
        closeOwner.disarm();
    }
#endif

#if SC_PLATFORM_NETBSD || SC_PLATFORM_FREEBSD
    void bsdThreadCallbackLifecycle(const StringView appDirectory)
    {
        if (not test_section("BSD thread callback lifecycle"))
            return;

        FileSystem fs;
        SC_TEST_EXPECT(fs.init(appDirectory));
        SC_TEST_EXPECT(fs.makeDirectory("__bsd_thread_callback"));
        auto removeDirectory = MakeDeferred([&] { (void)fs.removeDirectoryRecursive("__bsd_thread_callback"); });

        StringPath directory;
        SC_TEST_EXPECT(Path::join(directory, {appDirectory, "__bsd_thread_callback"}));
        FileSystemWatcher::ThreadRunner runner;

        FileSystemWatcher owner;
        SC_TEST_EXPECT(owner.init(runner));
        auto closeOwner = MakeDeferred([&] { (void)owner.close(); });

        FileSystemWatcher::FolderWatcher watcher;
        struct State
        {
            FileSystemWatcher*                owner   = nullptr;
            FileSystemWatcher::FolderWatcher* watcher = nullptr;

            EventObject completed;
            bool        closeRejected = false;
            bool        stopped       = false;
        } state{&owner, &watcher};
        watcher.notifyCallback = [&state](const FileSystemWatcher::Notification& event)
        {
            if (event.kind != FileSystemWatcher::NotificationKind::Change)
                return;
            const ResultFileSystemWatcher closed = state.owner->close();
            state.closeRejected                  = closed.isError(FileSystemWatcherError::CloseFailed) and
                                  closed.detail == FileSystemWatcherErrorDetail::PosixPthreadJoin;
            state.stopped = static_cast<bool>(state.watcher->stopWatching());
            state.completed.signal();
        };
        SC_TEST_EXPECT(owner.watch(watcher, directory.view()));
        SC_TEST_EXPECT(fs.write("__bsd_thread_callback/file", "content"));
        state.completed.wait();
        SC_TEST_EXPECT(state.closeRejected);
        SC_TEST_EXPECT(state.stopped);
        SC_TEST_EXPECT(watcher.stopWatching().isError(FileSystemWatcherError::NotWatching));
        SC_TEST_EXPECT(owner.close());
        closeOwner.disarm();
    }
#endif
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
