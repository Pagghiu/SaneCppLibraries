// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Foundation/Function.h"
#include "../Foundation/OpaqueObject.h"
#include "../Foundation/Result.h"
#include "../Foundation/StringPath.h"

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
/// @warning Modifications to files that do not affect directory entries may not trigger notifications. @n
/// This includes modifications made through symbolic or hard links located outside the watched directory,
/// pointing to a file of the watched directory. @n
/// Modifications made through memory-mapped file operations (mmap) can also exhibit the same behaviour. @n
/// The underlying OS APIs monitor directory entries rather than all possible file access methods.
///
/// There are two modes in which FileSystemWatcher can be initialized, defining how notifications are delivered:
///
/// | Mode                                  | Description                                       |
/// |:--------------------------------------|:--------------------------------------------------|
/// | SC::FileSystemWatcher::ThreadRunner   | @copybrief SC::FileSystemWatcher::ThreadRunner    |
/// | SC::FileSystemWatcher::EventLoopRunner| @copybrief SC::FileSystemWatcher::EventLoopRunner |
///
/// Example using SC::FileSystemWatcher::ThreadRunner:
/// \snippet Tests/Libraries/FileSystemWatcher/FileSystemWatcherTest.cpp fileSystemWatcherThreadRunnerSnippet
///
/// Example using SC::FileSystemWatcherAsync (that implements SC::FileSystemWatcher::EventLoopRunner):
/// \snippet Tests/Libraries/FileSystemWatcherAsync/FileSystemWatcherAsyncTest.cpp fileSystemWatcherAsyncSnippet

//! [OpaqueDeclarationSnippet]
struct FileSystemWatcher
{
  private:
    struct Internal;

    struct InternalDefinition
    {
        static constexpr int Windows = 3 * sizeof(void*);
        static constexpr int Apple   = 42 * sizeof(void*);
        static constexpr int Linux   = sizeof(void*) * 4;
        static constexpr int Default = Linux;

        static constexpr size_t Alignment = alignof(void*);

        using Object = Internal;
    };

  public:
    // Must be public to avoid GCC complaining
    using InternalOpaque = OpaqueObject<InternalDefinition>;

  private:
    InternalOpaque internal;

    //...
    //! [OpaqueDeclarationSnippet]
    struct ThreadRunnerInternal;
    struct ThreadRunnerDefinition
    {
        static constexpr int MaxWatchablePaths = 1024;

        static constexpr int Windows = (2 * MaxWatchablePaths + 2) * sizeof(void*) + sizeof(uint64_t);
        static constexpr int Apple   = sizeof(void*);
        static constexpr int Linux   = sizeof(void*) * 6;
        static constexpr int Default = Linux;

        static constexpr size_t Alignment = alignof(void*);

        using Object = ThreadRunnerInternal;
    };

    struct FolderWatcherInternal;
    struct FolderWatcherSizes
    {
        static constexpr int MaxNumberOfSubdirs   = 128; // Max number of subfolders tracked in a watcher
        static constexpr int MaxChangesBufferSize = 1024;

        static constexpr int Windows = MaxChangesBufferSize + sizeof(void*) + sizeof(void*);
        static constexpr int Apple   = sizeof(void*);
        static constexpr int Linux   = 1056 + 1024 + 8;
        static constexpr int Default = Linux;

        static constexpr size_t Alignment = alignof(void*);

        using Object = FolderWatcherInternal;
    };

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
        StringSpan basePath;                        ///< Reference to the watched directory
        StringSpan relativePath;                    ///< Relative path of the file being notified from `basePath`
        Operation  operation = Operation::Modified; ///< Notification type

        /// @brief Get the full path of the file being watched.
        /// @param path StringPath that will hold full (absolute) path
        /// @return Invalid result if it's not possible building the full path
        SC::Result getFullPath(StringPath& path) const;

      private:
        friend struct Internal;
#if SC_PLATFORM_APPLE
        StringSpan fullPath;
#endif
    };

    /// @brief Represents a single folder being watched.
    /// While in use, the address of this object must not change, as it's inserted in a linked list.
    /// @note You can create an SC::ArenaMap to create a buffer of these objects, that can be easily reused.
    struct FolderWatcher
    {
        /// @brief Constructs a folder watcher
        /// @param subFolderRelativePathsBuffer User provided buffer used to sub-folders relative paths.
        /// When an empty span is passed, the internal 1Kb buffer is used.
        /// This buffer is used on Linux only when watching folders recursively, it's unused on Windows / macOS.
        FolderWatcher(Span<char> subFolderRelativePathsBuffer = {});

        Function<void(const Notification&)> notifyCallback; ///< Function that will be called on a notification

        /// @brief Stop watching this directory. After calling it the FolderWatcher can be reused or released.
        /// @return Valid result if directory was unwatched successfully.
        Result stopWatching();

        /// @brief Sets debug name for AsyncFilePoll used on Windows (used only for debug purposes)
        void setDebugName(const char* debugName);

      private:
        friend struct FileSystemWatcher;
        friend struct FileSystemWatcherAsync;
#if SC_PLATFORM_WINDOWS
#if SC_ASYNC_ENABLE_LOG
        AlignedStorage<120> asyncStorage;
#else
        AlignedStorage<112> asyncStorage;
#endif
#endif
        OpaqueObject<FolderWatcherSizes> internal;

        FileSystemWatcher* parent = nullptr;
        FolderWatcher*     next   = nullptr;
        FolderWatcher*     prev   = nullptr;

        StringPath path;

#if SC_PLATFORM_LINUX
        Span<char> subFolderRelativePathsBuffer;
#endif
    };

    /// @brief Abstract class to use event loop notifications (see SC::FileSystemWatcherAsync).
    struct EventLoopRunner
    {
        virtual ~EventLoopRunner() {}

      protected:
#if SC_PLATFORM_APPLE
        virtual Result appleStartWakeUp()       = 0;
        virtual void   appleSignalEventObject() = 0;
        virtual Result appleWakeUpAndWait()     = 0;

#elif SC_PLATFORM_LINUX
        virtual Result linuxStartSharedFilePoll() = 0;
        virtual Result linuxStopSharedFilePoll()  = 0;

        int notifyFd = -1;

#else
        virtual Result windowsStartFolderFilePoll(FolderWatcher& watcher, void* handle) = 0;
        virtual Result windowsStopFolderFilePoll(FolderWatcher& watcher)                = 0;
        virtual void*  windowsGetOverlapped(FolderWatcher& watcher)                     = 0;
#endif
        friend struct Internal;
        FileSystemWatcher* fileSystemWatcher = nullptr;

        void internalInit(FileSystemWatcher& fsWatcher, int handle);
    };

    /// @brief Delivers notifications on a background thread.
    using ThreadRunner = OpaqueObject<ThreadRunnerDefinition>;

    /// @brief Setup watcher to receive notifications from a background thread
    /// @param runner Address of a ThreadRunner object that must be valid until close()
    /// @return Valid Result if the watcher has been initialized correctly
    Result init(ThreadRunner& runner);

    /// @brief Setup watcher to receive async notifications on an event loop
    /// @param runner Address of a EventLoopRunner object that must be valid until close()
    /// @return Valid Result if the watcher has been initialized correctly
    Result init(EventLoopRunner& runner);

    /// @brief Stops all watchers and frees the ThreadRunner or EventLoopRunner passed in init
    /// @return Valid Result if resources have been freed successfully
    Result close();

    /// @brief Starts watching a single directory, calling FolderWatcher::notifyCallback on file events.
    /// @param watcher Reference to a (not already used) watcher, with a valid FolderWatcher::notifyCallback.
    /// Its address must not change until FolderWatcher::stopWatching or FileSystemWatcher::close
    /// @param path The directory being monitored
    /// @return Valid Result if directory is accessible and the watcher is initialized properly.
    Result watch(FolderWatcher& watcher, StringSpan path);

    void asyncNotify(FolderWatcher* watcher);

  private:
    friend decltype(internal);
    friend decltype(FolderWatcher::internal);
    friend struct EventLoopRunner;
    // Trimmed duplicate of IntrusiveDoubleLinkedList<T>
    struct WatcherLinkedList
    {
        FolderWatcher* back  = nullptr; // has no next
        FolderWatcher* front = nullptr; // has no prev

        void queueBack(FolderWatcher& watcher);
        void remove(FolderWatcher& watcher);
    };
    WatcherLinkedList watchers;
};

//! @}
} // namespace SC
