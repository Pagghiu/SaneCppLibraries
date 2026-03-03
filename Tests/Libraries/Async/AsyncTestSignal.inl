// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"

#include <signal.h>
#if SC_PLATFORM_APPLE
#include <sys/sysctl.h> // sysctl / kinfo_proc / P_TRACED
#endif
#if not SC_PLATFORM_WINDOWS
#include <unistd.h> // getpid / kill
#endif

#if SC_PLATFORM_APPLE
namespace
{
static bool isDebuggerAttached()
{
    int               mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(::getpid())};
    struct kinfo_proc info   = {};
    size_t            size   = sizeof(info);
    if (::sysctl(mib, 4, &info, &size, nullptr, 0) != 0)
    {
        return false;
    }
    return (info.kp_proc.p_flag & P_TRACED) != 0;
}
} // namespace
#endif

void SC::AsyncTest::signal()
{
#if SC_PLATFORM_APPLE
    if (isDebuggerAttached())
    {
        report.console.printLine("AsyncTest - Skipping signal section while debugger is attached");
        return;
    }
#endif

    //-------------------------------------------------------------------------------------------------------
    // Test 1: Persistent single watcher - verifies signal delivery and deliveryCount
    //-------------------------------------------------------------------------------------------------------
    {
#if SC_PLATFORM_WINDOWS
        // On Windows, verify that AsyncSignal setup/teardown lifecycle works correctly.
        // Signal delivery via GenerateConsoleCtrlEvent requires a real console window,
        // so we test the registration mechanism directly.
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create(options));

            int callbackCount = 0;

            AsyncSignal caseSignal;
            caseSignal.setDebugName("signal-sigint");
            caseSignal.callback = [&](AsyncSignal::Result& res)
            {
                callbackCount += 1;
                SC_TEST_EXPECT(res.completionData.signalNumber == 2);
                SC_TEST_EXPECT(res.completionData.deliveryCount >= 1);
            };

            // Verify start succeeds for valid Windows signal numbers
            SC_TEST_EXPECT(caseSignal.start(eventLoop, 2)); // SIGINT -> CTRL_C_EVENT

            // Run one iteration to let setup complete, then stop
            SC_TEST_EXPECT(eventLoop.runNoWait());

            // Stop the signal watcher (exercises teardown)
            SC_TEST_EXPECT(caseSignal.stop(eventLoop));
            SC_TEST_EXPECT(eventLoop.runNoWait());
        }

        // Test multiple signal watchers can be registered (fanout)
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create(options));

            AsyncSignal signal1;
            signal1.setDebugName("fanout-1");
            signal1.callback = [](AsyncSignal::Result&) {};

            AsyncSignal signal2;
            signal2.setDebugName("fanout-2");
            signal2.callback = [](AsyncSignal::Result&) {};

            // Both can register for the same signal
            SC_TEST_EXPECT(signal1.start(eventLoop, 2));
            SC_TEST_EXPECT(signal2.start(eventLoop, 2));
            SC_TEST_EXPECT(eventLoop.runNoWait());

            // Both can be stopped
            SC_TEST_EXPECT(signal1.stop(eventLoop));
            SC_TEST_EXPECT(signal2.stop(eventLoop));
            SC_TEST_EXPECT(eventLoop.runNoWait());
        }
#else
        auto runSignalCase = [this](int testSignal, const char* debugName)
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create(options));

            struct SignalContext
            {
                bool sentSignal    = false;
                int  callbackCount = 0;
                int  callbackValue = 0;
                int  signalNumber  = 0;
            };
            SignalContext context;
            context.signalNumber = testSignal;

            AsyncSignal caseSignal;
            caseSignal.setDebugName(debugName);
            caseSignal.callback = [ctx = &context](AsyncSignal::Result& res)
            {
                ctx->callbackCount += 1;
                ctx->callbackValue = res.completionData.signalNumber;
                SC_ASSERT_RELEASE(res.completionData.deliveryCount >= 1);
            };

            AsyncLoopTimeout sendSignal;
            sendSignal.setDebugName("signal-send");
            sendSignal.callback = [this, ctx = &context](AsyncLoopTimeout::Result&)
            {
                ctx->sentSignal = true;
                SC_TEST_EXPECT(::kill(::getpid(), ctx->signalNumber) == 0);
            };

            AsyncLoopTimeout timeout;
            timeout.setDebugName("signal-timeout");
            timeout.callback = [this](AsyncLoopTimeout::Result&)
            { SC_TEST_EXPECT("AsyncTest::signal timed out waiting for signal callback" and false); };

            SC_TEST_EXPECT(caseSignal.start(eventLoop, testSignal));
            SC_TEST_EXPECT(sendSignal.start(eventLoop, TimeMs{10}));
            SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{2000}));
            eventLoop.excludeFromActiveCount(timeout);

            SC_TEST_EXPECT(eventLoop.run());
            SC_TEST_EXPECT(context.sentSignal);
            SC_TEST_EXPECT(context.callbackCount == 1);
            SC_TEST_EXPECT(context.callbackValue == testSignal);
        };

#if defined(SIGINT)
        runSignalCase(SIGINT, "signal-sigint");
#endif

#if defined(SIGTERM)
        runSignalCase(SIGTERM, "signal-sigterm");
#endif

#if defined(SIGHUP)
        runSignalCase(SIGHUP, "signal-sighup");
#endif
#endif
    }

    //-------------------------------------------------------------------------------------------------------
    // Test 2: Multi-watcher fanout - two watchers on same signal on same loop
    //
    // NOTE: Same-loop fanout is currently only supported on Windows.
    // On POSIX systems, only the first watcher registered for a specific signal on a given loop will receive
    // the signal. Subsequent registrations for the same signal on the same loop will fail.
    //-------------------------------------------------------------------------------------------------------

    //-------------------------------------------------------------------------------------------------------
    // Test 2: Invalid signal rejection
    //-------------------------------------------------------------------------------------------------------
    {
        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create(options));

        AsyncSignal invalidSignal;
        invalidSignal.callback = [](AsyncSignal::Result&) {};

        // Signal number 0 is always invalid
        SC_TEST_EXPECT(not invalidSignal.start(eventLoop, 0));

        // Negative signal numbers are invalid
        SC_TEST_EXPECT(not invalidSignal.start(eventLoop, -1));

#if SC_PLATFORM_WINDOWS
        // Unsupported signal numbers on Windows
        SC_TEST_EXPECT(not invalidSignal.start(eventLoop, 9));  // SIGKILL equivalent not mappable
        SC_TEST_EXPECT(not invalidSignal.start(eventLoop, 17)); // Not a console signal
#endif
    }
}
