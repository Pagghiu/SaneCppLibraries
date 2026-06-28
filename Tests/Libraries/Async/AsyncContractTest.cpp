// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Async/Async.h"
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
            if (test_section("reactivation keeps request owned"))
            {
                reactivationKeepsRequestOwned();
            }
            if (test_section("reactivated request can be stopped from callback"))
            {
                reactivatedRequestCanBeStoppedFromCallback();
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
            if (test_section("loop close frees submitted requests"))
            {
                loopCloseFreesSubmittedRequests();
            }
            if (test_section("loop close frees active requests"))
            {
                loopCloseFreesActiveRequests();
            }
            if (test_section("wakeUp coalescing and one-shot behavior"))
            {
                wakeUpCoalescingAndOneShotBehavior();
            }
            if (test_section("enumerateRequests reports submitted and active user requests"))
            {
                enumerateRequestsReportsSubmittedAndActiveUserRequests();
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
    void reactivationKeepsRequestOwned();
    void reactivatedRequestCanBeStoppedFromCallback();
    void normalCallbackIsCopiedBeforeInvocation();
    void nonReactivatedRequestCanBeReusedInsideCallback();
    void sequenceClearsQueuedRequestsOnCancel();
    void sequenceClearsQueuedRequestsOnError();
    void loopCloseFreesSubmittedRequests();
    void loopCloseFreesActiveRequests();
    void wakeUpCoalescingAndOneShotBehavior();
    void enumerateRequestsReportsSubmittedAndActiveUserRequests();

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
    int             closeCallbacks      = 0;
    bool            wasFreeWhenCallback = false;

    Function<void(AsyncResult&)> afterStopped;
    afterStopped = [&](AsyncResult& result)
    {
        closeCallbacks++;
        wasFreeWhenCallback = result.async.isFree();
    };

    wakeUp.callback = [](AsyncLoopWakeUp::Result&) {};
    SC_TEST_EXPECT(wakeUp.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(wakeUp.stop(eventLoop, &afterStopped));
    SC_TEST_EXPECT(runNoWaitUntil(eventLoop, [&] { return closeCallbacks == 1; }));
    SC_TEST_EXPECT(wasFreeWhenCallback);
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

namespace SC
{
void runAsyncContractTest(SC::TestReport& report) { AsyncContractTest test(report); }
} // namespace SC
