// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"

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
