// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Async/Async.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/ThreadPool.h"

namespace SC
{
struct AsyncContractTest;
}

struct SC::AsyncContractTest : public SC::TestCase
{
    AsyncEventLoop::Options options;

    AsyncContractTest(SC::TestReport& report) : TestCase(report, "AsyncContractTest")
    {
        int numRuns = 1;
        if (AsyncEventLoop::tryProbingIOUring())
        {
            options.apiType = AsyncEventLoop::Options::ApiType::ForceUseEpoll;
            numRuns         = 2;
        }

        for (int idx = 0; idx < numRuns; ++idx)
        {
            if (test_section("stop suppresses normal callback"))
            {
                stopSuppressesNormalCallback();
            }
            if (test_section("close callback runs after request is free"))
            {
                closeCallbackRunsAfterRequestIsFree();
            }
            if (test_section("stop free request fails"))
            {
                stopFreeRequestFails();
            }
            if (test_section("stop submitted request suppresses normal callback"))
            {
                stopSubmittedRequestSuppressesNormalCallback();
            }
            if (test_section("latest close callback wins while cancelling"))
            {
                latestCloseCallbackWinsWhileCancelling();
            }
            if (test_section("reactivation keeps request owned"))
            {
                reactivationKeepsRequestOwned();
            }
            if (test_section("reactivated request can be stopped from callback"))
            {
                reactivatedRequestCanBeStoppedFromCallback();
            }
            if (test_section("last reactivation decision wins"))
            {
                lastReactivationDecisionWins();
            }
            if (test_section("non-reactivated request cannot be stopped from callback"))
            {
                nonReactivatedRequestCannotBeStoppedFromCallback();
            }
            if (test_section("reactivated request uses replaced callback"))
            {
                reactivatedRequestUsesReplacedCallback();
            }
            if (test_section("normal callback is copied before invocation"))
            {
                normalCallbackIsCopiedBeforeInvocation();
            }
            if (test_section("non-reactivated request can be reused inside callback"))
            {
                nonReactivatedRequestCanBeReusedInsideCallback();
            }
            if (test_section("sequence clears queued requests on cancel"))
            {
                sequenceClearsQueuedRequestsOnCancel();
            }
            if (test_section("sequence clears queued requests on error"))
            {
                sequenceClearsQueuedRequestsOnError();
            }
            if (test_section("sequence resumes queued requests on cancel when configured"))
            {
                sequenceResumesQueuedRequestsOnCancelWhenConfigured();
            }
            if (test_section("sequence resumes queued requests on error when configured"))
            {
                sequenceResumesQueuedRequestsOnErrorWhenConfigured();
            }
            if (test_section("thread pool mode can force supplied pool"))
            {
                threadPoolModeCanForceSuppliedPool();
            }
            if (test_section("validation failure leaves request free"))
            {
                validationFailureLeavesRequestFree();
            }
            if (test_section("loop close frees submitted requests"))
            {
                loopCloseFreesSubmittedRequests();
            }
            if (test_section("loop close frees active requests"))
            {
                loopCloseFreesActiveRequests();
            }
            if (test_section("loop close drains pending close callback"))
            {
                loopCloseDrainsPendingCloseCallback();
            }
            if (test_section("loop close suppresses posted manual completion"))
            {
                loopCloseSuppressesPostedManualCompletion();
            }
            if (test_section("loop can be recreated after close"))
            {
                loopCanBeRecreatedAfterClose();
            }
            if (test_section("listeners wrap blocking poll only"))
            {
                listenersWrapBlockingPollOnly();
            }
            if (test_section("wakeUp coalescing and one-shot behavior"))
            {
                wakeUpCoalescingAndOneShotBehavior();
            }
            if (test_section("active count exclusion preserves callbacks"))
            {
                activeCountExclusionPreservesCallbacks();
            }
            if (test_section("excluded active request does not keep run alive"))
            {
                excludedActiveRequestDoesNotKeepRunAlive();
            }
            if (test_section("run exits on interrupt with active work"))
            {
                runExitsOnInterruptWithActiveWork();
            }
            if (test_section("enumerateRequests reports submitted and active user requests"))
            {
                enumerateRequestsReportsSubmittedAndActiveUserRequests();
            }
            if (test_section("enumerateRequests reports active sequenced requests"))
            {
                enumerateRequestsReportsActiveSequencedRequests();
            }

            if (numRuns == 2)
            {
                options.apiType = AsyncEventLoop::Options::ApiType::ForceUseIoUring;
            }
        }
    }

    void stopSuppressesNormalCallback();
    void closeCallbackRunsAfterRequestIsFree();
    void stopFreeRequestFails();
    void stopSubmittedRequestSuppressesNormalCallback();
    void latestCloseCallbackWinsWhileCancelling();
    void reactivationKeepsRequestOwned();
    void reactivatedRequestCanBeStoppedFromCallback();
    void lastReactivationDecisionWins();
    void nonReactivatedRequestCannotBeStoppedFromCallback();
    void reactivatedRequestUsesReplacedCallback();
    void normalCallbackIsCopiedBeforeInvocation();
    void nonReactivatedRequestCanBeReusedInsideCallback();
    void sequenceClearsQueuedRequestsOnCancel();
    void sequenceClearsQueuedRequestsOnError();
    void sequenceResumesQueuedRequestsOnCancelWhenConfigured();
    void sequenceResumesQueuedRequestsOnErrorWhenConfigured();
    void threadPoolModeCanForceSuppliedPool();
    void validationFailureLeavesRequestFree();
    void loopCloseFreesSubmittedRequests();
    void loopCloseFreesActiveRequests();
    void loopCloseDrainsPendingCloseCallback();
    void loopCloseSuppressesPostedManualCompletion();
    void loopCanBeRecreatedAfterClose();
    void listenersWrapBlockingPollOnly();
    void wakeUpCoalescingAndOneShotBehavior();
    void activeCountExclusionPreservesCallbacks();
    void excludedActiveRequestDoesNotKeepRunAlive();
    void runExitsOnInterruptWithActiveWork();
    void enumerateRequestsReportsSubmittedAndActiveUserRequests();
    void enumerateRequestsReportsActiveSequencedRequests();

    bool runNoWaitUntil(AsyncEventLoop& eventLoop, Function<bool()> predicate, int maxAttempts = 8)
    {
        for (int idx = 0; idx < maxAttempts; ++idx)
        {
            if (predicate())
            {
                return true;
            }
            SC_TEST_EXPECT(eventLoop.runNoWait());
        }
        return predicate();
    }

    bool runOnceUntil(AsyncEventLoop& eventLoop, Function<bool()> predicate, int maxAttempts = 8)
    {
        for (int idx = 0; idx < maxAttempts; ++idx)
        {
            if (predicate())
            {
                return true;
            }
            SC_TEST_EXPECT(eventLoop.runOnce());
        }
        return predicate();
    }
};

void SC::AsyncContractTest::stopSuppressesNormalCallback()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    AsyncLoopWakeUp wakeUp;
    int             normalCallbacks = 0;
    int             closeCallbacks  = 0;

    Function<void(AsyncResult&)> afterStopped;
    afterStopped = [&](AsyncResult&) { closeCallbacks++; };

    wakeUp.callback = [&](AsyncLoopWakeUp::Result&) { normalCallbacks++; };
    SC_TEST_EXPECT(wakeUp.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(wakeUp.stop(eventLoop, &afterStopped));
    SC_TEST_EXPECT(runNoWaitUntil(eventLoop, [&] { return closeCallbacks == 1; }));
    SC_TEST_EXPECT(normalCallbacks == 0);
    SC_TEST_EXPECT(closeCallbacks == 1);
    SC_TEST_EXPECT(wakeUp.isFree());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::closeCallbackRunsAfterRequestIsFree()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    AsyncLoopWakeUp wakeUp;
    struct Context
    {
        Function<void(AsyncResult&)>* afterStopped         = nullptr;
        int                           closeCallbacks       = 0;
        bool                          wasFreeWhenCallback  = false;
        bool                          closeCallbackCleared = false;
    } context;

    Function<void(AsyncResult&)> afterStopped;
    context.afterStopped = &afterStopped;
    afterStopped         = [ctx = &context](AsyncResult& result)
    {
        ctx->closeCallbacks++;
        ctx->wasFreeWhenCallback  = result.async.isFree();
        ctx->closeCallbackCleared = result.async.getCloseCallback() == nullptr;
        *ctx->afterStopped        = Function<void(AsyncResult&)>();
    };

    wakeUp.callback = [](AsyncLoopWakeUp::Result&) {};
    SC_TEST_EXPECT(wakeUp.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(wakeUp.stop(eventLoop, &afterStopped));
    SC_TEST_EXPECT(runNoWaitUntil(eventLoop, [&] { return context.closeCallbacks == 1; }));
    SC_TEST_EXPECT(context.wasFreeWhenCallback);
    SC_TEST_EXPECT(context.closeCallbackCleared);
    SC_TEST_EXPECT(wakeUp.isFree());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::stopFreeRequestFails()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    AsyncLoopTimeout timeout;
    SC_TEST_EXPECT(not timeout.stop(eventLoop));
    SC_TEST_EXPECT(timeout.isFree());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::stopSubmittedRequestSuppressesNormalCallback()
{
    AsyncEventLoop   eventLoop;
    AsyncLoopTimeout timeout;
    int              normalCallbacks = 0;
    int              closeCallbacks  = 0;
    bool             wasFreeOnClose  = false;

    SC_TEST_EXPECT(eventLoop.create(options));
    Function<void(AsyncResult&)> afterStopped;
    afterStopped = [&](AsyncResult& result)
    {
        closeCallbacks++;
        wasFreeOnClose = result.async.isFree();
    };

    timeout.callback = [&](AsyncLoopTimeout::Result&) { normalCallbacks++; };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{1}));
    SC_TEST_EXPECT(eventLoop.getNumberOfSubmittedRequests() == 1);
    SC_TEST_EXPECT(timeout.stop(eventLoop, &afterStopped));
    SC_TEST_EXPECT(timeout.isCancelling());
    SC_TEST_EXPECT(runNoWaitUntil(eventLoop, [&] { return closeCallbacks == 1; }));
    SC_TEST_EXPECT(normalCallbacks == 0);
    SC_TEST_EXPECT(closeCallbacks == 1);
    SC_TEST_EXPECT(wasFreeOnClose);
    SC_TEST_EXPECT(timeout.isFree());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::latestCloseCallbackWinsWhileCancelling()
{
    AsyncEventLoop  eventLoop;
    AsyncLoopWakeUp wakeUp;
    int             firstCloseCallbacks  = 0;
    int             secondCloseCallbacks = 0;
    int             normalCallbacks      = 0;

    SC_TEST_EXPECT(eventLoop.create(options));
    Function<void(AsyncResult&)> firstAfterStopped;
    Function<void(AsyncResult&)> secondAfterStopped;
    firstAfterStopped  = [&](AsyncResult&) { firstCloseCallbacks++; };
    secondAfterStopped = [&](AsyncResult&) { secondCloseCallbacks++; };
    wakeUp.callback    = [&](AsyncLoopWakeUp::Result&) { normalCallbacks++; };

    SC_TEST_EXPECT(wakeUp.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(wakeUp.isActive());
    SC_TEST_EXPECT(wakeUp.stop(eventLoop, &firstAfterStopped));
    SC_TEST_EXPECT(wakeUp.isCancelling());
    SC_TEST_EXPECT(wakeUp.stop(eventLoop, &secondAfterStopped));
    SC_TEST_EXPECT(runNoWaitUntil(eventLoop, [&] { return secondCloseCallbacks == 1; }));
    SC_TEST_EXPECT(firstCloseCallbacks == 0);
    SC_TEST_EXPECT(secondCloseCallbacks == 1);
    SC_TEST_EXPECT(normalCallbacks == 0);
    SC_TEST_EXPECT(wakeUp.isFree());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::reactivationKeepsRequestOwned()
{
    AsyncEventLoop   eventLoop;
    AsyncLoopTimeout timeout;
    int              callbacks = 0;

    SC_TEST_EXPECT(eventLoop.create(options));
    timeout.callback = [&](AsyncLoopTimeout::Result& result)
    {
        callbacks++;
        SC_TEST_EXPECT(result.getAsync().isFree());
        if (callbacks == 1)
        {
            result.getAsync().relativeTimeout = TimeMs{1};
            result.reactivateRequest(true);
            SC_TEST_EXPECT(result.getAsync().isActive());
        }
    };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{1}));
    while (callbacks < 2)
    {
        SC_TEST_EXPECT(eventLoop.runOnce());
    }
    SC_TEST_EXPECT(callbacks == 2);
    SC_TEST_EXPECT(timeout.isFree());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::reactivatedRequestCanBeStoppedFromCallback()
{
    AsyncEventLoop   eventLoop;
    AsyncLoopTimeout timeout;
    struct Context
    {
        Function<void(AsyncResult&)> afterStopped;
        int                          normalCallbacks  = 0;
        int                          closeCallbacks   = 0;
        bool                         insideCallback   = false;
        bool                         closeWasDeferred = false;
    } context;

    SC_TEST_EXPECT(eventLoop.create(options));
    context.afterStopped = [this, ctx = &context](AsyncResult& result)
    {
        ctx->closeCallbacks++;
        ctx->closeWasDeferred = not ctx->insideCallback;
        SC_TEST_EXPECT(result.async.isFree());
    };

    timeout.callback = [this, ctx = &context](AsyncLoopTimeout::Result& result)
    {
        ctx->insideCallback = true;
        ctx->normalCallbacks++;
        result.getAsync().relativeTimeout = TimeMs{1};
        result.reactivateRequest(true);
        SC_TEST_EXPECT(result.getAsync().stop(result.eventLoop, &ctx->afterStopped));
        ctx->insideCallback = false;
    };

    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{1}));
    SC_TEST_EXPECT(runOnceUntil(eventLoop, [&] { return context.closeCallbacks == 1; }, 16));
    SC_TEST_EXPECT(context.normalCallbacks == 1);
    SC_TEST_EXPECT(context.closeCallbacks == 1);
    SC_TEST_EXPECT(context.closeWasDeferred);
    SC_TEST_EXPECT(timeout.isFree());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::lastReactivationDecisionWins()
{
    AsyncEventLoop   eventLoop;
    AsyncLoopTimeout timeout;
    int              callbacks = 0;

    SC_TEST_EXPECT(eventLoop.create(options));
    timeout.callback = [&](AsyncLoopTimeout::Result& result)
    {
        callbacks++;
        result.reactivateRequest(true);
        SC_TEST_EXPECT(result.getAsync().isActive());
        result.reactivateRequest(false);
        SC_TEST_EXPECT(result.getAsync().isFree());
    };

    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{1}));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(callbacks == 1);
    SC_TEST_EXPECT(timeout.isFree());
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(callbacks == 1);
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::nonReactivatedRequestCannotBeStoppedFromCallback()
{
    AsyncEventLoop   eventLoop;
    AsyncLoopTimeout timeout;
    struct Context
    {
        Function<void(AsyncResult&)> afterStopped;
        int                          normalCallbacks = 0;
        int                          closeCallbacks  = 0;
    } context;

    context.afterStopped = [ctx = &context](AsyncResult&) { ctx->closeCallbacks++; };

    SC_TEST_EXPECT(eventLoop.create(options));
    timeout.callback = [this, ctx = &context](AsyncLoopTimeout::Result& result)
    {
        ctx->normalCallbacks++;
        SC_TEST_EXPECT(result.getAsync().isFree());
        SC_TEST_EXPECT(not result.getAsync().stop(result.eventLoop, &ctx->afterStopped));
    };

    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{1}));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(context.normalCallbacks == 1);
    SC_TEST_EXPECT(context.closeCallbacks == 0);
    SC_TEST_EXPECT(timeout.isFree());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::reactivatedRequestUsesReplacedCallback()
{
    AsyncEventLoop   eventLoop;
    AsyncLoopTimeout timeout;
    struct Context
    {
        AsyncLoopTimeout* timeout        = nullptr;
        int               firstCallback  = 0;
        int               secondCallback = 0;
    } context;
    context.timeout = &timeout;

    SC_TEST_EXPECT(eventLoop.create(options));
    timeout.callback = [ctx = &context](AsyncLoopTimeout::Result& result)
    {
        ctx->firstCallback++;
        ctx->timeout->callback            = [ctx](AsyncLoopTimeout::Result&) { ctx->secondCallback++; };
        result.getAsync().relativeTimeout = TimeMs{1};
        result.reactivateRequest(true);
    };

    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{1}));
    SC_TEST_EXPECT(runOnceUntil(eventLoop, [&] { return context.secondCallback == 1; }, 16));
    SC_TEST_EXPECT(context.firstCallback == 1);
    SC_TEST_EXPECT(context.secondCallback == 1);
    SC_TEST_EXPECT(timeout.isFree());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::normalCallbackIsCopiedBeforeInvocation()
{
    AsyncEventLoop   eventLoop;
    AsyncLoopTimeout timeout;
    struct Context
    {
        AsyncLoopTimeout* timeout        = nullptr;
        int               firstCallback  = 0;
        int               secondCallback = 0;
    } context;
    context.timeout = &timeout;

    SC_TEST_EXPECT(eventLoop.create(options));
    timeout.callback = [ctx = &context](AsyncLoopTimeout::Result&)
    {
        ctx->firstCallback++;
        ctx->timeout->callback = [ctx](AsyncLoopTimeout::Result&) { ctx->secondCallback++; };
    };

    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{1}));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(context.firstCallback == 1);
    SC_TEST_EXPECT(context.secondCallback == 0);
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{1}));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(context.firstCallback == 1);
    SC_TEST_EXPECT(context.secondCallback == 1);
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::nonReactivatedRequestCanBeReusedInsideCallback()
{
    AsyncEventLoop   eventLoop;
    AsyncLoopTimeout timeout;
    struct Context
    {
        AsyncLoopTimeout* timeout        = nullptr;
        int               firstCallback  = 0;
        int               secondCallback = 0;
    } context;
    context.timeout = &timeout;

    SC_TEST_EXPECT(eventLoop.create(options));
    timeout.callback = [this, ctx = &context](AsyncLoopTimeout::Result& result)
    {
        ctx->firstCallback++;
        SC_TEST_EXPECT(result.getAsync().isFree());
        ctx->timeout->callback = [ctx](AsyncLoopTimeout::Result&) { ctx->secondCallback++; };
        SC_TEST_EXPECT(ctx->timeout->start(result.eventLoop, TimeMs{1}));
    };

    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{1}));
    while (context.secondCallback == 0)
    {
        SC_TEST_EXPECT(eventLoop.runOnce());
    }
    SC_TEST_EXPECT(context.firstCallback == 1);
    SC_TEST_EXPECT(context.secondCallback == 1);
    SC_TEST_EXPECT(timeout.isFree());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::sequenceClearsQueuedRequestsOnCancel()
{
    AsyncEventLoop   eventLoop;
    AsyncSequence    sequence;
    AsyncLoopWakeUp  wakeUp;
    AsyncLoopTimeout queuedTimeout;
    int              wakeCallbacks    = 0;
    int              timeoutCallbacks = 0;
    int              closeCallbacks   = 0;

    SC_TEST_EXPECT(eventLoop.create(options));
    Function<void(AsyncResult&)> afterStopped;
    afterStopped = [&](AsyncResult&) { closeCallbacks++; };

    wakeUp.callback        = [&](AsyncLoopWakeUp::Result&) { wakeCallbacks++; };
    queuedTimeout.callback = [&](AsyncLoopTimeout::Result&) { timeoutCallbacks++; };
    wakeUp.executeOn(sequence);
    queuedTimeout.executeOn(sequence);

    SC_TEST_EXPECT(wakeUp.start(eventLoop));
    SC_TEST_EXPECT(queuedTimeout.start(eventLoop, TimeMs{1}));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(wakeUp.isActive());
    SC_TEST_EXPECT(not queuedTimeout.isFree());
    SC_TEST_EXPECT(wakeUp.stop(eventLoop, &afterStopped));
    SC_TEST_EXPECT(runNoWaitUntil(eventLoop, [&] { return closeCallbacks == 1; }));
    SC_TEST_EXPECT(wakeCallbacks == 0);
    SC_TEST_EXPECT(timeoutCallbacks == 0);
    SC_TEST_EXPECT(wakeUp.isFree());
    SC_TEST_EXPECT(queuedTimeout.isFree());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::sequenceClearsQueuedRequestsOnError()
{
    AsyncEventLoop    eventLoop;
    AsyncTaskSequence sequence;
    ThreadPool        threadPool;
    AsyncLoopWork     work;
    AsyncLoopWork     queuedWork;

    struct Context
    {
        int workCalls           = 0;
        int workCallbacks       = 0;
        int queuedWorkCalls     = 0;
        int queuedWorkCallbacks = 0;
    } context;

    SC_TEST_EXPECT(eventLoop.create(options));
    SC_TEST_EXPECT(threadPool.create(1));

    work.work = [ctx = &context]
    {
        ctx->workCalls++;
        return Result::Error("AsyncContractTest expected work error");
    };
    work.callback = [this, ctx = &context](AsyncLoopWork::Result& result)
    {
        ctx->workCallbacks++;
        SC_TEST_EXPECT(not result.isValid());
    };
    queuedWork.work = [ctx = &context]
    {
        ctx->queuedWorkCalls++;
        return Result(true);
    };
    queuedWork.callback = [ctx = &context](AsyncLoopWork::Result&) { ctx->queuedWorkCallbacks++; };

    SC_TEST_EXPECT(work.executeOn(sequence, threadPool));
    SC_TEST_EXPECT(queuedWork.executeOn(sequence, threadPool));
    SC_TEST_EXPECT(work.start(eventLoop));
    SC_TEST_EXPECT(queuedWork.start(eventLoop));

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(context.workCalls == 1);
    SC_TEST_EXPECT(context.workCallbacks == 1);
    SC_TEST_EXPECT(context.queuedWorkCalls == 0);
    SC_TEST_EXPECT(context.queuedWorkCallbacks == 0);
    SC_TEST_EXPECT(work.isFree());
    SC_TEST_EXPECT(queuedWork.isFree());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::sequenceResumesQueuedRequestsOnCancelWhenConfigured()
{
    AsyncEventLoop   eventLoop;
    AsyncSequence    sequence;
    AsyncLoopWakeUp  wakeUp;
    AsyncLoopTimeout queuedTimeout;
    int              wakeCallbacks    = 0;
    int              timeoutCallbacks = 0;
    int              closeCallbacks   = 0;

    sequence.clearSequenceOnCancel = false;

    SC_TEST_EXPECT(eventLoop.create(options));
    Function<void(AsyncResult&)> afterStopped;
    afterStopped = [&](AsyncResult&) { closeCallbacks++; };

    wakeUp.callback        = [&](AsyncLoopWakeUp::Result&) { wakeCallbacks++; };
    queuedTimeout.callback = [&](AsyncLoopTimeout::Result&) { timeoutCallbacks++; };
    wakeUp.executeOn(sequence);
    queuedTimeout.executeOn(sequence);

    SC_TEST_EXPECT(wakeUp.start(eventLoop));
    SC_TEST_EXPECT(queuedTimeout.start(eventLoop, TimeMs{1}));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(wakeUp.isActive());
    SC_TEST_EXPECT(wakeUp.stop(eventLoop, &afterStopped));
    SC_TEST_EXPECT(runOnceUntil(eventLoop, [&] { return closeCallbacks == 1 and timeoutCallbacks == 1; }, 16));
    SC_TEST_EXPECT(wakeCallbacks == 0);
    SC_TEST_EXPECT(timeoutCallbacks == 1);
    SC_TEST_EXPECT(closeCallbacks == 1);
    SC_TEST_EXPECT(wakeUp.isFree());
    SC_TEST_EXPECT(queuedTimeout.isFree());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::sequenceResumesQueuedRequestsOnErrorWhenConfigured()
{
    AsyncEventLoop    eventLoop;
    AsyncTaskSequence sequence;
    ThreadPool        threadPool;
    AsyncLoopWork     work;
    AsyncLoopWork     queuedWork;

    struct Context
    {
        int  workCalls           = 0;
        int  workCallbacks       = 0;
        int  queuedWorkCalls     = 0;
        int  queuedWorkCallbacks = 0;
        bool workWasValid        = true;
        bool queuedWasValid      = false;
    } context;

    sequence.clearSequenceOnError = false;

    SC_TEST_EXPECT(eventLoop.create(options));
    SC_TEST_EXPECT(threadPool.create(1));

    work.work = [ctx = &context]
    {
        ctx->workCalls++;
        return Result::Error("AsyncContractTest expected work error");
    };
    work.callback = [ctx = &context](AsyncLoopWork::Result& result)
    {
        ctx->workCallbacks++;
        ctx->workWasValid = result.isValid();
    };
    queuedWork.work = [ctx = &context]
    {
        ctx->queuedWorkCalls++;
        return Result(true);
    };
    queuedWork.callback = [ctx = &context](AsyncLoopWork::Result& result)
    {
        ctx->queuedWorkCallbacks++;
        ctx->queuedWasValid = result.isValid();
    };

    SC_TEST_EXPECT(work.executeOn(sequence, threadPool));
    SC_TEST_EXPECT(queuedWork.executeOn(sequence, threadPool));
    SC_TEST_EXPECT(work.start(eventLoop));
    SC_TEST_EXPECT(queuedWork.start(eventLoop));

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(context.workCalls == 1);
    SC_TEST_EXPECT(context.workCallbacks == 1);
    SC_TEST_EXPECT(not context.workWasValid);
    SC_TEST_EXPECT(context.queuedWorkCalls == 1);
    SC_TEST_EXPECT(context.queuedWorkCallbacks == 1);
    SC_TEST_EXPECT(context.queuedWasValid);
    SC_TEST_EXPECT(work.isFree());
    SC_TEST_EXPECT(queuedWork.isFree());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::threadPoolModeCanForceSuppliedPool()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    if (eventLoop.needsThreadPoolForFileOperations())
    {
        SC_TEST_EXPECT(eventLoop.close());
        return;
    }

    SmallStringNative<255> dirPath  = StringEncoding::Native;
    SmallStringNative<255> filePath = StringEncoding::Native;
    const StringView       dirName  = "AsyncContractTest";
    const StringView       fileName = "thread-pool-mode.txt";
    SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory.view(), dirName}));
    SC_TEST_EXPECT(Path::join(filePath, {dirPath.view(), fileName}));

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(dirName));
    SC_TEST_EXPECT(fs.write(filePath.view(), StringView("abcd").toCharSpan()));

    FileDescriptor fd;
    FileOpen       openMode;
    openMode.mode     = FileOpen::Read;
    openMode.blocking = true;
    SC_TEST_EXPECT(fd.open(filePath.view(), openMode));

    FileDescriptor::Handle handle = FileDescriptor::Invalid;
    SC_TEST_EXPECT(fd.get(handle, Result::Error("AsyncContractTest invalid file handle")));

    ThreadPool uncreatedThreadPool;

    char              nativeBuffer[4] = {};
    AsyncFileRead     nativeRead;
    AsyncTaskSequence nativeSequence;
    int               nativeCallbacks = 0;
    bool              nativeWasValid  = false;
    nativeRead.callback               = [&](AsyncFileRead::Result& result)
    {
        nativeCallbacks++;
        nativeWasValid = result.isValid();
    };
    nativeRead.handle = handle;
    nativeRead.buffer = {nativeBuffer, sizeof(nativeBuffer)};
    SC_TEST_EXPECT(nativeRead.executeOn(nativeSequence, uncreatedThreadPool));
    SC_TEST_EXPECT(nativeRead.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(nativeCallbacks == 1);
    SC_TEST_EXPECT(nativeWasValid);
    SC_TEST_EXPECT(nativeRead.isFree());

    char              forcedBuffer[4] = {};
    AsyncFileRead     forcedRead;
    AsyncTaskSequence forcedSequence;
    int               forcedCallbacks = 0;
    bool              forcedWasValid  = true;
    forcedRead.callback               = [&](AsyncFileRead::Result& result)
    {
        forcedCallbacks++;
        forcedWasValid = result.isValid();
    };
    forcedRead.handle = handle;
    forcedRead.buffer = {forcedBuffer, sizeof(forcedBuffer)};
    SC_TEST_EXPECT(forcedRead.executeOn(forcedSequence, uncreatedThreadPool, AsyncThreadPoolMode::ForceThreadPool));
    SC_TEST_EXPECT(forcedRead.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(forcedCallbacks == 1);
    SC_TEST_EXPECT(not forcedWasValid);
    SC_TEST_EXPECT(forcedRead.isFree());

    struct OpenContext
    {
        int  nativeCallbacks = 0;
        bool nativeWasValid  = false;
        int  forcedCallbacks = 0;
        bool forcedWasValid  = true;
    } openContext;

    AsyncFileSystemOperation nativeOpen;
    nativeOpen.callback = [this, ctx = &openContext](AsyncFileSystemOperation::Result& result)
    {
        ctx->nativeCallbacks++;
        ctx->nativeWasValid = result.isValid();
        if (result.isValid())
        {
            FileDescriptor openedFile(result.completionData.handle);
            SC_TEST_EXPECT(openedFile.close());
        }
    };
    SC_TEST_EXPECT(nativeOpen.setThreadPool(uncreatedThreadPool));
    SC_TEST_EXPECT(nativeOpen.open(eventLoop, filePath.view(), openMode));

    alignas(uint64_t) uint8_t eventsMemory[8 * 1024];
    AsyncKernelEvents         kernelEvents;
    kernelEvents.eventsMemory = eventsMemory;

    SC_TEST_EXPECT(eventLoop.submitRequests(kernelEvents));
    SC_TEST_EXPECT(nativeOpen.isActive());

    bool foundActiveNativeOpen = false;
    eventLoop.enumerateRequests(
        [&nativeOpen, &foundActiveNativeOpen](AsyncRequest& request)
        {
            if (&request == &nativeOpen)
            {
                foundActiveNativeOpen = true;
            }
        });
    SC_TEST_EXPECT(foundActiveNativeOpen);

    SC_TEST_EXPECT(eventLoop.blockingPoll(kernelEvents));
    SC_TEST_EXPECT(eventLoop.dispatchCompletions(kernelEvents));
    SC_TEST_EXPECT(openContext.nativeCallbacks == 1);
    SC_TEST_EXPECT(openContext.nativeWasValid);

    AsyncFileSystemOperation forcedOpen;
    forcedOpen.callback = [ctx = &openContext](AsyncFileSystemOperation::Result& result)
    {
        ctx->forcedCallbacks++;
        ctx->forcedWasValid = result.isValid();
    };
    SC_TEST_EXPECT(forcedOpen.setThreadPool(uncreatedThreadPool, AsyncThreadPoolMode::ForceThreadPool));
    SC_TEST_EXPECT(forcedOpen.open(eventLoop, filePath.view(), openMode));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(openContext.forcedCallbacks == 1);
    SC_TEST_EXPECT(not openContext.forcedWasValid);

    SC_TEST_EXPECT(fd.close());
    SC_TEST_EXPECT(fs.removeFile(filePath.view()));
    SC_TEST_EXPECT(fs.removeEmptyDirectory(dirPath.view()));
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::validationFailureLeavesRequestFree()
{
    AsyncEventLoop eventLoop;
    AsyncSignal    signal;
    int            callbacks = 0;

    SC_TEST_EXPECT(eventLoop.create(options));
    signal.callback = [&](AsyncSignal::Result&) { callbacks++; };

    Result startResult = signal.start(eventLoop, -1);
    SC_TEST_EXPECT(not startResult);
    SC_TEST_EXPECT(signal.isFree());
    SC_TEST_EXPECT(eventLoop.getNumberOfSubmittedRequests() == 0);
    SC_TEST_EXPECT(eventLoop.getNumberOfActiveRequests() == 0);

    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(callbacks == 0);
    SC_TEST_EXPECT(signal.isFree());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::loopCloseFreesSubmittedRequests()
{
    AsyncEventLoop   eventLoop;
    AsyncLoopTimeout timeout;
    int              callbacks = 0;

    SC_TEST_EXPECT(eventLoop.create(options));
    timeout.callback = [&](AsyncLoopTimeout::Result&) { callbacks++; };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{100}));
    SC_TEST_EXPECT(not timeout.isFree());
    SC_TEST_EXPECT(eventLoop.close());
    SC_TEST_EXPECT(callbacks == 0);
    SC_TEST_EXPECT(timeout.isFree());
}

void SC::AsyncContractTest::loopCloseFreesActiveRequests()
{
    AsyncEventLoop  eventLoop;
    AsyncLoopWakeUp wakeUp;
    int             callbacks = 0;

    SC_TEST_EXPECT(eventLoop.create(options));
    wakeUp.callback = [&](AsyncLoopWakeUp::Result&) { callbacks++; };
    SC_TEST_EXPECT(wakeUp.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(wakeUp.isActive());
    SC_TEST_EXPECT(eventLoop.close());
    SC_TEST_EXPECT(callbacks == 0);
    SC_TEST_EXPECT(wakeUp.isFree());
}

void SC::AsyncContractTest::loopCloseDrainsPendingCloseCallback()
{
    AsyncEventLoop  eventLoop;
    AsyncLoopWakeUp wakeUp;
    int             normalCallbacks = 0;
    int             closeCallbacks  = 0;
    bool            wasFreeOnClose  = false;

    Function<void(AsyncResult&)> afterStopped;
    afterStopped = [&](AsyncResult& result)
    {
        closeCallbacks++;
        wasFreeOnClose = result.async.isFree();
    };

    SC_TEST_EXPECT(eventLoop.create(options));
    wakeUp.callback = [&](AsyncLoopWakeUp::Result&) { normalCallbacks++; };
    SC_TEST_EXPECT(wakeUp.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(wakeUp.stop(eventLoop, &afterStopped));
    SC_TEST_EXPECT(eventLoop.close());
    SC_TEST_EXPECT(normalCallbacks == 0);
    SC_TEST_EXPECT(closeCallbacks == 1);
    SC_TEST_EXPECT(wasFreeOnClose);
    SC_TEST_EXPECT(wakeUp.isFree());
}

void SC::AsyncContractTest::loopCloseSuppressesPostedManualCompletion()
{
    AsyncEventLoop          eventLoop;
    AsyncExternalCompletion completion;
    int                     callbacks = 0;

    SC_TEST_EXPECT(eventLoop.create(options));
    completion.callback = [&](AsyncExternalCompletion::Result&) { callbacks++; };
    SC_TEST_EXPECT(completion.start(eventLoop));
    SC_TEST_EXPECT(completion.markSubmissionPending());
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(eventLoop.postExternalCompletion(completion, 17));
    SC_TEST_EXPECT(eventLoop.close());
    SC_TEST_EXPECT(callbacks == 0);
    SC_TEST_EXPECT(not completion.hasSubmissionPending());
    SC_TEST_EXPECT(completion.isFree());
}

void SC::AsyncContractTest::loopCanBeRecreatedAfterClose()
{
    AsyncEventLoop  eventLoop;
    AsyncLoopWakeUp wakeUp;
    int             callbacks = 0;

    wakeUp.callback = [&](AsyncLoopWakeUp::Result&) { callbacks++; };

    SC_TEST_EXPECT(eventLoop.create(options));
    SC_TEST_EXPECT(wakeUp.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(wakeUp.wakeUp(eventLoop));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(callbacks == 1);
    SC_TEST_EXPECT(wakeUp.isFree());
    SC_TEST_EXPECT(eventLoop.close());
    SC_TEST_EXPECT(not eventLoop.isInitialized());

    SC_TEST_EXPECT(eventLoop.create(options));
    SC_TEST_EXPECT(eventLoop.isInitialized());
    SC_TEST_EXPECT(wakeUp.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(wakeUp.wakeUp(eventLoop));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(callbacks == 2);
    SC_TEST_EXPECT(wakeUp.isFree());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::listenersWrapBlockingPollOnly()
{
    AsyncEventLoop  eventLoop;
    AsyncLoopWakeUp wakeUp;

    struct ListenerContext
    {
        int callbacks        = 0;
        int beforePollCalls  = 0;
        int afterPollCalls   = 0;
        int callbackSnapshot = 0;
    } context;

    AsyncEventLoopListeners listeners;
    listeners.beforeBlockingPoll = [&context](AsyncEventLoop&) { context.beforePollCalls++; };
    listeners.afterBlockingPoll  = [&context](AsyncEventLoop&) { context.afterPollCalls++; };

    alignas(uint64_t) uint8_t eventsMemory[8 * 1024];
    AsyncKernelEvents         kernelEvents;
    kernelEvents.eventsMemory = eventsMemory;

    SC_TEST_EXPECT(eventLoop.create(options));
    eventLoop.setListeners(&listeners);

    wakeUp.callback = [&](AsyncLoopWakeUp::Result&)
    {
        context.callbacks++;
        context.callbackSnapshot = context.beforePollCalls + context.afterPollCalls;
    };

    SC_TEST_EXPECT(wakeUp.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.submitRequests(kernelEvents));
    SC_TEST_EXPECT(context.beforePollCalls == 0);
    SC_TEST_EXPECT(context.afterPollCalls == 0);

    SC_TEST_EXPECT(wakeUp.wakeUp(eventLoop));
    SC_TEST_EXPECT(eventLoop.blockingPoll(kernelEvents));
    SC_TEST_EXPECT(context.beforePollCalls == 1);
    SC_TEST_EXPECT(context.afterPollCalls == 1);
    SC_TEST_EXPECT(context.callbacks == 0);

    SC_TEST_EXPECT(eventLoop.dispatchCompletions(kernelEvents));
    SC_TEST_EXPECT(context.callbacks == 1);
    SC_TEST_EXPECT(context.callbackSnapshot == 2);
    SC_TEST_EXPECT(context.beforePollCalls == 1);
    SC_TEST_EXPECT(context.afterPollCalls == 1);

    eventLoop.setListeners(nullptr);
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::wakeUpCoalescingAndOneShotBehavior()
{
    AsyncEventLoop  eventLoop;
    AsyncLoopWakeUp wakeUp;
    int             callbacks      = 0;
    uint32_t        lastDeliveries = 0;

    SC_TEST_EXPECT(eventLoop.create(options));
    wakeUp.callback = [&](AsyncLoopWakeUp::Result& result)
    {
        callbacks++;
        lastDeliveries = result.completionData.deliveryCount;
    };
    SC_TEST_EXPECT(wakeUp.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(wakeUp.wakeUp(eventLoop));
    SC_TEST_EXPECT(wakeUp.wakeUp(eventLoop));
    SC_TEST_EXPECT(wakeUp.wakeUp(eventLoop));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(callbacks == 1);
    SC_TEST_EXPECT(lastDeliveries == 3);
    SC_TEST_EXPECT(wakeUp.isFree());

    SC_TEST_EXPECT(wakeUp.wakeUp(eventLoop));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(callbacks == 1);

    SC_TEST_EXPECT(wakeUp.start(eventLoop));
    SC_TEST_EXPECT(wakeUp.wakeUp(eventLoop));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(callbacks == 2);
    SC_TEST_EXPECT(lastDeliveries == 1);
    SC_TEST_EXPECT(wakeUp.isFree());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::activeCountExclusionPreservesCallbacks()
{
    AsyncEventLoop  eventLoop;
    AsyncLoopWakeUp wakeUp;
    AsyncLoopWakeUp driverWakeUp;
    int             callbacks       = 0;
    int             driverCallbacks = 0;

    SC_TEST_EXPECT(eventLoop.create(options));
    wakeUp.callback       = [&](AsyncLoopWakeUp::Result&) { callbacks++; };
    driverWakeUp.callback = [&](AsyncLoopWakeUp::Result&) { driverCallbacks++; };

    SC_TEST_EXPECT(wakeUp.start(eventLoop));
    SC_TEST_EXPECT(driverWakeUp.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(wakeUp.isActive());
    SC_TEST_EXPECT(driverWakeUp.isActive());
    SC_TEST_EXPECT(eventLoop.getNumberOfActiveRequests() == 2);

    eventLoop.excludeFromActiveCount(wakeUp);
    SC_TEST_EXPECT(eventLoop.getNumberOfActiveRequests() == 1);

    SC_TEST_EXPECT(wakeUp.wakeUp(eventLoop));
    SC_TEST_EXPECT(driverWakeUp.wakeUp(eventLoop));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(callbacks == 1);
    SC_TEST_EXPECT(driverCallbacks == 1);
    SC_TEST_EXPECT(wakeUp.isFree());
    SC_TEST_EXPECT(driverWakeUp.isFree());

    SC_TEST_EXPECT(wakeUp.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(wakeUp.isActive());
    eventLoop.excludeFromActiveCount(wakeUp);
    SC_TEST_EXPECT(eventLoop.getNumberOfActiveRequests() == 0);
    eventLoop.includeInActiveCount(wakeUp);
    SC_TEST_EXPECT(eventLoop.getNumberOfActiveRequests() == 1);

    SC_TEST_EXPECT(eventLoop.close());
    SC_TEST_EXPECT(wakeUp.isFree());
}

void SC::AsyncContractTest::excludedActiveRequestDoesNotKeepRunAlive()
{
    AsyncEventLoop  eventLoop;
    AsyncLoopWakeUp wakeUp;
    int             callbacks = 0;

    SC_TEST_EXPECT(eventLoop.create(options));
    wakeUp.callback = [&](AsyncLoopWakeUp::Result&) { callbacks++; };

    SC_TEST_EXPECT(wakeUp.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(wakeUp.isActive());
    SC_TEST_EXPECT(eventLoop.getNumberOfActiveRequests() == 1);

    eventLoop.excludeFromActiveCount(wakeUp);
    SC_TEST_EXPECT(eventLoop.getNumberOfActiveRequests() == 0);
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(callbacks == 0);
    SC_TEST_EXPECT(wakeUp.isActive());

    SC_TEST_EXPECT(eventLoop.close());
    SC_TEST_EXPECT(wakeUp.isFree());
}

void SC::AsyncContractTest::runExitsOnInterruptWithActiveWork()
{
    AsyncEventLoop   eventLoop;
    AsyncLoopTimeout timeout;
    int              callbacks = 0;

    SC_TEST_EXPECT(eventLoop.create(options));
    timeout.callback = [&](AsyncLoopTimeout::Result& result)
    {
        callbacks++;
        result.getAsync().relativeTimeout = TimeMs{1};
        result.reactivateRequest(true);
        eventLoop.interrupt();
    };

    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{1}));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(callbacks == 1);
    SC_TEST_EXPECT(not timeout.isFree());
    SC_TEST_EXPECT(eventLoop.getNumberOfActiveRequests() + eventLoop.getNumberOfSubmittedRequests() == 1);

    SC_TEST_EXPECT(eventLoop.close());
    SC_TEST_EXPECT(timeout.isFree());
}

void SC::AsyncContractTest::enumerateRequestsReportsSubmittedAndActiveUserRequests()
{
    AsyncEventLoop   eventLoop;
    AsyncLoopWakeUp  activeWakeUp;
    AsyncLoopTimeout submittedTimeout;

    SC_TEST_EXPECT(eventLoop.create(options));
    activeWakeUp.callback     = [](AsyncLoopWakeUp::Result&) {};
    submittedTimeout.callback = [](AsyncLoopTimeout::Result&) {};

    SC_TEST_EXPECT(activeWakeUp.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(activeWakeUp.isActive());
    SC_TEST_EXPECT(submittedTimeout.start(eventLoop, TimeMs{100}));

    struct EnumerationContext
    {
        AsyncRequest* activeWakeUp       = nullptr;
        AsyncRequest* submittedTimeout   = nullptr;
        int           enumeratedRequests = 0;
        bool          foundActiveWakeUp  = false;
        bool          foundSubmitted     = false;
    } context;
    context.activeWakeUp     = &activeWakeUp;
    context.submittedTimeout = &submittedTimeout;
    eventLoop.enumerateRequests(
        [ctx = &context](AsyncRequest& request)
        {
            ctx->enumeratedRequests++;
            if (&request == ctx->activeWakeUp)
            {
                ctx->foundActiveWakeUp = true;
            }
            if (&request == ctx->submittedTimeout)
            {
                ctx->foundSubmitted = true;
            }
        });

    SC_TEST_EXPECT(context.foundActiveWakeUp);
    SC_TEST_EXPECT(context.foundSubmitted);
    SC_TEST_EXPECT(context.enumeratedRequests == 2);
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncContractTest::enumerateRequestsReportsActiveSequencedRequests()
{
    AsyncEventLoop  eventLoop;
    AsyncSequence   sequence;
    AsyncLoopWakeUp activeWakeUp;

    SC_TEST_EXPECT(eventLoop.create(options));
    activeWakeUp.executeOn(sequence);
    activeWakeUp.callback = [](AsyncLoopWakeUp::Result&) {};

    SC_TEST_EXPECT(activeWakeUp.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(activeWakeUp.isActive());

    struct EnumerationContext
    {
        AsyncRequest* activeWakeUp       = nullptr;
        int           enumeratedRequests = 0;
        bool          foundActiveWakeUp  = false;
    } context;
    context.activeWakeUp = &activeWakeUp;
    eventLoop.enumerateRequests(
        [ctx = &context](AsyncRequest& request)
        {
            ctx->enumeratedRequests++;
            if (&request == ctx->activeWakeUp)
            {
                ctx->foundActiveWakeUp = true;
            }
        });

    SC_TEST_EXPECT(context.foundActiveWakeUp);
    SC_TEST_EXPECT(context.enumeratedRequests == 1);
    SC_TEST_EXPECT(eventLoop.close());
    SC_TEST_EXPECT(activeWakeUp.isFree());
}

namespace SC
{
void runAsyncContractTest(SC::TestReport& report) { AsyncContractTest test(report); }
} // namespace SC
