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
        static constexpr int Windows = 184;
        static constexpr int Apple   = 104;
        static constexpr int Default = 336;

        static constexpr size_t Alignment = alignof(void*);

        using Object = KernelQueue;
    };

    using KernelQueueOpaque = OpaqueObject<KernelQueueDefinition>;

    // Using opaque to allow defining KernelQueue class later
    KernelQueueOpaque kernelQueue;

    AsyncEventLoop* loop = nullptr;

    Atomic<bool> wakeUpPending = false;

    int numberOfActiveHandles     = 0;
    int numberOfManualCompletions = 0;
    int numberOfExternals         = 0;

    // Submitting phase
    IntrusiveDoubleLinkedList<AsyncRequest> submissions;

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

    Time::HighResolutionCounter loopTime;

    // AsyncRequest flags
    static constexpr int16_t Flag_ManualCompletion = 1 << 0;

    [[nodiscard]] Result close();

    [[nodiscard]] int getTotalNumberOfActiveHandle() const;

    void removeActiveHandle(AsyncRequest& async);
    void addActiveHandle(AsyncRequest& async);
    void scheduleManualCompletion(AsyncRequest& async);
    void increaseActiveCount();
    void decreaseActiveCount();

    // Timers
    [[nodiscard]] const Time::HighResolutionCounter* findEarliestTimer() const;

    void invokeExpiredTimers();
    void updateTime();
    void executeTimers(KernelEvents& kernelEvents, const Time::HighResolutionCounter& nextTimer);

    [[nodiscard]] Result cancelAsync(AsyncRequest& async);

    // LoopWakeUp
    void executeWakeUps(AsyncResult& result);

    // Setup
    [[nodiscard]] Result queueSubmission(AsyncRequest& async, AsyncTask* task);

    // Phases
    [[nodiscard]] Result stageSubmission(KernelEvents& kernelEvents, AsyncRequest& async);
    [[nodiscard]] Result setupAsync(KernelEvents& kernelEvents, AsyncRequest& async);
    [[nodiscard]] Result teardownAsync(KernelEvents& kernelEvents, AsyncRequest& async);
    [[nodiscard]] Result activateAsync(KernelEvents& kernelEvents, AsyncRequest& async);
    [[nodiscard]] Result cancelAsync(KernelEvents& kernelEvents, AsyncRequest& async);
    [[nodiscard]] Result completeAsync(KernelEvents& kernelEvents, AsyncRequest& async, Result&& returnCode,
                                       bool& reactivate);

    struct SetupAsyncPhase;
    struct TeardownAsyncPhase;
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

    [[nodiscard]] Result runStep(SyncMode syncMode);

    void runStepExecuteCompletions(KernelEvents& kernelEvents);
    void runStepExecuteManualCompletions(KernelEvents& kernelEvents);
    void runStepExecuteManualThreadPoolCompletions(KernelEvents& kernelEvents);

    friend struct AsyncRequest;

    template <typename T>
    void freeAsyncRequests(IntrusiveDoubleLinkedList<T>& linkedList);

    template <typename T>
    [[nodiscard]] Result waitForThreadPoolTasks(IntrusiveDoubleLinkedList<T>& linkedList);

    template <typename Lambda>
    [[nodiscard]] static Result applyOnAsync(AsyncRequest& async, Lambda&& lambda);
};
