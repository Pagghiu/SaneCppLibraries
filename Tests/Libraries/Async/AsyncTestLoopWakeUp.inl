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
        SC_TEST_EXPECT(res.getAsync().stop());
    };
    SC_TEST_EXPECT(wakeUp2.start(eventLoop));
    Thread newThread1;
    Thread newThread2;
    Result loopRes1 = Result(false);
    Result loopRes2 = Result(false);
    auto   action1  = [&](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("test1"));
        loopRes1 = wakeUp1.wakeUp();
    };
    auto action2 = [&](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("test2"));
        loopRes2 = wakeUp1.wakeUp();
    };
    SC_TEST_EXPECT(newThread1.start(action1));
    SC_TEST_EXPECT(newThread2.start(action2));
    SC_TEST_EXPECT(newThread1.join());
    SC_TEST_EXPECT(newThread2.join());
    SC_TEST_EXPECT(loopRes1);
    SC_TEST_EXPECT(loopRes2);
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(context.wakeUp1Called == 1);
    SC_TEST_EXPECT(context.wakeUp2Called == 0);
    SC_TEST_EXPECT(context.wakeUp1ThreadID == Thread::CurrentThreadID());
}

void SC::AsyncTest::loopWakeUpEventObject()
{
    struct TestParams
    {
        int         notifier1Called         = 0;
        int         observedNotifier1Called = -1;
        EventObject eventObject;
        Result      loopRes1 = Result(false);
    } params;

    uint64_t callbackThreadID = 0;

    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    AsyncLoopWakeUp wakeUp;

    wakeUp.callback = [&](AsyncLoopWakeUp::Result&)
    {
        callbackThreadID = Thread::CurrentThreadID();
        params.notifier1Called++;
    };
    SC_TEST_EXPECT(wakeUp.start(eventLoop, &params.eventObject));
    Thread newThread1;
    auto   threadLambda = [&params, &wakeUp](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("test1"));
        params.loopRes1 = wakeUp.wakeUp();
        params.eventObject.wait();
        params.observedNotifier1Called = params.notifier1Called;
    };
    SC_TEST_EXPECT(newThread1.start(threadLambda));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(params.notifier1Called == 1);
    SC_TEST_EXPECT(newThread1.join());
    SC_TEST_EXPECT(params.loopRes1);
    SC_TEST_EXPECT(params.observedNotifier1Called == 1);
    SC_TEST_EXPECT(callbackThreadID == Thread::CurrentThreadID());
}
