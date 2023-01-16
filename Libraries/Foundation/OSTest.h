// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Limits.h"
#include "OS.h"
#include "Test.h"

namespace SC
{
struct OSTest;
}

struct SC::OSTest : public SC::TestCase
{
    OSTest(SC::TestReport& report) : TestCase(report, "OSTest")
    {
        using namespace SC;

        if (test_section("printBacktrace"))
        {
            SC_TEST_EXPECT(OS::printBacktrace());
            size_t frames = OS::printBacktrace(0, -1);
            SC_TEST_EXPECT(frames == 0);
        }
        if (test_section("captureBacktrace"))
        {
            void*    traceBuffer[10];
            uint32_t hash   = 0;
            size_t   frames = OS::captureBacktrace(2, traceBuffer, sizeof(traceBuffer), &hash);
            SC_TEST_EXPECT(hash != 0);
            SC_TEST_EXPECT(frames != 0);
            constexpr auto maxVal = static_cast<size_t>(static_cast<int>(MaxValue())) + 1;
            frames                = OS::captureBacktrace(2, nullptr, maxVal * sizeof(void*), &hash);
            SC_TEST_EXPECT(frames == 0);
        }
    }
};
