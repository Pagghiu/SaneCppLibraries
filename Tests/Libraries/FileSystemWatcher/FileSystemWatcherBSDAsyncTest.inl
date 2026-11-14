// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include <errno.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>

void SC::FileSystemWatcherAsyncTest::bsdFailedRunnerInitialization(const StringView appDirectory)
{
    if (not test_section("BSD failed runner initialization"))
        return;

    struct FailedRunner : FileSystemWatcher::EventLoopRunner
    {
        int seenDescriptor = -1;

        Result bsdStartSharedFileReadiness() override
        {
            seenDescriptor = notifyFd;
            return Result::Error(ResultCategory(99), 12);
        }
        Result bsdStopSharedFileReadiness() override { return Result(true); }
        bool   detached() const { return fileSystemWatcher == nullptr and notifyFd == -1; }
    } failedRunner;
    FileSystemWatcher             owner;
    const ResultFileSystemWatcher failed = owner.init(failedRunner);
    SC_TEST_EXPECT(not failed);
    SC_TEST_EXPECT(failed.toResult().category() == ResultCategory(99));
    SC_TEST_EXPECT(failed.toResult().errorValue() == 12);
    SC_TEST_EXPECT(failedRunner.detached());
    SC_TEST_EXPECT(failedRunner.seenDescriptor >= 0);
    errno = 0;
    SC_TEST_EXPECT(::fcntl(failedRunner.seenDescriptor, F_GETFD) == -1 and errno == EBADF);
    SC_TEST_EXPECT(owner.close().isError(FileSystemWatcherError::NotInitialized));

    FileSystemWatcher::ThreadRunner runner;
    SC_TEST_EXPECT(owner.init(runner));
    FileSystemWatcher secondOwner;
    SC_TEST_EXPECT(secondOwner.init(runner).isError(FileSystemWatcherError::InitializationFailed));
    FileSystem fs;
    SC_TEST_EXPECT(fs.init(appDirectory));
    SC_TEST_EXPECT(fs.makeDirectory("__bsd_init"));
    auto removeDirectory = MakeDeferred([&] { (void)fs.removeDirectoryRecursive("__bsd_init"); });

    StringPath directory;
    SC_TEST_EXPECT(Path::join(directory, {appDirectory, "__bsd_init"}));
    FileSystemWatcher::FolderWatcher watcher;
    watcher.notifyCallback = [](const FileSystemWatcher::Notification&) {};
    SC_TEST_EXPECT(owner.watch(watcher, directory.view()));
    SC_TEST_EXPECT(owner.close());
    SC_TEST_EXPECT(secondOwner.init(runner));
    SC_TEST_EXPECT(secondOwner.close());

    struct IdleRunner : FileSystemWatcher::EventLoopRunner
    {
        Result bsdStartSharedFileReadiness() override { return Result(true); }
        Result bsdStopSharedFileReadiness() override { return Result(true); }
    } idle;
    SC_TEST_EXPECT(owner.init(idle));
    SC_TEST_EXPECT(secondOwner.init(idle).isError(FileSystemWatcherError::InitializationFailed));
    SC_TEST_EXPECT(owner.close());
    SC_TEST_EXPECT(secondOwner.init(idle));
    SC_TEST_EXPECT(secondOwner.close());
}

void SC::FileSystemWatcherAsyncTest::bsdCapacityAndFailure(const StringView appDirectory)
{
    if (not test_section("BSD capacity and failure"))
        return;

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(appDirectory));
    SC_TEST_EXPECT(fs.makeDirectory("__bsd_capacity"));
    SC_TEST_EXPECT(fs.makeDirectory("__bsd_observer"));
    auto removeDirectories = MakeDeferred(
        [&]
        {
            (void)fs.removeDirectoryRecursive("__bsd_capacity");
            (void)fs.removeDirectoryRecursive("__bsd_observer");
        });
    StringPath directory;
    StringPath observerDirectory;
    SC_TEST_EXPECT(Path::join(directory, {appDirectory, "__bsd_capacity"}));
    SC_TEST_EXPECT(Path::join(observerDirectory, {appDirectory, "__bsd_observer"}));
    SC_TEST_EXPECT(fs.write("__bsd_capacity/a", "a"));
    SC_TEST_EXPECT(fs.write("__bsd_capacity/b", "b"));

    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());
    auto closeLoop = MakeDeferred([&] { (void)eventLoop.close(); });

    FileSystemWatcherAsyncT<AsyncEventLoop> runner;
    runner.init(eventLoop);
    FileSystemWatcher owner;
    SC_TEST_EXPECT(owner.init(runner));
    bool ownerOpen  = true;
    auto closeOwner = MakeDeferred(
        [&]
        {
            if (ownerOpen)
                (void)owner.close();
        });

    constexpr size_t capacityBytes = FileSystemWatcher::FolderWatcher::entryStorageSize(2);

    char storage[capacityBytes + 1];

    FileSystemWatcher::FolderWatcher limited({}, {storage + 1, capacityBytes});
    FileSystemWatcher::FolderWatcher observer;
    struct State
    {
        int limitedStops  = 0;
        int observerStops = 0;

        ResultFileSystemWatcher failure;
    } state;
    limited.notifyCallback = [&state](const FileSystemWatcher::Notification& event)
    {
        if (event.kind == FileSystemWatcher::NotificationKind::WatchStopped)
        {
            ++state.limitedStops;
            state.failure = event.failure;
        }
    };
    observer.notifyCallback = [&state](const FileSystemWatcher::Notification& event)
    {
        if (event.kind == FileSystemWatcher::NotificationKind::WatchStopped)
            ++state.observerStops;
    };
    const ResultFileSystemWatcher initiallyFull = owner.watch(limited, directory.view());
    SC_TEST_EXPECT(initiallyFull.isError(FileSystemWatcherError::WatchLimitExceeded));
    SC_TEST_EXPECT(initiallyFull.detail == FileSystemWatcherErrorDetail::BSDInventory);
    SC_TEST_EXPECT(limited.stopWatching().isError(FileSystemWatcherError::NotWatching));
    SC_TEST_EXPECT(fs.removeFile("__bsd_capacity/b"));
    SC_TEST_EXPECT(owner.watch(limited, directory.view()));
    SC_TEST_EXPECT(owner.watch(observer, observerDirectory.view()));
    submitQueuedWatcher(eventLoop);
    SC_TEST_EXPECT(fs.write("__bsd_capacity/b", "b"));
    SC_TEST_EXPECT(runUntil(eventLoop, [&state] { return state.limitedStops == 1 and state.observerStops == 1; }));
    SC_TEST_EXPECT(state.failure.isError(FileSystemWatcherError::WatchLimitExceeded));
    SC_TEST_EXPECT(state.failure.detail == FileSystemWatcherErrorDetail::BSDReconcile);
    FileSystemWatcher::FolderWatcher another;
    another.notifyCallback = [](const FileSystemWatcher::Notification&) {};
    SC_TEST_EXPECT(owner.watch(another, observerDirectory.view()).isError(FileSystemWatcherError::MonitoringFailed));
    SC_TEST_EXPECT(owner.close());
    ownerOpen = false;
    SC_TEST_EXPECT(owner.init(runner));
    ownerOpen = true;
    SC_TEST_EXPECT(owner.watch(another, directory.view()));
    SC_TEST_EXPECT(another.stopWatching());
    SC_TEST_EXPECT(owner.close());
    closeOwner.disarm();
    SC_TEST_EXPECT(eventLoop.close());
    closeLoop.disarm();
}

void SC::FileSystemWatcherAsyncTest::bsdDescriptorExhaustion(const StringView appDirectory)
{
    if (not test_section("BSD descriptor exhaustion rollback"))
        return;

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(appDirectory));
    SC_TEST_EXPECT(fs.makeDirectory("__bsd_descriptors"));
    SC_TEST_EXPECT(fs.write("__bsd_descriptors/file", "content"));
    auto removeDirectory = MakeDeferred([&] { (void)fs.removeDirectoryRecursive("__bsd_descriptors"); });

    StringPath directory;
    SC_TEST_EXPECT(Path::join(directory, {appDirectory, "__bsd_descriptors"}));
    FileSystemWatcher::ThreadRunner runner;

    FileSystemWatcher owner;
    SC_TEST_EXPECT(owner.init(runner));
    auto closeOwner = MakeDeferred([&] { (void)owner.close(); });

    FileSystemWatcher::FolderWatcher watcher;
    watcher.notifyCallback = [](const FileSystemWatcher::Notification&) {};

    struct rlimit original;
    const bool    gotLimit = ::getrlimit(RLIMIT_NOFILE, &original) == 0;
    SC_TEST_EXPECT(gotLimit);
    if (not gotLimit)
        return;
    struct rlimit limited = original;
    if (limited.rlim_cur > 64)
        limited.rlim_cur = 64;
    const bool lowered = ::setrlimit(RLIMIT_NOFILE, &limited) == 0;
    SC_TEST_EXPECT(lowered);
    if (not lowered)
        return;
    int    held[64];
    size_t count              = 0;
    auto   releaseDescriptors = MakeDeferred(
        [&]
        {
            while (count != 0)
                (void)::close(held[--count]);
            (void)::setrlimit(RLIMIT_NOFILE, &original);
        });
    int fd;
    while (count < sizeof(held) / sizeof(held[0]) and (fd = ::open("/dev/null", O_RDONLY)) != -1)
        held[count++] = fd;
    const int exhaustionError = errno;
    SC_TEST_EXPECT(count > 2 and exhaustionError == EMFILE);
    if (count <= 2 or exhaustionError != EMFILE)
        return;
    const int available = held[--count];
    SC_TEST_EXPECT(::close(available) == 0);
    const ResultFileSystemWatcher failed = owner.watch(watcher, directory.view());
    SC_TEST_EXPECT(failed.isError(FileSystemWatcherError::WatchSetupFailed));
    SC_TEST_EXPECT(failed.detail == FileSystemWatcherErrorDetail::BSDInventory);
    SC_TEST_EXPECT(failed.nativeError == EMFILE);
    SC_TEST_EXPECT(watcher.stopWatching().isError(FileSystemWatcherError::NotWatching));
    errno = 0;
    SC_TEST_EXPECT(::fcntl(available, F_GETFD) == -1 and errno == EBADF);
    SC_TEST_EXPECT(::close(held[--count]) == 0);
    SC_TEST_EXPECT(::close(held[--count]) == 0);
    SC_TEST_EXPECT(owner.watch(watcher, directory.view()));
    SC_TEST_EXPECT(watcher.stopWatching());
    while (count != 0)
        SC_TEST_EXPECT(::close(held[--count]) == 0);
    SC_TEST_EXPECT(::setrlimit(RLIMIT_NOFILE, &original) == 0);
    releaseDescriptors.disarm();
    SC_TEST_EXPECT(owner.close());
    closeOwner.disarm();
}

void SC::FileSystemWatcherAsyncTest::bsdPassiveEntries(const StringView appDirectory)
{
    if (not test_section("BSD passive entry status"))
        return;

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(appDirectory));
    SC_TEST_EXPECT(fs.makeDirectory("__bsd_passive"));
    auto removeDirectory = MakeDeferred([&] { (void)fs.removeDirectoryRecursive("__bsd_passive"); });

    StringPath directory;
    StringPath target;
    StringPath firstLink;
    StringPath secondLink;
    SC_TEST_EXPECT(Path::join(directory, {appDirectory, "__bsd_passive"}));
    SC_TEST_EXPECT(Path::join(target, {directory.view(), "missing"}));
    SC_TEST_EXPECT(Path::join(firstLink, {directory.view(), "first-link"}));
    SC_TEST_EXPECT(Path::join(secondLink, {directory.view(), "second-link"}));
    SC_TEST_EXPECT(fs.createSymbolicLink(target.view(), firstLink.view()));

    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());
    auto closeLoop = MakeDeferred([&] { (void)eventLoop.close(); });

    FileSystemWatcherAsyncT<AsyncEventLoop> runner;
    runner.init(eventLoop);
    FileSystemWatcher owner;
    SC_TEST_EXPECT(owner.init(runner));
    auto closeOwner = MakeDeferred([&] { (void)owner.close(); });
    struct State
    {
        int limited = 0;

        bool correct = true;
    } state;
    FileSystemWatcher::FolderWatcher watcher;
    watcher.notifyCallback = [&state](const FileSystemWatcher::Notification& event)
    {
        if (event.kind == FileSystemWatcher::NotificationKind::CoverageLimited)
        {
            ++state.limited;
            state.correct = state.correct and event.failure.isError(FileSystemWatcherError::CoverageLimited) and
                            event.failure.detail == FileSystemWatcherErrorDetail::BSDPassiveEntries and
                            event.operation == FileSystemWatcher::Operation::AddRemoveRename and
                            event.relativePath.isEmpty();
        }
    };
    SC_TEST_EXPECT(owner.watch(watcher, directory.view()));
    SC_TEST_EXPECT(runUntil(eventLoop, [&state] { return state.limited == 1; }));
    SC_TEST_EXPECT(state.correct);
    SC_TEST_EXPECT(fs.rename(firstLink.view(), secondLink.view()));
    SC_TEST_EXPECT(runUntil(eventLoop, [&state] { return state.limited == 2; }));
    SC_TEST_EXPECT(state.correct);
    SC_TEST_EXPECT(fs.createSymbolicLink(target.view(), firstLink.view()));
    SC_TEST_EXPECT(runUntil(eventLoop, [&state] { return state.limited == 3; }));
    SC_TEST_EXPECT(state.correct);
    SC_TEST_EXPECT(owner.close());
    closeOwner.disarm();
    SC_TEST_EXPECT(eventLoop.close());
    closeLoop.disarm();
}

void SC::FileSystemWatcherAsyncTest::bsdExtendedStorage(const StringView appDirectory)
{
    if (not test_section("BSD extended entry storage"))
        return;

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(appDirectory));
    SC_TEST_EXPECT(fs.makeDirectory("__bsd_large"));
    auto removeDirectory = MakeDeferred([&] { (void)fs.removeDirectoryRecursive("__bsd_large"); });

    StringPath directory;
    SC_TEST_EXPECT(Path::join(directory, {appDirectory, "__bsd_large"}));
    for (size_t index = 0; index < 130; ++index)
    {
        StringPath name;
        SC_TEST_EXPECT(StringBuilder::format(name, "__bsd_large/item-{}", index));
        SC_TEST_EXPECT(fs.write(name.view(), "content"));
    }

    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());
    auto closeLoop = MakeDeferred([&] { (void)eventLoop.close(); });

    FileSystemWatcherAsyncT<AsyncEventLoop> runner;
    runner.init(eventLoop);
    FileSystemWatcher owner;
    SC_TEST_EXPECT(owner.init(runner));
    auto closeOwner = MakeDeferred([&] { (void)owner.close(); });

    FileSystemWatcher::FolderWatcher defaultWatcher;
    defaultWatcher.notifyCallback              = [](const FileSystemWatcher::Notification&) {};
    const ResultFileSystemWatcher defaultLimit = owner.watch(defaultWatcher, directory.view());
    SC_TEST_EXPECT(defaultLimit.isError(FileSystemWatcherError::WatchLimitExceeded));
    SC_TEST_EXPECT(defaultWatcher.stopWatching().isError(FileSystemWatcherError::NotWatching));

    char overlapping[256];

    FileSystemWatcher::FolderWatcher invalid({overlapping, 32}, overlapping);
    invalid.notifyCallback                = [](const FileSystemWatcher::Notification&) {};
    const ResultFileSystemWatcher overlap = owner.watch(invalid, directory.view());
    SC_TEST_EXPECT(overlap.isError(FileSystemWatcherError::BufferTooSmall));
    SC_TEST_EXPECT(overlap.detail == FileSystemWatcherErrorDetail::BSDEntryStorage);
    SC_TEST_EXPECT(invalid.stopWatching().isError(FileSystemWatcherError::NotWatching));

    char entryStorage[FileSystemWatcher::FolderWatcher::entryStorageSize(160)];

    FileSystemWatcher::FolderWatcher expanded({}, entryStorage);

    int matching            = 0;
    expanded.notifyCallback = [&matching](const FileSystemWatcher::Notification& event)
    {
        if (event.kind == FileSystemWatcher::NotificationKind::Change and event.relativePath == "item-130")
            ++matching;
    };
    SC_TEST_EXPECT(owner.watch(expanded, directory.view()));
    submitQueuedWatcher(eventLoop);
    SC_TEST_EXPECT(fs.write("__bsd_large/item-130", "added"));
    SC_TEST_EXPECT(runUntil(eventLoop, [&matching] { return matching > 0; }));
    SC_TEST_EXPECT(expanded.stopWatching());
    SC_TEST_EXPECT(owner.close());
    closeOwner.disarm();
    SC_TEST_EXPECT(eventLoop.close());
    closeLoop.disarm();
}

void SC::FileSystemWatcherAsyncTest::bsdCallbackLifecycle(const StringView appDirectory)
{
    if (not test_section("BSD callback lifecycle"))
        return;

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(appDirectory));
    SC_TEST_EXPECT(fs.makeDirectory("__bsd_callbacks"));
    auto removeDirectory = MakeDeferred([&] { (void)fs.removeDirectoryRecursive("__bsd_callbacks"); });

    StringPath directory;
    SC_TEST_EXPECT(Path::join(directory, {appDirectory, "__bsd_callbacks"}));
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());
    auto closeLoop = MakeDeferred([&] { (void)eventLoop.close(); });

    FileSystemWatcherAsyncT<AsyncEventLoop> runner;
    runner.init(eventLoop);
    FileSystemWatcher owner;
    SC_TEST_EXPECT(owner.init(runner));
    auto closeOwner = MakeDeferred([&] { (void)owner.close(); });

    FileSystemWatcher::FolderWatcher first, second;
    struct State
    {
        FileSystemWatcher*                owner  = nullptr;
        FileSystemWatcher::FolderWatcher* first  = nullptr;
        FileSystemWatcher::FolderWatcher* second = nullptr;

        bool stopped = false;
        bool closed  = false;
        bool correct = true;
    } state{&owner, &first, &second};
    first.notifyCallback = [&state](const FileSystemWatcher::Notification& event)
    {
        if (event.kind != FileSystemWatcher::NotificationKind::Change)
            return;
        state.correct =
            static_cast<bool>(state.first->stopWatching()) and static_cast<bool>(state.second->stopWatching());
        state.stopped = true;
    };
    second.notifyCallback = [](const FileSystemWatcher::Notification&) {};
    SC_TEST_EXPECT(owner.watch(first, directory.view()));
    SC_TEST_EXPECT(owner.watch(second, directory.view()));
    submitQueuedWatcher(eventLoop);
    SC_TEST_EXPECT(fs.write("__bsd_callbacks/stop", "x"));
    SC_TEST_EXPECT(runUntil(eventLoop, [&state] { return state.stopped; }));
    SC_TEST_EXPECT(state.correct);
    SC_TEST_EXPECT(first.stopWatching().isError(FileSystemWatcherError::NotWatching));
    SC_TEST_EXPECT(second.stopWatching().isError(FileSystemWatcherError::NotWatching));

    first.notifyCallback = [&state](const FileSystemWatcher::Notification& event)
    {
        if (event.kind != FileSystemWatcher::NotificationKind::Change)
            return;
        state.correct = static_cast<bool>(state.owner->close());
        state.closed  = true;
    };
    SC_TEST_EXPECT(owner.watch(first, directory.view()));
    submitQueuedWatcher(eventLoop);
    SC_TEST_EXPECT(fs.write("__bsd_callbacks/close", "x"));
    SC_TEST_EXPECT(runUntil(eventLoop, [&state] { return state.closed; }));
    SC_TEST_EXPECT(state.correct);
    closeOwner.disarm();
    SC_TEST_EXPECT(eventLoop.close());
    closeLoop.disarm();
}

void SC::FileSystemWatcherAsyncTest::bsdDynamicTreeAndRootLoss(const StringView appDirectory)
{
    if (not test_section("BSD dynamic tree and root loss"))
        return;

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(appDirectory));
    SC_TEST_EXPECT(fs.makeDirectory("__bsd_dynamic"));
    auto removeDirectory = MakeDeferred(
        [&]
        {
            (void)fs.removeDirectoryRecursive("__bsd_dynamic");
            (void)fs.removeDirectoryRecursive("__bsd_moved");
        });
    StringPath directory;
    SC_TEST_EXPECT(Path::join(directory, {appDirectory, "__bsd_dynamic"}));
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());
    auto closeLoop = MakeDeferred([&] { (void)eventLoop.close(); });

    FileSystemWatcherAsyncT<AsyncEventLoop> runner;
    runner.init(eventLoop);
    FileSystemWatcher owner;
    SC_TEST_EXPECT(owner.init(runner));
    auto closeOwner = MakeDeferred([&] { (void)owner.close(); });
    struct State
    {
        StringView expected;
        int        structural = 0;
        int        matching   = 0;
        int        stopped    = 0;

        bool stopError = false;
    } state;
    FileSystemWatcher::FolderWatcher watcher;
    watcher.notifyCallback = [&state](const FileSystemWatcher::Notification& event)
    {
        if (event.kind == FileSystemWatcher::NotificationKind::WatchStopped)
        {
            ++state.stopped;
            state.stopError = event.failure.isError(FileSystemWatcherError::MonitoringFailed) and
                              event.failure.detail == FileSystemWatcherErrorDetail::BSDReconcile;
        }
        else if (event.kind == FileSystemWatcher::NotificationKind::Change)
        {
            if (event.operation == FileSystemWatcher::Operation::AddRemoveRename)
                ++state.structural;
            if (event.relativePath == state.expected)
                ++state.matching;
        }
    };
    SC_TEST_EXPECT(owner.watch(watcher, directory.view()));
    submitQueuedWatcher(eventLoop);
    SC_TEST_EXPECT(fs.makeDirectory("__bsd_dynamic/new"));
    SC_TEST_EXPECT(fs.makeDirectory("__bsd_dynamic/new/deep"));
    SC_TEST_EXPECT(fs.write("__bsd_dynamic/new/deep/existing", "a"));
    SC_TEST_EXPECT(runUntil(eventLoop, [&state] { return state.structural > 0; }));
    state.expected = "new/deep/later";
    state.matching = 0;
    SC_TEST_EXPECT(fs.write("__bsd_dynamic/new/deep/later", "b"));
    SC_TEST_EXPECT(runUntil(eventLoop, [&state] { return state.matching > 0; }));
    const int beforeMove = state.structural;
    SC_TEST_EXPECT(fs.rename("__bsd_dynamic/new", "__bsd_dynamic/moved"));
    SC_TEST_EXPECT(runUntil(eventLoop, [&state, beforeMove] { return state.structural > beforeMove; }));
    state.expected = "moved/deep/existing";
    state.matching = 0;
    SC_TEST_EXPECT(fs.write("__bsd_dynamic/moved/deep/existing", "changed"));
    SC_TEST_EXPECT(runUntil(eventLoop, [&state] { return state.matching > 0; }));
    SC_TEST_EXPECT(fs.rename("__bsd_dynamic", "__bsd_moved"));
    SC_TEST_EXPECT(runUntil(eventLoop, [&state] { return state.stopped > 0; }));
    SC_TEST_EXPECT(state.stopError);
    SC_TEST_EXPECT(owner.close());
    closeOwner.disarm();
    SC_TEST_EXPECT(eventLoop.close());
    closeLoop.disarm();
}

void SC::FileSystemWatcherAsyncTest::bsdAsyncFailureCallbacks(const StringView appDirectory)
{
    if (not test_section("BSD async failure callbacks"))
        return;

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(appDirectory));
    SC_TEST_EXPECT(fs.makeDirectory("__bsd_failure_callbacks"));
    auto removeDirectory = MakeDeferred([&] { (void)fs.removeDirectoryRecursive("__bsd_failure_callbacks"); });

    StringPath directory;
    SC_TEST_EXPECT(Path::join(directory, {appDirectory, "__bsd_failure_callbacks"}));
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());
    auto closeLoop = MakeDeferred([&] { (void)eventLoop.close(); });

    FileSystemWatcherAsyncT<AsyncEventLoop> runner;
    runner.init(eventLoop);
    FileSystemWatcher owner;
    SC_TEST_EXPECT(owner.init(runner));
    auto closeOwner = MakeDeferred([&] { (void)owner.close(); });

    FileSystemWatcher::FolderWatcher first, removed, last;
    struct State
    {
        FileSystemWatcher*                owner   = nullptr;
        FileSystemWatcher::FolderWatcher* first   = nullptr;
        FileSystemWatcher::FolderWatcher* removed = nullptr;

        int firstCalls   = 0;
        int removedCalls = 0;
        int lastCalls    = 0;

        bool correct = true;
        bool closed  = false;
    } state{&owner, &first, &removed};
    first.notifyCallback = [&state](const FileSystemWatcher::Notification& event)
    {
        ++state.firstCalls;
        state.correct = state.correct and event.kind == FileSystemWatcher::NotificationKind::WatchStopped and
                        event.failure.toResult().category() == ResultCategory(99) and
                        event.failure.toResult().errorValue() == 17 and
                        static_cast<bool>(state.first->stopWatching()) and
                        static_cast<bool>(state.removed->stopWatching());
    };
    removed.notifyCallback = [&state](const FileSystemWatcher::Notification&) { ++state.removedCalls; };
    last.notifyCallback    = [&state](const FileSystemWatcher::Notification& event)
    {
        ++state.lastCalls;
        state.correct = state.correct and event.kind == FileSystemWatcher::NotificationKind::WatchStopped and
                        event.failure.toResult().category() == ResultCategory(99) and
                        event.failure.toResult().errorValue() == 17;
        state.closed = static_cast<bool>(state.owner->close());
    };
    SC_TEST_EXPECT(owner.watch(first, directory.view()));
    SC_TEST_EXPECT(owner.watch(removed, directory.view()));
    SC_TEST_EXPECT(owner.watch(last, directory.view()));
    submitQueuedWatcher(eventLoop);
    owner.asyncNotifyFailure(Result::Error(ResultCategory(99), 17));
    SC_TEST_EXPECT(state.firstCalls == 1);
    SC_TEST_EXPECT(state.removedCalls == 0);
    SC_TEST_EXPECT(state.lastCalls == 1);
    SC_TEST_EXPECT(state.correct);
    SC_TEST_EXPECT(state.closed);
    closeOwner.disarm();
    SC_TEST_EXPECT(eventLoop.close());
    closeLoop.disarm();
}

void SC::FileSystemWatcherAsyncTest::bsdNativeReadFailure(const StringView appDirectory)
{
    if (not test_section("BSD native read failure"))
        return;

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(appDirectory));
    SC_TEST_EXPECT(fs.makeDirectory("__bsd_native_failure"));
    auto removeDirectory = MakeDeferred([&] { (void)fs.removeDirectoryRecursive("__bsd_native_failure"); });

    StringPath directory;
    SC_TEST_EXPECT(Path::join(directory, {appDirectory, "__bsd_native_failure"}));
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());
    auto closeLoop = MakeDeferred([&] { (void)eventLoop.close(); });
    struct Runner : FileSystemWatcherAsyncT<AsyncEventLoop>
    {
        int nativeQueue() const { return notifyFd; }
    } runner;
    runner.init(eventLoop);
    FileSystemWatcher owner;
    SC_TEST_EXPECT(owner.init(runner));
    auto closeOwner = MakeDeferred([&] { (void)owner.close(); });
    struct State
    {
        int                     stopped = 0;
        ResultFileSystemWatcher failure;
    } state;
    FileSystemWatcher::FolderWatcher watcher;
    watcher.notifyCallback = [&state](const FileSystemWatcher::Notification& event)
    {
        if (event.kind == FileSystemWatcher::NotificationKind::WatchStopped)
        {
            ++state.stopped;
            state.failure = event.failure;
        }
    };
    SC_TEST_EXPECT(owner.watch(watcher, directory.view()));
    submitQueuedWatcher(eventLoop);
    const int queue = runner.nativeQueue();
    const int saved = ::dup(queue);
    SC_TEST_EXPECT(saved >= 0);
    if (saved < 0)
        return;
    auto closeSaved = MakeDeferred([saved] { (void)::close(saved); });
    SC_TEST_EXPECT(::close(queue) == 0);
    const bool rearmed  = owner.asyncNotify(nullptr);
    const int  restored = ::dup2(saved, queue);
    SC_TEST_EXPECT(restored == queue);
    SC_TEST_EXPECT(not rearmed);
    SC_TEST_EXPECT(state.stopped == 1);
    SC_TEST_EXPECT(state.failure.isError(FileSystemWatcherError::MonitoringFailed));
    SC_TEST_EXPECT(state.failure.detail == FileSystemWatcherErrorDetail::BSDReadEvents);
    SC_TEST_EXPECT(state.failure.nativeError == EBADF);
    SC_TEST_EXPECT(not owner.asyncNotify(nullptr));
    SC_TEST_EXPECT(owner.close());
    closeOwner.disarm();
    SC_TEST_EXPECT(eventLoop.close());
    closeLoop.disarm();
}
