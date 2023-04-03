// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Test.h"
#include "../Threading/Threading.h" // EventObject
#include "EventLoop.h"

namespace SC
{
struct EventLoopTest;
}

struct SC::EventLoopTest : public SC::TestCase
{
    EventLoopTest(SC::TestReport& report) : TestCase(report, "EventLoopTest")
    {
        using namespace SC;
        // TODO: Add EventLoop::resetTimeout
        if (test_section("addTimeout"))
        {
            AsyncTimeout timeout1, timeout2;
            EventLoop    eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            int  timeout1Called   = 0;
            int  timeout2Called   = 0;
            auto timeout1Callback = [&](AsyncResult& res)
            {
                // As we are in a timeout callback we know for fact that we could easily access
                // what we need with res.async.operation.fields.timeout, playing at the edge of UB.
                // unionAs<...>() however is more safe, as it will return nullptr when casted to wrong
                // type.
                Async::Timeout* timeout = res.async.operation.unionAs<Async::Timeout>();
                SC_TEST_EXPECT(timeout and timeout->timeout.ms == 1);
                timeout1Called++;
            };
            SC_TEST_EXPECT(eventLoop.addTimeout(timeout1, 1_ms, move(timeout1Callback)));
            auto timeout2Callback = [&](AsyncResult&)
            {
                // TODO: investigate allowing dropping AsyncResult
                timeout2Called++;
            };
            SC_TEST_EXPECT(eventLoop.addTimeout(timeout2, 100_ms, move(timeout2Callback)));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(timeout1Called == 1 and timeout2Called == 0);
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(timeout1Called == 1 and timeout2Called == 1);
        }
        if (test_section("wakeUpFromExternalThread"))
        {
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            Thread newThread;
            int    threadWasCalled = 0;
            int    wakeUpSucceded  = 0;

            Action externalThreadLambda = [&]
            {
                threadWasCalled++;
                if (eventLoop.wakeUpFromExternalThread())
                {
                    wakeUpSucceded++;
                }
            };
            SC_TEST_EXPECT(newThread.start("test", &externalThreadLambda));
            TimeCounter start, end;
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(newThread.join());
            SC_TEST_EXPECT(threadWasCalled == 1);
            SC_TEST_EXPECT(wakeUpSucceded == 1);
        }
        if (test_section("AsyncWakeUp::wakeUp"))
        {
            int       wakeUp1Called   = 0;
            int       wakeUp2Called   = 0;
            uint64_t  wakeUp1ThreadID = 0;
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            AsyncWakeUp wakeUp1;
            AsyncWakeUp wakeUp2;

            auto lambda1 = [&](EventLoop&)
            {
                wakeUp1ThreadID = Thread::CurrentThreadID();
                wakeUp1Called++;
            };
            SC_TEST_EXPECT(eventLoop.addWakeUp(wakeUp1, lambda1));
            SC_TEST_EXPECT(eventLoop.addWakeUp(wakeUp2, [&](EventLoop&) { wakeUp2Called++; }));
            Thread     newThread1;
            Thread     newThread2;
            ReturnCode loopRes1 = false;
            ReturnCode loopRes2 = false;
            Action     action1  = [&] { loopRes1 = wakeUp1.wakeUp(); };
            Action     action2  = [&] { loopRes2 = wakeUp1.wakeUp(); };
            SC_TEST_EXPECT(newThread1.start("test1", &action1));
            SC_TEST_EXPECT(newThread2.start("test2", &action2));
            SC_TEST_EXPECT(newThread1.join());
            SC_TEST_EXPECT(newThread2.join());
            SC_TEST_EXPECT(loopRes1);
            SC_TEST_EXPECT(loopRes2);
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(wakeUp1Called == 1);
            SC_TEST_EXPECT(wakeUp2Called == 0);
            SC_TEST_EXPECT(wakeUp1ThreadID == Thread::CurrentThreadID());
        }
        if (test_section("AsyncWakeUp::wakeUp with sync eventObject"))
        {
            struct TestParams
            {
                int         notifier1Called         = 0;
                int         observedNotifier1Called = -1;
                EventObject eventObject;
            } params;

            uint64_t callbackThreadID = 0;

            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            AsyncWakeUp wakeUp;

            auto eventLoopLambda = [&](EventLoop&)
            {
                callbackThreadID = Thread::CurrentThreadID();
                params.notifier1Called++;
            };
            SC_TEST_EXPECT(eventLoop.addWakeUp(wakeUp, eventLoopLambda, &params.eventObject));
            Thread     newThread1;
            ReturnCode loopRes1     = false;
            Action     threadLambda = [&]
            {
                loopRes1 = wakeUp.wakeUp();
                params.eventObject.wait();
                params.observedNotifier1Called = params.notifier1Called;
            };
            SC_TEST_EXPECT(newThread1.start("test1", &threadLambda));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(params.notifier1Called == 1);
            SC_TEST_EXPECT(newThread1.join());
            SC_TEST_EXPECT(loopRes1);
            SC_TEST_EXPECT(params.observedNotifier1Called == 1);
            SC_TEST_EXPECT(callbackThreadID == Thread::CurrentThreadID());
        }
    }
};
