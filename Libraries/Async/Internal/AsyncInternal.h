#pragma once
#include "../Async.h"

#include "../../Containers/IntrusiveDoubleLinkedList.h"
#include "ThreadSafeLinkedList.h"

struct SC::AsyncEventLoop::Internal
{
#if SC_PLATFORM_LINUX
    struct KernelQueuePosix;
    struct KernelEventsPosix;
    struct KernelQueueIoURing;
    struct KernelEventsIoURing;
    struct KernelQueue;
    struct KernelEvents;
#elif SC_PLATFORM_APPLE
    struct KernelQueuePosix;
    struct KernelEventsPosix;
    using KernelQueue  = KernelQueuePosix;
    using KernelEvents = KernelEventsPosix;
#elif SC_PLATFORM_WINDOWS
    struct KernelQueue;
    struct KernelEvents;
#else
    struct KernelQueue;
    struct KernelEvents;
#endif

    struct KernelQueueDefinition
    {
        static constexpr int Windows = 136;
        static constexpr int Apple   = 104;
        static constexpr int Linux   = 328;
        static constexpr int Default = Linux;

        static constexpr size_t Alignment = alignof(void*);

        using Object = KernelQueue;
    };

    using KernelQueueOpaque = OpaqueObject<KernelQueueDefinition>;

    // Using opaque to allow defining KernelQueue class later
    KernelQueueOpaque kernelQueue;

    Atomic<bool> wakeUpPending = false;
    Options      createOptions;

    bool runTimers   = false;
    bool initialized = false;
    bool interrupted = false;

    int numberOfSubmissions       = 0;
    int numberOfActiveHandles     = 0;
    int numberOfManualCompletions = 0;
    int numberOfExternals         = 0;

    // Submitting phase
    IntrusiveDoubleLinkedList<AsyncRequest> submissions;

    // Cancellation phase
    IntrusiveDoubleLinkedList<AsyncRequest> cancellations;

    // Active phase
    IntrusiveDoubleLinkedList<AsyncLoopTimeout>   activeLoopTimeouts;
    IntrusiveDoubleLinkedList<AsyncLoopWakeUp>    activeLoopWakeUps;
    IntrusiveDoubleLinkedList<AsyncLoopWork>      activeLoopWork;
    IntrusiveDoubleLinkedList<AsyncProcessExit>   activeProcessExits;
    IntrusiveDoubleLinkedList<AsyncSocketAccept>  activeSocketAccepts;
    IntrusiveDoubleLinkedList<AsyncSocketConnect> activeSocketConnects;
    IntrusiveDoubleLinkedList<AsyncSocketSend>    activeSocketSends;
    IntrusiveDoubleLinkedList<AsyncSocketReceive> activeSocketReceives;
    IntrusiveDoubleLinkedList<AsyncSocketClose>   activeSocketCloses;
    IntrusiveDoubleLinkedList<AsyncFileRead>      activeFileReads;
    IntrusiveDoubleLinkedList<AsyncFileWrite>     activeFileWrites;
    IntrusiveDoubleLinkedList<AsyncFileClose>     activeFileCloses;
    IntrusiveDoubleLinkedList<AsyncFilePoll>      activeFilePolls;

    // Manual completions
    IntrusiveDoubleLinkedList<AsyncRequest> manualCompletions;

    ThreadSafeLinkedList<AsyncRequest> manualThreadPoolCompletions;

    Time::Monotonic loopTime;

    AsyncEventLoopListeners* listeners = nullptr;

    // AsyncRequest flags
    static constexpr int16_t Flag_ManualCompletion       = 1 << 0;
    static constexpr int16_t Flag_ExcludeFromActiveCount = 1 << 1;
    static constexpr int16_t Flag_Internal               = 1 << 2;
    static constexpr int16_t Flag_WatcherSet             = 1 << 3;

    [[nodiscard]] Result close(AsyncEventLoop& loop);

    [[nodiscard]] int getTotalNumberOfActiveHandle() const;

    void removeActiveHandle(AsyncRequest& async);
    void addActiveHandle(AsyncRequest& async);
    void scheduleManualCompletion(AsyncRequest& async);

    // Timers
    [[nodiscard]] AsyncLoopTimeout* findEarliestLoopTimeout() const;

    void invokeExpiredTimers(Time::Absolute currentTime);
    void updateTime();

    [[nodiscard]] Result stop(AsyncEventLoop& loop, AsyncRequest& async, Function<void(AsyncResult&)>* onClose);

    // LoopWakeUp
    void executeWakeUps();

    // Setup
    void queueSubmission(AsyncEventLoop& loop, AsyncRequest& async);

    // Phases
    [[nodiscard]] Result stageSubmission(KernelEvents& kernelEvents, AsyncRequest& async);
    [[nodiscard]] Result setupAsync(KernelEvents& kernelEvents, AsyncRequest& async);
    [[nodiscard]] Result activateAsync(KernelEvents& kernelEvents, AsyncRequest& async);
    [[nodiscard]] Result cancelAsync(KernelEvents& kernelEvents, AsyncRequest& async);
    [[nodiscard]] Result completeAsync(KernelEvents& kernelEvents, AsyncRequest& async, Result&& returnCode,
                                       bool& reactivate);

    struct SetupAsyncPhase;
    struct ReactivateAsyncPhase;
    struct ActivateAsyncPhase;
    struct CancelAsyncPhase;
    struct CompleteAsyncPhase;
    [[nodiscard]] Result completeAndEventuallyReactivate(KernelEvents& kernelEvents, AsyncRequest& async,
                                                         Result&& returnCode);

    void reportError(KernelEvents& kernelEvents, AsyncRequest& async, Result&& returnCode);

    enum class SyncMode
    {
        NoWait,
        ForcedForwardProgress
    };

    [[nodiscard]] Result runStep(AsyncEventLoop& loop, SyncMode syncMode);

    [[nodiscard]] Result submitRequests(AsyncEventLoop& loop, AsyncKernelEvents& kernelEvents);
    [[nodiscard]] Result blockingPoll(AsyncEventLoop& loop, SyncMode syncMode, AsyncKernelEvents& kernelEvents);
    [[nodiscard]] Result dispatchCompletions(AsyncEventLoop& loop, SyncMode syncMode, AsyncKernelEvents& kernelEvents);

    void executeCancellationCallbacks();
    void runStepExecuteCompletions(KernelEvents& kernelEvents);
    void runStepExecuteManualCompletions(KernelEvents& kernelEvents);
    void runStepExecuteManualThreadPoolCompletions(KernelEvents& kernelEvents);

    friend struct AsyncRequest;

    template <typename T>
    void stopRequests(IntrusiveDoubleLinkedList<T>& linkedList);

    template <typename T>
    void enumerateRequests(IntrusiveDoubleLinkedList<T>& linkedList, Function<void(AsyncRequest&)>& callback);

    template <typename T>
    [[nodiscard]] Result waitForThreadPoolTasks(IntrusiveDoubleLinkedList<T>& linkedList);

    template <typename Lambda>
    [[nodiscard]] static Result applyOnAsync(AsyncRequest& async, Lambda&& lambda);

    struct AsyncTeardown
    {
        AsyncRequest::Type        type          = AsyncRequest::Type::LoopTimeout;
        int16_t                   flags         = 0;
        AsyncEventLoop*           eventLoop     = nullptr;
        FileDescriptor::Handle    fileHandle    = FileDescriptor::Invalid;
        SocketDescriptor::Handle  socketHandle  = SocketDescriptor::Invalid;
        ProcessDescriptor::Handle processHandle = ProcessDescriptor::Invalid;
#if SC_CONFIGURATION_DEBUG
        char debugName[128] = "None";
#endif
    };

    void                 prepareTeardown(AsyncRequest& async, AsyncTeardown& teardown);
    [[nodiscard]] Result teardownAsync(AsyncTeardown& async);

    template <typename T>
    static size_t getSummedSizeOfBuffers(T& async)
    {
        size_t summedSizeBytes;
        if (async.singleBuffer)
        {
            summedSizeBytes = async.buffer.sizeInBytes();
        }
        else
        {
            summedSizeBytes = 0;
            for (size_t idx = 0; idx < async.buffers.sizeInElements(); ++idx)
            {
                summedSizeBytes += async.buffers[idx].sizeInBytes();
            }
        }
        return summedSizeBytes;
    }
};
