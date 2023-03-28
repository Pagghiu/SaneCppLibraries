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
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            int   called1 = 0;
            int   called2 = 0;
            Async c1, c2;
            auto  timeout1Callback = [&](AsyncResult& res)
            {
                // As we are in a timeout callback we know for fact that we could easily access
                // what we need with res.async.operation.fields.timeout, playing at the edge of UB.
                // unionAs<...>() however is more safe, as it will return nullptr when casted to wrong
                // type.
                Async::Timeout* timeout = res.async.operation.unionAs<Async::Timeout>();
                SC_TEST_EXPECT(timeout and timeout->timeout.ms == 1);
                called1++;
            };
            SC_TEST_EXPECT(eventLoop.addTimeout(1_ms, c1, move(timeout1Callback)));
            auto timeout2Callback = [&](AsyncResult&)
            {
                // TODO: investigate allowing dropping AsyncResult
                called2++;
            };
            SC_TEST_EXPECT(eventLoop.addTimeout(100_ms, c2, move(timeout2Callback)));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(called1 == 1 and called2 == 0);
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(called1 == 1 and called2 == 1);
        }
        if (test_section("wakeUpFromExternalThread"))
        {
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            Thread newThread;
            int    threadCalled                      = 0;
            int    wakeUpFromExternalThreadSucceeded = 0;

            auto externalThreadLambda = [&]
            {
                threadCalled++;
                if (eventLoop.wakeUpFromExternalThread())
                {
                    wakeUpFromExternalThreadSucceeded++;
                }
            };
            SC_TEST_EXPECT(newThread.start("test", move(externalThreadLambda)));
            TimeCounter start, end;
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(newThread.join());
            SC_TEST_EXPECT(threadCalled == 1);
            SC_TEST_EXPECT(wakeUpFromExternalThreadSucceeded == 1);
        }
        if (test_section("ExternalThreadNotifier"))
        {
            int       notifier1Called  = 0;
            int       notifier2Called  = 0;
            uint64_t  callbackThreadID = 0;
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            EventLoop::ExternalThreadNotifier notifier1;
            EventLoop::ExternalThreadNotifier notifier2;

            auto notifier1Callback = [&](EventLoop&)
            {
                callbackThreadID = Thread::CurrentThreadID();
                notifier1Called++;
            };
            SC_TEST_EXPECT(eventLoop.initNotifier(notifier1, move(notifier1Callback)));
            SC_TEST_EXPECT(eventLoop.initNotifier(notifier2, [&](EventLoop&) { notifier2Called++; }));
            Thread     newThread1;
            Thread     newThread2;
            ReturnCode loopRes1    = false;
            ReturnCode loopRes2    = false;
            auto       thread1Func = [&] { loopRes1 = eventLoop.notifyFromExternalThread(notifier1); };
            auto       thread2Func = [&] { loopRes2 = eventLoop.notifyFromExternalThread(notifier1); };
            SC_TEST_EXPECT(newThread1.start("test1", move(thread1Func)));
            SC_TEST_EXPECT(newThread2.start("test2", move(thread2Func)));
            SC_TEST_EXPECT(newThread1.join());
            SC_TEST_EXPECT(newThread2.join());
            SC_TEST_EXPECT(loopRes1);
            SC_TEST_EXPECT(loopRes2);
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(notifier1Called == 1);
            SC_TEST_EXPECT(notifier2Called == 0);
            SC_TEST_EXPECT(callbackThreadID == Thread::CurrentThreadID());
            eventLoop.removeNotifier(notifier1);
            eventLoop.removeNotifier(notifier2);
        }
        if (test_section("ExternalThreadNotifier with sync eventObject"))
        {
            struct Params
            {
                int         notifier1Called         = 0;
                int         observedNotifier1Called = -1;
                EventObject eventObject;
            } params;

            uint64_t callbackThreadID = 0;

            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            EventLoop::ExternalThreadNotifier notifier1;

            auto eventLoopLambda = [&](EventLoop&)
            {
                callbackThreadID = Thread::CurrentThreadID();
                params.notifier1Called++;
            };
            SC_TEST_EXPECT(eventLoop.initNotifier(notifier1, move(eventLoopLambda), &params.eventObject));
            Thread     newThread1;
            ReturnCode loopRes1     = false;
            auto       threadLambda = [&]
            {
                loopRes1 = eventLoop.notifyFromExternalThread(notifier1);
                params.eventObject.wait();
                params.observedNotifier1Called = params.notifier1Called;
            };
            SC_TEST_EXPECT(newThread1.start("test1", move(threadLambda)));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(loopRes1);
            SC_TEST_EXPECT(params.notifier1Called == 1);
            SC_TEST_EXPECT(newThread1.join());
            SC_TEST_EXPECT(params.observedNotifier1Called == 1);
            SC_TEST_EXPECT(callbackThreadID == Thread::CurrentThreadID());
            eventLoop.removeNotifier(notifier1);
        }
    }
};
