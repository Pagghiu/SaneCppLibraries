// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Async/Async.h" // AsyncLoopWakeUp
#include "../Foundation/Function.h"
#include "../Foundation/OpaqueObject.h"
#include "../Foundation/Result.h"
#include "../Threading/Threading.h" // EventObject

namespace SC
{

//! @defgroup group_file_system_watcher FileSystem Watcher
//! @copybrief library_file_system_watcher (see @ref library_file_system_watcher for more details)

//! @addtogroup group_file_system_watcher
//! @{

/// @brief Notifies about events (add, remove, rename, modified) on files and directories.
/// Caller can specify a callback for receiving notifications the SC::FileSystemWatcher::watch method.
///
/// Changes are grouped in two categories:
/// - Added, removed and renamed files and directories
/// - Modified files
///
/// There are two modes in which FileSystemWatcher can be initialized, defining how notifications are delivered:
///
/// | Mode                                  | Description                                       |
/// |:--------------------------------------|:--------------------------------------------------|
/// | SC::FileSystemWatcher::ThreadRunner   | @copybrief SC::FileSystemWatcher::ThreadRunner    |
/// | SC::FileSystemWatcher::EventLoopRunner| @copybrief SC::FileSystemWatcher::EventLoopRunner |
///
/// Example using SC::FileSystemWatcher::EventLoopRunner:
/// \snippet Tests/Libraries/FileSystemWatcher/FileSystemWatcherTest.cpp fileSystemWatcherEventLoopRunnerSnippet
///
/// Example using SC::FileSystemWatcher::ThreadRunner:
/// \snippet Tests/Libraries/FileSystemWatcher/FileSystemWatcherTest.cpp fileSystemWatcherThreadRunnerSnippet
struct FileSystemWatcher
{
  private:
    struct Internal;
    struct ThreadRunnerInternal;
    struct ThreadRunnerDefinition
    {
        static constexpr int MaxWatchablePaths = 1024;
        static constexpr int Windows =
            (2 * MaxWatchablePaths) * sizeof(void*) + sizeof(uint64_t) + sizeof(Thread) + sizeof(Action);
        static constexpr int Apple   = sizeof(void*);
        static constexpr int Linux   = sizeof(Thread) + sizeof(void*) * 2;
        static constexpr int Default = Linux;

        static constexpr size_t Alignment = alignof(void*);

        using Object = ThreadRunnerInternal;
    };

    struct FolderWatcherInternal;
    struct FolderWatcherSizes
    {
        static constexpr int MaxNumberOfSubdirs   = 128; // Max number of subfolders tracked in a watcher
        static constexpr int MaxChangesBufferSize = 1024;
        static constexpr int Windows =
            MaxChangesBufferSize + sizeof(void*) + sizeof(FileDescriptor) + sizeof(AsyncFilePoll);
        static constexpr int Apple   = sizeof(void*);
        static constexpr int Linux   = 1056;
        static constexpr int Default = Linux;

        static constexpr size_t Alignment = alignof(void*);

        using Object = FolderWatcherInternal;
    };
    struct InternalDefinition
    {
        static constexpr int Windows = 3 * sizeof(void*);
        static constexpr int Apple   = 43 * sizeof(void*) + sizeof(Mutex);
        static constexpr int Linux   = sizeof(void*) * 4;
        static constexpr int Default = Linux;

        static constexpr size_t Alignment = alignof(void*);

        using Object = Internal;
    };

    using InternalOpaque = OpaqueObject<InternalDefinition>;

    InternalOpaque internal;

  public:
    /// @brief Specifies the event classes. Some events are grouped in a single one because
    /// it's non-trivial providing precise notifications that are consistent across platforms.
    enum class Operation
    {
        Modified,        ///< A file or directory has been modified in its contents and/or timestamp
        AddRemoveRename, ///< A file or directory has been added, removed or renamed
    };

    /// @brief Notification holding type and path
    struct Notification
    {
        StringViewData basePath;                        ///< Reference to the watched directory
        StringViewData relativePath;                    ///< Relative path of the file being notified from `basePath`
        Operation      operation = Operation::Modified; ///< Notification type

        /// @brief Get the full path of the file being watched.
        /// @param[in] bufferSpan Space holding full path
        /// @param[out] outFullPath The full path
        /// @return Invalid result if it's not possible building the full path
        SC::Result getFullPath(Span<native_char_t> bufferSpan, StringViewData& outFullPath) const;

      private:
        friend struct Internal;
#if SC_PLATFORM_APPLE
        StringViewData fullPath;
#endif
    };

    /// @brief Represents a single folder being watched.
    /// While in use, the address of this object must not change, as it's inserted in a linked list.
    /// @note You can create an SC::ArenaMap to create a buffer of these objects, that can be easily reused.
    struct FolderWatcher
    {
        Function<void(const Notification&)> notifyCallback; ///< Function that will be called on a notification

        /// @brief Stop watching this directory. After calling it the FolderWatcher can be reused or released.
        /// @return Valid result if directory was unwatched successfully.
        Result stopWatching();

        /// @brief Sets debug name for AsyncFilePoll used on Windows (used only for debug purposes)
        void setDebugName(const char* debugName);

      private:
        friend struct FileSystemWatcher;
        friend struct IntrusiveDoubleLinkedList<FolderWatcher>;
        FileSystemWatcher* parent = nullptr;
        FolderWatcher*     next   = nullptr;
        FolderWatcher*     prev   = nullptr;

        native_char_t  pathBuffer[StringPath::MaxPath];
        StringViewData path;

        OpaqueObject<FolderWatcherSizes> internal;
    };

    /// @brief Delivers notifications using @ref library_async (SC::AsyncEventLoop).
    struct EventLoopRunner
    {

      private:
        friend struct FileSystemWatcher;
        AsyncEventLoop* eventLoop = nullptr;
#if SC_PLATFORM_APPLE
        AsyncLoopWakeUp asyncWakeUp = {};
        EventObject     eventObject = {};
#elif SC_PLATFORM_LINUX
        AsyncFilePoll asyncPoll = {};
#endif
    };

    /// @brief Delivers notifications on a background thread.
    using ThreadRunner = OpaqueObject<ThreadRunnerDefinition>;

    /// @brief Setup watcher to receive notifications from a background thread
    /// @param runner Address of a ThreadRunner object that must be valid until close()
    /// @return Valid Result if the watcher has been initialized correctly
    Result init(ThreadRunner& runner);

    /// @brief Setup watcher to receive async notifications on SC::AsyncEventLoop
    /// @param runner Address of a ThreadRunner object that must be valid until close()
    /// @param eventLoop A valid AsyncEventLoop
    /// @return Valid Result if the watcher has been initialized correctly
    Result init(EventLoopRunner& runner, AsyncEventLoop& eventLoop);

    /// @brief Stops all watchers and frees the ThreadRunner or EventLoopRunner passed in init
    /// @return Valid Result if resources have been freed successfully
    Result close();

    /// @brief Starts watching a single directory, calling FolderWatcher::notifyCallback on file events.
    /// @param watcher Reference to a (not already used) watcher, with a valid FolderWatcher::notifyCallback.
    /// Its address must not change until FolderWatcher::stopWatching or FileSystemWatcher::close
    /// @param path The directory being monitored
    /// @return Valid Result if directory is accessible and the watcher is initialized properly.
    Result watch(FolderWatcher& watcher, StringViewData path);

  private:
    friend decltype(internal);
    friend decltype(FolderWatcher::internal);
    IntrusiveDoubleLinkedList<FolderWatcher> watchers;
};

//! @}
} // namespace SC
