// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/CompilerMacrosExport.h"
#ifndef SC_EXPORT_LIBRARY_FILE_SYSTEM_WATCHER
#define SC_EXPORT_LIBRARY_FILE_SYSTEM_WATCHER 0
#endif
#define SC_FILE_SYSTEM_WATCHER_EXPORT SC_COMPILER_LIBRARY_EXPORT(SC_EXPORT_LIBRARY_FILE_SYSTEM_WATCHER)

#include "../Common/Assert.h"
#include "../Common/CompilerOffsetOf.h"
#include "../Common/Function.h"
#include "../Common/IGrowableBufferStringPath.h"
#include "../Common/OpaqueObject.h"
#include "../Common/PlatformMacrosType.h"
#include "../Common/Result.h"
#include "FileSystemWatcherError.h"

namespace SC
{
SC_DECLARE_ASSERT_PROVIDER(FileSystemWatcherAssert, SC_FILE_SYSTEM_WATCHER_EXPORT);

#define SC_FILE_SYSTEM_WATCHER_ASSERT_RELEASE(e)        SC_ASSERT_PROVIDER_RELEASE(SC::FileSystemWatcherAssert, e)
#define SC_FILE_SYSTEM_WATCHER_ASSERT_DEBUG(e)          SC_ASSERT_PROVIDER_DEBUG(SC::FileSystemWatcherAssert, e)
#define SC_FILE_SYSTEM_WATCHER_TRUST_RESULT(expression) SC_FILE_SYSTEM_WATCHER_ASSERT_RELEASE(expression)

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
/// Example using SC::FileSystemWatcherAsyncT (that implements SC::FileSystemWatcher::EventLoopRunner):
/// \snippet Tests/Libraries/FileSystemWatcher/FileSystemWatcherAsyncTest.cpp fileSystemWatcherAsyncSnippet

//! [OpaqueDeclarationSnippet]
struct FileSystemWatcher
{
  private:
    struct Internal;

    struct InternalDefinition
    {
        static constexpr int Windows = 3 * sizeof(void*);
        // The Apple backend keeps a Notification and structured result inline during event-loop handoff.
        static constexpr int Apple   = 61 * sizeof(void*);
        static constexpr int Linux   = 256;
        static constexpr int Default = 512; // BSD keeps a mutex and callback-lifetime state inline.

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
        static constexpr int Default = 256; // BSD thread and shutdown state.

        static constexpr size_t Alignment = alignof(void*);

        using Object = ThreadRunnerInternal;
    };

    struct FolderWatcherInternal;
    struct FolderWatcherSizes
    {
        static constexpr int MaxNumberOfSubdirs   = 128; // Max number of subfolders tracked in a watcher
        static constexpr int MaxChangesBufferSize = 16 * 1024;

        static constexpr int Windows = MaxChangesBufferSize + sizeof(void*) + sizeof(void*);
        static constexpr int Apple   = sizeof(StringPath) + sizeof(void*);
        static constexpr int Linux   = 24 * MaxNumberOfSubdirs + 4096 + 40;
        static constexpr int Default = 13 * 1024; // BSD bounded vnode inventory and path storage.

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

    /// @brief Describes whether a notification represents a change or a loss of monitoring information.
    enum class NotificationKind
    {
        Change,          ///< A file or directory may have changed
        RescanRequired,  ///< Changes were lost; rescan the watched tree and restore coverage if necessary
        WatchStopped,    ///< Monitoring has stopped for this root; inspect Notification::failure before restarting
        CoverageLimited, ///< Some entries cannot be monitored directly; inspect Notification::failure
    };

    /// @brief Notification holding type and path
    struct Notification
    {
        StringSpan basePath;     ///< Reference to the watched directory
        StringSpan relativePath; ///< Relative path of the file being notified from `basePath`

        Operation        operation = Operation::Modified;      ///< Coarse change class
        NotificationKind kind      = NotificationKind::Change; ///< Change or monitoring status

        ResultFileSystemWatcher failure; ///< Failure for WatchStopped or CoverageLimited; successful otherwise

        /// @brief Get the full path of the file being watched.
        /// @param path StringPath that will hold full (absolute) path
        /// @return Invalid result if it's not possible building the full path
        ResultFileSystemWatcher getFullPath(StringPath& path) const;

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
        /// When an empty span is passed, the internal 4 KiB buffer is used.
        /// This buffer is used for recursive paths on Linux and BSD; it is unused on Windows and macOS.
        FolderWatcher(Span<char> subFolderRelativePathsBuffer = {});

        /// @brief Supplies optional storage for more watched entries on BSD.
        /// @param subFolderRelativePathsBuffer Optional storage for relative path components.
        /// @param entryStorage Optional byte storage for native entries and reconciliation marks. It must not overlap
        /// the path buffer or be used for anything else until this watcher is stopped. Other platforms ignore it.
        /// @note On BSD, use entryStorageSize(numberOfEntries) to reserve enough bytes for the requested entries,
        /// including the root; a zero-length span selects the built-in storage.
        FolderWatcher(Span<char> subFolderRelativePathsBuffer, Span<char> entryStorage);

        /// @brief Byte storage sufficient for at least `numberOfEntries` BSD entries, or zero if the size overflows.
        static constexpr size_t entryStorageSize(size_t numberOfEntries)
        {
            constexpr size_t bytesPerEntry = SC_PLATFORM_FREEBSD ? 24 : 65;
            constexpr size_t padding       = alignof(uint64_t) - 1;
            return numberOfEntries == 0 or numberOfEntries > (static_cast<size_t>(-1) - padding) / bytesPerEntry
                       ? 0
                       : numberOfEntries * bytesPerEntry + padding;
        }

        Function<void(const Notification&)> notifyCallback; ///< Function that will be called on a notification

        /// @brief Stop watching this directory. After calling it the FolderWatcher can be reused or released.
        /// @return Valid result if directory was unwatched successfully.
        ResultFileSystemWatcher stopWatching();

        /// @brief Sets debug name for AsyncExternalCompletion used on Windows (used only for debug purposes)
        void setDebugName(const char* debugName);

      private:
        friend struct FileSystemWatcher;
        template <typename T_AsyncEventLoop>
        friend struct FileSystemWatcherAsyncT;
#if SC_PLATFORM_WINDOWS
#if SC_ASYNC_ENABLE_LOG
        AlignedStorage<160> asyncStorage;
#else
        AlignedStorage<152> asyncStorage;
#endif
#endif
        OpaqueObject<FolderWatcherSizes> internal;

        FileSystemWatcher* parent = nullptr;
        FolderWatcher*     next   = nullptr;
        FolderWatcher*     prev   = nullptr;

        StringPath path;

#if SC_PLATFORM_LINUX || SC_PLATFORM_FREEBSD
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
        virtual Result appleStopWakeUp()        = 0;
        virtual void   appleSignalEventObject() = 0;
        virtual void   appleResetHandoff()      = 0;
        virtual Result appleWakeUpAndWait()     = 0;

#elif SC_PLATFORM_LINUX || SC_PLATFORM_NETBSD || SC_PLATFORM_FREEBSD || SC_PLATFORM_OPENBSD
        // On failure, no readiness request may remain active or retain notifyFd.
#if SC_PLATFORM_LINUX
        virtual Result linuxStartSharedFileReadiness() = 0;
        virtual Result linuxStopSharedFileReadiness()  = 0;
#else
        virtual Result bsdStartSharedFileReadiness() = 0;
        virtual Result bsdStopSharedFileReadiness()  = 0;
#endif

        int notifyFd = -1;

#else
        virtual Result windowsStartFolderExternalCompletion(FolderWatcher& watcher, void* handle) = 0;
        virtual Result windowsRequestStopFolderExternalCompletion(FolderWatcher& watcher)         = 0;
        virtual Result windowsWaitFolderExternalCompletionStopped(FolderWatcher& watcher)         = 0;
        virtual Result windowsMarkFolderExternalCompletionPending(FolderWatcher& watcher)         = 0;
        virtual Result windowsClearFolderExternalCompletionPending(FolderWatcher& watcher)        = 0;
        virtual void*  windowsGetOverlapped(FolderWatcher& watcher)                               = 0;
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
    ResultFileSystemWatcher init(ThreadRunner& runner);

    /// @brief Setup watcher to receive async notifications on an event loop
    /// @param runner Address of a EventLoopRunner object that must be valid until close()
    /// @return Valid Result if the watcher has been initialized correctly
    ResultFileSystemWatcher init(EventLoopRunner& runner);

    /// @brief Stops all watchers and frees the ThreadRunner or EventLoopRunner passed in init
    /// @note Closing from the ThreadRunner callback fails; close from another thread after
    /// the callback.
    /// @return Valid Result if resources have been freed successfully
    ResultFileSystemWatcher close();

    /// @brief Starts watching a single directory, calling FolderWatcher::notifyCallback on file events.
    /// @param watcher Reference to a (not already used) watcher, with a valid FolderWatcher::notifyCallback.
    /// Its address must not change until FolderWatcher::stopWatching or FileSystemWatcher::close
    /// @param path The directory being monitored
    /// @return Valid Result if directory is accessible and the watcher is initialized properly.
    /// @note A missing root or a root that is not a directory is rejected without consuming the FolderWatcher.
    ResultFileSystemWatcher watch(FolderWatcher& watcher, StringSpan path);

    bool asyncNotify(FolderWatcher* watcher, size_t bytesTransferred = 0);
#if SC_PLATFORM_LINUX || SC_PLATFORM_NETBSD || SC_PLATFORM_FREEBSD || SC_PLATFORM_OPENBSD
    void asyncNotifyFailure(Result failure);
#elif SC_PLATFORM_WINDOWS
    void asyncNotifyFailure(FolderWatcher& watcher, Result failure);
#endif

  private:
    friend decltype(internal);
    friend decltype(FolderWatcher::internal);
    template <typename T_AsyncEventLoop>
    friend struct FileSystemWatcherAsyncT;
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

/// @brief FileSystemWatcherAsyncT is an implementation of SC::FileSystemWatcher that uses SC::Async.
///
/// This class exists as a template is to break the dependency of SC::FileSystemWatcher from SC::AsyncEventLoop.
///
/// Example:
/// \snippet Tests/Libraries/FileSystemWatcher/FileSystemWatcherAsyncTest.cpp fileSystemWatcherAsyncSnippet
template <typename T_AsyncEventLoop>
struct FileSystemWatcherAsyncT : public FileSystemWatcher::EventLoopRunner
{
    using Self = FileSystemWatcherAsyncT;

    using T_AsyncLoopWakeUp         = typename T_AsyncEventLoop::LoopWakeUp;
    using T_AsyncFileReadiness      = typename T_AsyncEventLoop::FileReadiness;
    using T_AsyncExternalCompletion = typename T_AsyncEventLoop::ExternalCompletion;
    using T_EventObject             = typename T_AsyncEventLoop::EventObjectType;
    using T_AsyncResult             = typename T_AsyncEventLoop::ResultType;

    void init(T_AsyncEventLoop& loop) { eventLoop = &loop; }

  protected:
    T_AsyncEventLoop* eventLoop = nullptr;

#if SC_PLATFORM_APPLE
    virtual Result appleStartWakeUp() override
    {
        if (eventLoop == nullptr or fileSystemWatcher == nullptr)
            return Result::Error(FileSystemWatcherResultCategory, FileSystemWatcherError::NotInitialized);
        T_AsyncLoopWakeUp& wakeUp = asyncWakeUp;
        wakeUp.callback.template bind<Self, &Self::onEventLoopNotification>(*this);
        return wakeUp.start(*eventLoop);
    }

    virtual void appleSignalEventObject() override { eventObject.signal(); }

    virtual void appleResetHandoff() override
    {
        notificationGeneration++;
        // The producer is quiescent. Consume any unused release signal.
        eventObject.signal();
        eventObject.wait();
    }

    virtual Result appleStopWakeUp() override
    {
        if (not asyncWakeUp.isFree())
        {
            SC_TRY(asyncWakeUp.stop(*eventLoop));
            while (not asyncWakeUp.isFree())
            {
                SC_TRY(eventLoop->runNoWait());
            }
        }
        appleResetHandoff();
        return Result(true);
    }

    virtual Result appleWakeUpAndWait() override
    {
        const Result res = asyncWakeUp.wakeUp(*eventLoop);
        eventObject.wait();
        return res;
    }

    void onEventLoopNotification(typename T_AsyncLoopWakeUp::Result& result)
    {
        const size_t generation = notificationGeneration;
        result.reactivateRequest(true);
        const bool delivered = fileSystemWatcher->asyncNotify(nullptr);
        if (delivered and generation == notificationGeneration)
        {
            eventObject.signal();
        }
    }

    size_t notificationGeneration = 0;

    T_AsyncLoopWakeUp asyncWakeUp = {};
    T_EventObject     eventObject = {};
#elif SC_PLATFORM_LINUX || SC_PLATFORM_NETBSD || SC_PLATFORM_FREEBSD || SC_PLATFORM_OPENBSD
#if SC_PLATFORM_LINUX
    virtual Result linuxStartSharedFileReadiness() override { return startSharedFileReadiness(); }
    virtual Result linuxStopSharedFileReadiness() override { return stopSharedFileReadiness(); }
#else
    virtual Result bsdStartSharedFileReadiness() override { return startSharedFileReadiness(); }
    virtual Result bsdStopSharedFileReadiness() override { return stopSharedFileReadiness(); }
#endif

    Result startSharedFileReadiness()
    {
        if (eventLoop == nullptr or fileSystemWatcher == nullptr)
            return Result::Error(FileSystemWatcherResultCategory, FileSystemWatcherError::NotInitialized);
        SC_TRY(eventLoop->associateExternallyCreatedFileDescriptorHandle(notifyFd));
        asyncPoll.callback.template bind<Self, &Self::onEventLoopNotification>(*this);
        return asyncPoll.start(*eventLoop, notifyFd);
    }

    Result stopSharedFileReadiness()
    {
        if (sharedDeliveryAlive != nullptr)
        {
            *sharedDeliveryAlive = false;
            sharedDeliveryAlive  = nullptr;
        }
        if (not asyncPoll.isFree())
        {
            SC_TRY(asyncPoll.stop(*eventLoop));
            while (not asyncPoll.isFree())
            {
                SC_TRY(eventLoop->runNoWait());
            }
        }
        return Result(true);
    }

    void onEventLoopNotification(typename T_AsyncFileReadiness::Result& result)
    {
        bool deliveryAlive  = true;
        sharedDeliveryAlive = &deliveryAlive;

        bool rearm = false;
        if (result.isValid())
        {
            rearm = fileSystemWatcher->asyncNotify(nullptr);
        }
        else
        {
            fileSystemWatcher->asyncNotifyFailure(result.isValid());
        }
        if (deliveryAlive)
        {
            sharedDeliveryAlive = nullptr;
            if (rearm)
                result.reactivateRequest(true);
        }
    }

    bool* sharedDeliveryAlive = nullptr;

    T_AsyncFileReadiness asyncPoll = {};
#else
    using FolderWatcher = FileSystemWatcher::FolderWatcher;
    virtual Result windowsStartFolderExternalCompletion(FolderWatcher& watcher, void* handle) override
    {
        if (eventLoop == nullptr or fileSystemWatcher == nullptr)
            return Result::Error(FileSystemWatcherResultCategory, FileSystemWatcherError::NotInitialized);
        T_AsyncExternalCompletion& completion =
            watcher.asyncStorage.template reinterpret_as<T_AsyncExternalCompletion>();
        placementNew(completion);
        completion.setDebugName("FileSystemWatcherAsync Completion");
        completion.callback.template bind<Self, &Self::onEventLoopNotification>(*this);
        const Result started = completion.start(*eventLoop, handle);
        if (not started)
            completion.~T_AsyncExternalCompletion();
        return started;
    }

    virtual Result windowsRequestStopFolderExternalCompletion(FolderWatcher& watcher) override
    {
        T_AsyncExternalCompletion& completion =
            watcher.asyncStorage.template reinterpret_as<T_AsyncExternalCompletion>();

        if (completion.isFree() and not completion.hasSubmissionPending())
            return Result(true);
        return completion.stop(*eventLoop);
    }

    virtual Result windowsWaitFolderExternalCompletionStopped(FolderWatcher& watcher) override
    {
        T_AsyncExternalCompletion& completion =
            watcher.asyncStorage.template reinterpret_as<T_AsyncExternalCompletion>();
        while (not completion.isFree())
        {
            SC_TRY(eventLoop->runOnce());
        }
        completion.~T_AsyncExternalCompletion();
        return Result(true);
    }

    virtual Result windowsMarkFolderExternalCompletionPending(FolderWatcher& watcher) override
    {
        T_AsyncExternalCompletion& completion =
            watcher.asyncStorage.template reinterpret_as<T_AsyncExternalCompletion>();
        return completion.markSubmissionPending();
    }

    virtual Result windowsClearFolderExternalCompletionPending(FolderWatcher& watcher) override
    {
        T_AsyncExternalCompletion& completion =
            watcher.asyncStorage.template reinterpret_as<T_AsyncExternalCompletion>();
        return completion.clearSubmissionPending();
    }

    virtual void* windowsGetOverlapped(FolderWatcher& watcher) override
    {
        T_AsyncExternalCompletion& completion =
            watcher.asyncStorage.template reinterpret_as<T_AsyncExternalCompletion>();
        return completion.getWindowsOverlapped();
    }

    void onEventLoopNotification(typename T_AsyncExternalCompletion::Result& result)
    {
        SC_COMPILER_WARNING_PUSH_OFFSETOF;
        auto&          storage = reinterpret_cast<decltype(FolderWatcher::asyncStorage)&>(result.getAsync());
        FolderWatcher& watcher = SC_COMPILER_FIELD_OFFSET(FolderWatcher, asyncStorage, storage);
        if (result.isValid())
        {
            if (fileSystemWatcher->asyncNotify(&watcher, result.completionData.bytesTransferred))
                result.reactivateRequest(true);
        }
        else
            fileSystemWatcher->asyncNotifyFailure(watcher, result.isValid());
        SC_COMPILER_WARNING_POP_OFFSETOF;
    }

#endif
};

//! @}
} // namespace SC
