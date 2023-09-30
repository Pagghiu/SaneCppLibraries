// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Async/EventLoop.h" // AsyncLoopWakeUp
#include "../Foundation/Containers/IntrusiveDoubleLinkedList.h"
#include "../Foundation/Language/Function.h"
#include "../Foundation/Language/Opaque.h"
#include "../Foundation/Language/Result.h"
#include "../Foundation/Strings/StringView.h"
#include "../Threading/Threading.h" // EventObject

// Header
namespace SC
{
struct FileSystemWatcher;
struct String;
} // namespace SC

struct SC::FileSystemWatcher
{
    struct Internal;
    enum class Operation
    {
        Modified,
        AddRemoveRename,
    };
    struct Notification
    {
        StringView basePath;
        StringView relativePath;
        Operation  operation = Operation::Modified;

        SC::ReturnCode getFullPath(String& bufferString, StringView& outStringView) const;

      private:
        friend struct Internal;
#if SC_PLATFORM_APPLE
        StringView fullPath;
#endif
    };

    struct ThreadRunnerInternal;
    struct ThreadRunnerSizes
    {
        static constexpr int MaxWatchablePaths = 1024;
        static constexpr int Windows =
            (2 * MaxWatchablePaths) * sizeof(void*) + sizeof(uint64_t) + sizeof(Thread) + sizeof(Action);
        static constexpr int Apple   = sizeof(void*);
        static constexpr int Default = sizeof(void*);
    };
    using ThreadRunnerTraits = OpaqueTraits<ThreadRunnerInternal, ThreadRunnerSizes>;
    using ThreadRunner       = OpaqueUniqueObject<OpaqueFuncs<ThreadRunnerTraits>>;

    struct FolderWatcherInternal;
    struct FolderWatcherSizes
    {
        static constexpr int MaxChangesBufferSize = 1024;
#if SC_PLATFORM_WINDOWS
        static constexpr int Windows =
            MaxChangesBufferSize + sizeof(void*) + sizeof(FileDescriptor) + sizeof(AsyncWindowsPoll);
#else
        static constexpr int Windows = 0;
#endif
        static constexpr int Apple   = sizeof(void*);
        static constexpr int Default = sizeof(void*);
    };
    using FolderWatcherTraits = OpaqueTraits<FolderWatcherInternal, FolderWatcherSizes>;
    using FolderWatcherOpaque = OpaqueUniqueObject<OpaqueFuncs<FolderWatcherTraits>>;

    struct FolderWatcher
    {
        FileSystemWatcher*  parent = nullptr;
        FolderWatcher*      next   = nullptr;
        FolderWatcher*      prev   = nullptr;
        String*             path   = nullptr;
        FolderWatcherOpaque internal;

        Function<void(const Notification&)> notifyCallback;

        ReturnCode unwatch();
    };
    IntrusiveDoubleLinkedList<FolderWatcher> watchers;

    struct EventLoopRunner
    {
        EventLoop& eventLoop;

#if SC_PLATFORM_APPLE
        AsyncLoopWakeUp eventLoopAsync = {};
        EventObject     eventObject    = {};
#endif
    };

    [[nodiscard]] ReturnCode init(ThreadRunner& runner);
    [[nodiscard]] ReturnCode init(EventLoopRunner& runner);
    [[nodiscard]] ReturnCode close();
    [[nodiscard]] ReturnCode watch(FolderWatcher& watcher, String& path,
                                   Function<void(const Notification&)>&& notifyCallback);

    struct InternalSizes
    {
        static constexpr int Windows = 3 * sizeof(void*);
        static constexpr int Apple   = 43 * sizeof(void*) + sizeof(Mutex);
        static constexpr int Default = sizeof(void*);
    };

    using InternalTraits = OpaqueTraits<Internal, InternalSizes>;
    using InternalOpaque = OpaqueUniqueObject<OpaqueFuncs<InternalTraits>>;
    InternalOpaque internal;
};
