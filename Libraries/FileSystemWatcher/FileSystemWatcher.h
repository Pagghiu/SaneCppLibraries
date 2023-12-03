// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Async/Async.h" // Async::LoopWakeUp
#include "../Containers/IntrusiveDoubleLinkedList.h"
#include "../Foundation/Function.h"
#include "../Foundation/OpaqueObject.h"
#include "../Foundation/Result.h"
#include "../Strings/StringView.h"
#include "../Threading/Threading.h" // EventObject

namespace SC
{
struct FileSystemWatcher;
struct String;
} // namespace SC

//! @defgroup group_file_system_watcher FileSystem Watcher
//! @copybrief library_file_system_watcher (see @ref library_file_system_watcher for more details)

//! @addtogroup group_file_system_watcher
//! @{

/// @brief Notifies about events (add, remove, rename, modified) on files and directories
struct SC::FileSystemWatcher
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
        static constexpr int Default = sizeof(void*);

        static constexpr size_t Alignment = alignof(void*);

        using Object = ThreadRunnerInternal;
    };

    struct FolderWatcherInternal;
    struct FolderWatcherSizes
    {
        static constexpr int MaxChangesBufferSize = 1024;
#if SC_PLATFORM_WINDOWS
        static constexpr int Windows =
            MaxChangesBufferSize + sizeof(void*) + sizeof(FileDescriptor) + sizeof(Async::WindowsPoll);
#else
        static constexpr int Windows = 0;
#endif
        static constexpr int Apple   = sizeof(void*);
        static constexpr int Default = sizeof(void*);

        static constexpr size_t Alignment = alignof(void*);

        using Object = FolderWatcherInternal;
    };
    struct InternalDefinition
    {
        static constexpr int Windows = 3 * sizeof(void*);
        static constexpr int Apple   = 43 * sizeof(void*) + sizeof(Mutex);
        static constexpr int Default = sizeof(void*);

        static constexpr size_t Alignment = alignof(void*);

        using Object = Internal;
    };

    using InternalOpaque = OpaqueObject<InternalDefinition>;

    InternalOpaque internal;

  public:
    /// @brief Specifies the event classes. Some events are grouped in a signle one because
    /// it's non-trivial providing precise notifications that are consistent across platforms.
    enum class Operation
    {
        Modified,        ///< A file or directory has been modified in its contents and/or timestamp
        AddRemoveRename, ///< A file or directory has been added, removed or renamed
    };

    /// @brief Notification holding type and path
    struct Notification
    {
        StringView basePath;                        ///< Reference to the watched directory
        StringView relativePath;                    ///< Relative path of the file being notified from `basePath`
        Operation  operation = Operation::Modified; ///< Notification type

        /// @brief Get the full path of the file being watched.
        /// @param[in] bufferString On some platform a String to hold the joined full path is needed.
        /// @param[out] outFullPath The full path
        /// @return Invalid result if it's not possible building the full path
        SC::Result getFullPath(String& bufferString, StringView& outFullPath) const;

      private:
        friend struct Internal;
#if SC_PLATFORM_APPLE
        StringView fullPath;
#endif
    };

    /// @brief Represents a single folder being watched.
    /// While in use, the address of this object must not change, as it's inserted in a linked list.
    /// @note You can create an SC::ArenaMap to create a buffer of these objects, that can be easily reused.
    struct FolderWatcher
    {
        Function<void(const Notification&)> notifyCallback; ///< Function that will be called on a notification

        /// @brief Stop watching this Direectory. After calling it the FolderWatcher can be reused or released.
        /// @return Valid result if directory was unwached successfuly.
        Result unwatch();

      private:
        friend struct FileSystemWatcher;
        friend struct IntrusiveDoubleLinkedList<FolderWatcher>;
        FileSystemWatcher* parent = nullptr;
        FolderWatcher*     next   = nullptr;
        FolderWatcher*     prev   = nullptr;
        String*            path   = nullptr;

        OpaqueObject<FolderWatcherSizes> internal;
    };

    /// @brief Support object to allow user holding memory for needed resources for async mode
    struct EventLoopRunner
    {
        Async::EventLoop& eventLoop;
        EventLoopRunner(Async::EventLoop& eventLoop) : eventLoop(eventLoop) {}

      private:
        friend struct FileSystemWatcher;
#if SC_PLATFORM_APPLE
        Async::LoopWakeUp eventLoopAsync = {};
        EventObject       eventObject    = {};
#endif
    };

    /// @brief Support object to allow user holding memory for needed resources for threaded mode
    using ThreadRunner = OpaqueObject<ThreadRunnerDefinition>;

    /// @brief Setup watcher to receive notifications from a background thread
    /// @param runner Address of a ThreadRunner object that must be valid until close()
    /// @return Valid Result if the watcher has been initialized correctly
    [[nodiscard]] Result init(ThreadRunner& runner);

    /// @brief Setup watcher to receive async notifications on SC::EventLoop
    /// @param runner Address of a ThreadRunner object that must be valid until close()
    /// @return Valid Result if the watcher has been initialized correctly
    [[nodiscard]] Result init(EventLoopRunner& runner);

    /// @brief Stops all watchers and frees the ThreadRunner or EventLoopRunner passed in init
    /// @return Valid Result if resources have been freed successfully
    [[nodiscard]] Result close();

    /// @brief Watch a single directory
    /// @param watcher Reference to a (not already used) watcher. Its address must not change until
    /// FolderWatcher::unwatch or FileSystemWatcher::close
    /// @param path The directory being monitored
    /// @param notifyCallback A callback that will be invoked by the given runner
    /// @return Valid Result if directory is accessible and the watcher is initialized properly.
    [[nodiscard]] Result watch(FolderWatcher& watcher, String& path,
                               Function<void(const Notification&)>&& notifyCallback);

  private:
    friend decltype(internal);
    friend decltype(FolderWatcher::internal);
    IntrusiveDoubleLinkedList<FolderWatcher> watchers;
};

//! @}
