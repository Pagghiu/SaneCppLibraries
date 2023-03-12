// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Test.h"
#include "Loop.h"

namespace SC
{
struct LoopTest;
}

struct SC::LoopTest : public SC::TestCase
{
    LoopTest(SC::TestReport& report) : TestCase(report, "LoopTest")
    {
        using namespace SC;
        if (test_section("addTimer"))
        {
            Loop loop;
            SC_TEST_EXPECT(loop.create());
            int called1 = 0;
            int called2 = 0;
            SC_TEST_EXPECT(loop.addTimer(10_ms, [&]() { called1++; }));
            SC_TEST_EXPECT(loop.addTimer(100_ms, [&]() { called2++; }));
            SC_TEST_EXPECT(loop.runOnce());
            SC_TEST_EXPECT(called1 == 1 and called2 == 0);
            SC_TEST_EXPECT(loop.runOnce());
            SC_TEST_EXPECT(called1 == 1 and called2 == 1);
        }
        if (test_section("wakeUpFromExternalThread"))
        {
            Loop loop;
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
