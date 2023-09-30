// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.

struct SC::FileSystemWatcher::Internal
{
    [[nodiscard]] ReturnCode init(FileSystemWatcher& self, EventLoopRunner& eventLoopSupport)
    {
        return ReturnCode(false);
    }
    [[nodiscard]] ReturnCode init(FileSystemWatcher& self, ThreadRunner& threadingSupport) { return ReturnCode(false); }
    [[nodiscard]] ReturnCode close() { return ReturnCode(false); }
    [[nodiscard]] ReturnCode startWatching(FolderWatcher*) { return ReturnCode(false); }
    [[nodiscard]] ReturnCode stopWatching(FolderWatcher&) { return ReturnCode(false); }
};

struct SC::FileSystemWatcher::FolderWatcherInternal
{
};
struct SC::FileSystemWatcher::ThreadRunnerInternal
{
};

SC::ReturnCode SC::FileSystemWatcher::Notification::getFullPath(String&, StringView&) const
{
    return ReturnCode(false);
}
