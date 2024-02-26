#pragma once
#include "../Async.h"

#include "../../Containers/IntrusiveDoubleLinkedList.h"

struct SC::AsyncEventLoop::Private
{
    AsyncEventLoop* eventLoop = nullptr;

    int numberOfActiveHandles     = 0;
    int numberOfManualCompletions = 0;
    int numberOfExternals         = 0;

    // Submitting phase
    IntrusiveDoubleLinkedList<AsyncRequest> submissions;

    // Active phase
    IntrusiveDoubleLinkedList<AsyncLoopTimeout>   activeLoopTimeouts;
    IntrusiveDoubleLinkedList<AsyncLoopWakeUp>    activeLoopWakeUps;
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

    Time::HighResolutionCounter loopTime;

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
    void executeTimers(KernelQueue& queue, const Time::HighResolutionCounter& nextTimer);

    [[nodiscard]] Result cancelAsync(AsyncRequest& async);

    // LoopWakeUp
    void executeWakeUps(AsyncResult& result);

    // Setup
    [[nodiscard]] Result queueSubmission(AsyncRequest& async);

    // Phases
    [[nodiscard]] Result stageSubmission(KernelQueue& queue, AsyncRequest& async);
    [[nodiscard]] Result setupAsync(KernelQueue& queue, AsyncRequest& async);
    [[nodiscard]] Result teardownAsync(KernelQueue& queue, AsyncRequest& async);
    [[nodiscard]] Result activateAsync(KernelQueue& queue, AsyncRequest& async);
    [[nodiscard]] Result cancelAsync(KernelQueue& queue, AsyncRequest& async);
    [[nodiscard]] Result completeAsync(KernelQueue& queue, AsyncRequest& async, Result&& returnCode, bool& reactivate);

    struct SetupAsyncPhase;
    struct TeardownAsyncPhase;
    struct ActivateAsyncPhase;
    struct CancelAsyncPhase;
    struct CompleteAsyncPhase;
    [[nodiscard]] Result completeAndEventuallyReactivate(KernelQueue& queue, AsyncRequest& async, Result&& returnCode);

    void reportError(KernelQueue& queue, AsyncRequest& async, Result&& returnCode);

    enum class SyncMode
    {
        NoWait,
        ForcedForwardProgress
    };

    [[nodiscard]] Result runStep(SyncMode syncMode);

    void runStepExecuteCompletions(KernelQueue& queue);
    void runStepExecuteManualCompletions(KernelQueue& queue);

    friend struct AsyncRequest;

    template <typename T>
    void freeAsyncRequests(IntrusiveDoubleLinkedList<T>& linkedList);

    template <typename Lambda>
    [[nodiscard]] static Result applyOnAsync(AsyncRequest& async, Lambda&& lambda);
};
