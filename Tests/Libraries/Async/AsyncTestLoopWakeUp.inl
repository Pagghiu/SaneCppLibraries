// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"
#include "Libraries/Time/Time.h"

void SC::AsyncTest::loopWakeUpFromExternalThread()
{
    // TODO: This test is not actually testing anything (on Linux)
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    Thread newThread;

    struct Context
    {
        AsyncEventLoop& eventLoop;
        int             threadWasCalled;
        int             wakeUpSucceeded;
    } context                 = {eventLoop, 0, 0};
    auto externalThreadLambda = [&context](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("test"));
        context.threadWasCalled++;
        if (context.eventLoop.wakeUpFromExternalThread())
        {
            context.wakeUpSucceeded++;
        }
    };
    SC_TEST_EXPECT(newThread.start(externalThreadLambda));
    Time::HighResolutionCounter start, end;
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(newThread.join());
    SC_TEST_EXPECT(newThread.start(externalThreadLambda));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(newThread.join());
    SC_TEST_EXPECT(context.threadWasCalled == 2);
    SC_TEST_EXPECT(context.wakeUpSucceeded == 2);
}

void SC::AsyncTest::loopWakeUp()
{
    struct Context
    {
        int      wakeUp1Called   = 0;
        int      wakeUp2Called   = 0;
        uint64_t wakeUp1ThreadID = 0;
    } context;
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    AsyncLoopWakeUp wakeUp1;
    AsyncLoopWakeUp wakeUp2;
    wakeUp1.setDebugName("wakeUp1");
    wakeUp1.callback = [this, &context](AsyncLoopWakeUp::Result& res)
    {
        context.wakeUp1ThreadID = Thread::CurrentThreadID();
        context.wakeUp1Called++;
        SC_TEST_EXPECT(not res.getAsync().isActive());
    };
    SC_TEST_EXPECT(wakeUp1.start(eventLoop));
    wakeUp2.setDebugName("wakeUp2");
    wakeUp2.callback = [this, &context](AsyncLoopWakeUp::Result& res)
    {
        context.wakeUp2Called++;
        SC_TEST_EXPECT(res.getAsync().stop(res.eventLoop));
    };
    SC_TEST_EXPECT(wakeUp2.start(eventLoop));

    Thread newThread1;
    Thread newThread2;
    struct ThreadContext
    {
        AsyncEventLoop&  eventLoop;
        AsyncLoopWakeUp& asyncWakeUp;
        Result           loopRes = Result(false);
    };
    ThreadContext threadContext1 = {eventLoop, wakeUp1};
    ThreadContext threadContext2 = {eventLoop, wakeUp2};

    auto action1 = [&threadContext1](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("test1"));
        threadContext1.loopRes = threadContext1.asyncWakeUp.wakeUp(threadContext1.eventLoop);
    };
    auto action2 = [&threadContext1, &threadContext2](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("test2"));
        threadContext2.loopRes = threadContext1.asyncWakeUp.wakeUp(threadContext1.eventLoop);
    };
    SC_TEST_EXPECT(newThread1.start(action1));
    SC_TEST_EXPECT(newThread2.start(action2));
    SC_TEST_EXPECT(newThread1.join());
    SC_TEST_EXPECT(newThread2.join());
    SC_TEST_EXPECT(threadContext1.loopRes);
    SC_TEST_EXPECT(threadContext2.loopRes);
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(context.wakeUp1Called == 1);
    SC_TEST_EXPECT(context.wakeUp2Called == 0);
    SC_TEST_EXPECT(context.wakeUp1ThreadID == Thread::CurrentThreadID());
}

void SC::AsyncTest::loopWakeUpCoalescing()
{
    struct Context
    {
        int      coalescedCallbacks           = 0;
        int      coalescedDeliveries          = 0;
        int      nonCoalescedCallbacks        = 0;
        int      nonCoalescedDeliveries       = 0;
        uint64_t coalescedThreadID            = 0;
        uint64_t nonCoalescedThreadID         = 0;
        Result   coalescedWakeUpResults[3]    = {Result(false), Result(false), Result(false)};
        Result   nonCoalescedWakeUpResults[3] = {Result(false), Result(false), Result(false)};
    } context;

    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    AsyncLoopWakeUp coalescedWakeUp;
    coalescedWakeUp.setDebugName("coalescedWakeUp");
    coalescedWakeUp.callback = [this, &context](AsyncLoopWakeUp::Result& result)
    {
        context.coalescedCallbacks++;
        context.coalescedDeliveries += static_cast<int>(result.completionData.deliveryCount);
        context.coalescedThreadID = Thread::CurrentThreadID();
        SC_TEST_EXPECT(result.completionData.deliveryCount == 3);
        SC_TEST_EXPECT(not result.getAsync().isActive());
    };
    SC_TEST_EXPECT(coalescedWakeUp.start(eventLoop));

    AsyncLoopWakeUpOptions nonCoalescingOptions;
    nonCoalescingOptions.coalesce = false;

    AsyncLoopWakeUp nonCoalescedWakeUp;
    nonCoalescedWakeUp.setDebugName("nonCoalescedWakeUp");
    nonCoalescedWakeUp.callback = [this, &context](AsyncLoopWakeUp::Result& result)
    {
        context.nonCoalescedCallbacks++;
        context.nonCoalescedDeliveries += static_cast<int>(result.completionData.deliveryCount);
        context.nonCoalescedThreadID = Thread::CurrentThreadID();
        SC_TEST_EXPECT(result.completionData.deliveryCount == 1);
        SC_TEST_EXPECT(not result.getAsync().isActive());
        if (context.nonCoalescedCallbacks < 3)
        {
            result.reactivateRequest(true);
        }
    };
    SC_TEST_EXPECT(nonCoalescedWakeUp.start(eventLoop, nonCoalescingOptions));

    Thread senderThread;
    struct ThreadContext
    {
        Context*         context;
        AsyncEventLoop*  eventLoop;
        AsyncLoopWakeUp* coalescedWakeUp;
        AsyncLoopWakeUp* nonCoalescedWakeUp;
    } threadContext   = {&context, &eventLoop, &coalescedWakeUp, &nonCoalescedWakeUp};
    auto senderLambda = [&threadContext](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("wakeUp"));
        for (int i = 0; i < 3; ++i)
        {
            threadContext.context->coalescedWakeUpResults[i] =
                threadContext.coalescedWakeUp->wakeUp(*threadContext.eventLoop);
            threadContext.context->nonCoalescedWakeUpResults[i] =
                threadContext.nonCoalescedWakeUp->wakeUp(*threadContext.eventLoop);
        }
    };
    SC_TEST_EXPECT(senderThread.start(senderLambda));
    SC_TEST_EXPECT(senderThread.join());

    for (int i = 0; i < 3; ++i)
    {
        SC_TEST_EXPECT(context.coalescedWakeUpResults[i]);
        SC_TEST_EXPECT(context.nonCoalescedWakeUpResults[i]);
    }

    Time::HighResolutionCounter timeout;
    timeout.snap();
    timeout = timeout.offsetBy(500_ms);

    while ((context.coalescedCallbacks < 1 or context.nonCoalescedCallbacks < 3))
    {
        SC_TEST_EXPECT(eventLoop.runNoWait());
        Time::HighResolutionCounter now;
        now.snap();
        if (now.isLaterThanOrEqualTo(timeout))
        {
            break;
        }
        Thread::Sleep(1);
    }

    SC_TEST_EXPECT(context.coalescedCallbacks == 1);
    SC_TEST_EXPECT(context.coalescedDeliveries == 3);
    SC_TEST_EXPECT(context.nonCoalescedCallbacks == 3);
    SC_TEST_EXPECT(context.nonCoalescedDeliveries == 3);
    SC_TEST_EXPECT(context.coalescedThreadID == Thread::CurrentThreadID());
    SC_TEST_EXPECT(context.nonCoalescedThreadID == Thread::CurrentThreadID());
}

void SC::AsyncTest::loopWakeUpEventObject()
{
    struct TestParams
    {
        AsyncEventLoop  eventLoop;
        AsyncLoopWakeUp asyncWakeUp;

        EventObject eventObject;
        Result      wakeUpRes = Result(false);

        int      notifier1Called         = 0;
        int      observedNotifier1Called = -1;
        uint64_t callbackThreadID        = 0;
    } params;

    SC_TEST_EXPECT(params.eventLoop.create(options));

    params.asyncWakeUp.callback = [&](AsyncLoopWakeUp::Result&)
    {
        params.callbackThreadID = Thread::CurrentThreadID();
        params.notifier1Called++;
    };
    SC_TEST_EXPECT(params.asyncWakeUp.start(params.eventLoop, params.eventObject));
    Thread newThread1;
    auto   threadLambda = [&params](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("test1"));
        params.wakeUpRes = params.asyncWakeUp.wakeUp(params.eventLoop);
        params.eventObject.wait();
        params.observedNotifier1Called = params.notifier1Called;
    };
    SC_TEST_EXPECT(newThread1.start(threadLambda));
    SC_TEST_EXPECT(params.eventLoop.runOnce());
    SC_TEST_EXPECT(params.notifier1Called == 1);
    SC_TEST_EXPECT(newThread1.join());
    SC_TEST_EXPECT(params.wakeUpRes);
    SC_TEST_EXPECT(params.observedNotifier1Called == 1);
    SC_TEST_EXPECT(params.callbackThreadID == Thread::CurrentThreadID());
}
