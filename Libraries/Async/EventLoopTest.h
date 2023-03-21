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
        // TODO: Implement async lifecycle
        if (test_section("addTimeout"))
        {
            EventLoop loop;
            SC_TEST_EXPECT(loop.create());
            int   called1 = 0;
            int   called2 = 0;
            Async c1, c2;
            loop.addTimeout(10_ms, c1,
                            [&](AsyncResult& res)
                            {
                                // As we are in a timeout callback we know for fact that we could easily access
                                // what we need with res.async.operation.fields.timeout, playing at the edge of UB.
                                // unionAs<...>() however is more safe, as it will return nullptr when casted to wrong
                                // type.
                                Async::Timeout* timeout = res.async.operation.unionAs<Async::Timeout>();
                                SC_TEST_EXPECT(timeout and timeout->timeout.ms == 10);
                                called1++;
                            });
            loop.addTimeout(100_ms, c2,
                            [&](AsyncResult&) { called2++; }); // TODO: investigate allowing dropping AsyncResult
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
    }
};
