// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

struct SC::FileSystemWatcher::Internal
{
    [[nodiscard]] Result init(FileSystemWatcher&, EventLoopRunner&) { return Result(false); }
    [[nodiscard]] Result init(FileSystemWatcher&, ThreadRunner&) { return Result(false); }
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
