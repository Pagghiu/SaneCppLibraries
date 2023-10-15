// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.

struct SC::FileSystemWatcher::Internal
{
    [[nodiscard]] Result init(FileSystemWatcher& self, EventLoopRunner& eventLoopSupport) { return Result(false); }
    [[nodiscard]] Result init(FileSystemWatcher& self, ThreadRunner& threadingSupport) { return Result(false); }
    [[nodiscard]] Result close() { return Result(false); }
    [[nodiscard]] Result startWatching(FolderWatcher*) { return Result(false); }
    [[nodiscard]] Result stopWatching(FolderWatcher&) { return Result(false); }
};

struct SC::FileSystemWatcher::FolderWatcherInternal
{
};
struct SC::FileSystemWatcher::ThreadRunnerInternal
{
};

SC::Result SC::FileSystemWatcher::Notification::getFullPath(String&, StringView&) const { return Result(false); }
