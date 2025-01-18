// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"

void SC::AsyncTest::loopTimeout()
{
    AsyncLoopTimeout timeout1, timeout2;
    AsyncEventLoop   eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    int timeout1Called = 0;
    int timeout2Called = 0;
    timeout1.callback  = [&](AsyncLoopTimeout::Result& res)
    {
        SC_TEST_EXPECT(res.getAsync().relativeTimeout == 1_ms);
        SC_TEST_EXPECT(res.getAsync().isFree());
        SC_TEST_EXPECT(not res.getAsync().isActive());
        SC_TEST_EXPECT(not res.getAsync().isCancelling());
        timeout1Called++;
    };
    SC_TEST_EXPECT(timeout1.start(eventLoop, Time::Milliseconds(1)));
    timeout2.callback = [&](AsyncLoopTimeout::Result& res)
    {
        if (timeout2Called == 0)
        {
            // Re-activate timeout2, modifying also its relative timeout to 1 ms (see SC_TEST_EXPECT below)
            SC_TEST_EXPECT(res.getAsync().isFree());
            SC_TEST_EXPECT(not res.getAsync().isActive());
            res.reactivateRequest(true);
            SC_TEST_EXPECT(res.getAsync().isActive());
            res.getAsync().relativeTimeout = 1_ms;
        }
        timeout2Called++;
    };
    SC_TEST_EXPECT(timeout2.start(eventLoop, 100_ms));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(timeout1Called == 1 and timeout2Called == 0); // timeout1 fires after 1 ms
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(timeout1Called == 1 and timeout2Called == 1); // timeout2 fires after 100 ms
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(timeout1Called == 1 and timeout2Called == 2); // Re-activated timeout2 fires again after 1 ms
}
