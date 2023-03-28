// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Test.h"
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
            EventLoop loop;
            SC_TEST_EXPECT(loop.create());
            int   called1 = 0;
            int   called2 = 0;
            Async c1, c2;
            loop.addTimeout(1_ms, c1,
                            [&](AsyncResult& res)
                            {
                                // As we are in a timeout callback we know for fact that we could easily access
                                // what we need with res.async.operation.fields.timeout, playing at the edge of UB.
                                // unionAs<...>() however is more safe, as it will return nullptr when casted to wrong
                                // type.
                                Async::Timeout* timeout = res.async.operation.unionAs<Async::Timeout>();
                                SC_TEST_EXPECT(timeout and timeout->timeout.ms == 1);
                                called1++;
                            });
            loop.addTimeout(100_ms, c2,
                            [&](AsyncResult&)
                            {
                                // TODO: investigate allowing dropping AsyncResult
                                called2++;
                            });
            SC_TEST_EXPECT(loop.runOnce());
            SC_TEST_EXPECT(called1 == 1 and called2 == 0);
            SC_TEST_EXPECT(loop.runOnce());
            SC_TEST_EXPECT(called1 == 1 and called2 == 1);
        }
        if (test_section("wakeUpFromExternalThread"))
        {
            EventLoop loop;
            SC_TEST_EXPECT(loop.create());
            Thread newThread;
            int    threadCalled                      = 0;
            int    wakeUpFromExternalThreadSucceeded = 0;
            SC_TEST_EXPECT(newThread.start("test",
                                           [&]
                                           {
                                               threadCalled++;
                                               if (loop.wakeUpFromExternalThread())
                                               {
                                                   wakeUpFromExternalThreadSucceeded++;
                                               }
                                           }));
            TimeCounter start, end;
            SC_TEST_EXPECT(loop.runOnce());
            SC_TEST_EXPECT(newThread.join());
            SC_TEST_EXPECT(threadCalled == 1);
            SC_TEST_EXPECT(wakeUpFromExternalThreadSucceeded == 1);
        }
        if (test_section("ExternalThreadNotifier"))
        {
            EventLoop loop;
            int       notifier1Called = 0;
            int       notifier2Called = 0;
            SC_TEST_EXPECT(loop.create());
            EventLoop::ExternalThreadNotifier notifier1;
            EventLoop::ExternalThreadNotifier notifier2;

            uint64_t   callbackThreadID = 0;
            const auto notifier1res     = loop.initNotifier(notifier1,
                                                            [&](EventLoop&)
                                                            {
                                                            // TODO: Add thread id check
                                                            callbackThreadID = Thread::CurrentThreadID();
                                                            notifier1Called++;
                                                        });
            SC_TEST_EXPECT(notifier1res);
            const auto notifier2res = loop.initNotifier(notifier2, [&](EventLoop&) { notifier2Called++; });
            SC_TEST_EXPECT(notifier2res);
            Thread     newThread1;
            Thread     newThread2;
            ReturnCode loopRes1   = false;
            ReturnCode loopRes2   = false;
            const auto threadRes1 = newThread1.start("test1",
                                                     [&]
                                                     {
                                                         loopRes1 = loop.notifyFromExternalThread(notifier1);
                                                         // TODO: Add a waitFor / event signal for callback called
                                                     });
            const auto threadRes2 = newThread2.start("test2",
                                                     [&]
                                                     {
                                                         loopRes2 = loop.notifyFromExternalThread(notifier1);
                                                         // TODO: Add a waitFor / event signal for callback called
                                                     });
            SC_TEST_EXPECT(threadRes1);
            SC_TEST_EXPECT(threadRes2);
            SC_TEST_EXPECT(newThread1.join());
            SC_TEST_EXPECT(newThread2.join());
            SC_TEST_EXPECT(loopRes1);
            SC_TEST_EXPECT(loopRes2);
            SC_TEST_EXPECT(loop.runOnce());
            SC_TEST_EXPECT(notifier1Called == 1);
            SC_TEST_EXPECT(notifier2Called == 0);
            SC_TEST_EXPECT(callbackThreadID == Thread::CurrentThreadID());
            loop.removeNotifier(notifier1);
            loop.removeNotifier(notifier2);
            // TODO: Add get thread id and check in the notifier that we have the same thread id
        }
    }
};
