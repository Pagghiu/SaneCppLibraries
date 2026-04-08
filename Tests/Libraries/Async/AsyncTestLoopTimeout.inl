// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"
#include "Libraries/Time/Time.h"

void SC::AsyncTest::loopTimeout()
{
#if SC_PLATFORM_WINDOWS
    static const int64_t shortTimeoutMs = 10;
    static const int64_t longTimeoutMs  = 500;
#else
    static const int64_t shortTimeoutMs = 1;
    static const int64_t longTimeoutMs  = 100;
#endif

    AsyncLoopTimeout timeout1, timeout2;
    AsyncEventLoop   eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    struct Context
    {
        int timeout1Called      = 0;
        int timeout2Called      = 0;
        int callbackOrder[3]    = {0};
        int callbackCount       = 0;
        int beforePollIOCounter = 0;
        int afterPollIOCounter  = 0;
    } context;
    timeout1.callback = [this, ctx = &context](AsyncLoopTimeout::Result& res)
    {
        SC_TEST_EXPECT(res.getAsync().relativeTimeout.milliseconds == shortTimeoutMs);
        SC_TEST_EXPECT(res.getAsync().isFree());
        SC_TEST_EXPECT(not res.getAsync().isActive());
        SC_TEST_EXPECT(not res.getAsync().isCancelling());
        SC_TEST_EXPECT(ctx->callbackCount <
                       static_cast<int>(sizeof(ctx->callbackOrder) / sizeof(ctx->callbackOrder[0])));
        ctx->callbackOrder[ctx->callbackCount++] = 1;
        ctx->timeout1Called++;
    };
    timeout2.callback = [this, ctx = &context](AsyncLoopTimeout::Result& res)
    {
        SC_TEST_EXPECT(ctx->callbackCount <
                       static_cast<int>(sizeof(ctx->callbackOrder) / sizeof(ctx->callbackOrder[0])));
        ctx->callbackOrder[ctx->callbackCount++] = 2;
        if (ctx->timeout2Called == 0)
        {
            // Re-activate timeout2, modifying also its relative timeout to shortTimeout (see SC_TEST_EXPECT below)
            SC_TEST_EXPECT(res.getAsync().isFree());
            SC_TEST_EXPECT(not res.getAsync().isActive());
            res.reactivateRequest(true);
            SC_TEST_EXPECT(res.getAsync().isActive());
            res.getAsync().relativeTimeout = TimeMs{shortTimeoutMs};
        }
        ctx->timeout2Called++;
    };
    SC_TEST_EXPECT(timeout2.start(eventLoop, TimeMs{longTimeoutMs}));
    SC_TEST_EXPECT(timeout1.start(eventLoop, TimeMs{shortTimeoutMs}));

    AsyncLoopTimeout* earliestTimeout = eventLoop.findEarliestLoopTimeout();
    SC_TEST_EXPECT(earliestTimeout == &timeout1);
    AsyncEventLoopListeners listeners;
    listeners.beforeBlockingPoll = [this, ctx = &context](AsyncEventLoop&)
    {
        ctx->beforePollIOCounter++;
        SC_TEST_EXPECT(ctx->afterPollIOCounter == 0);
        SC_TEST_EXPECT(ctx->timeout1Called == 0);
        SC_TEST_EXPECT(ctx->timeout2Called == 0);
    };
    listeners.afterBlockingPoll = [this, ctx = &context](AsyncEventLoop&)
    {
        ctx->afterPollIOCounter++;
        SC_TEST_EXPECT(ctx->beforePollIOCounter == 1);
        SC_TEST_EXPECT(ctx->timeout1Called == 0);
        SC_TEST_EXPECT(ctx->timeout2Called == 0);
    };
    eventLoop.setListeners(&listeners);
    SC_TEST_EXPECT(eventLoop.runOnce());
    eventLoop.setListeners(nullptr);
    SC_TEST_EXPECT(context.beforePollIOCounter == 1);
    SC_TEST_EXPECT(context.afterPollIOCounter == 1);
    SC_TEST_EXPECT(context.timeout1Called == 1);
    SC_TEST_EXPECT(context.timeout2Called <= 1);
    SC_TEST_EXPECT(context.callbackCount >= 1 and context.callbackOrder[0] == 1);

    int runOnceAttempts = 0;
    while (context.timeout2Called < 2)
    {
        SC_TEST_EXPECT(runOnceAttempts < 3);
        SC_TEST_EXPECT(eventLoop.runOnce());
        runOnceAttempts++;
    }

    SC_TEST_EXPECT(context.timeout1Called == 1 and context.timeout2Called == 2);
    SC_TEST_EXPECT(context.callbackCount == 3);
    SC_TEST_EXPECT(context.callbackOrder[0] == 1);
    SC_TEST_EXPECT(context.callbackOrder[1] == 2);
    SC_TEST_EXPECT(context.callbackOrder[2] == 2);
}
