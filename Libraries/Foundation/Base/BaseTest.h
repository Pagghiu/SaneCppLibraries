// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../Testing/Test.h"
#include "../Base/Assert.h"
#include "../Base/Memory.h"

namespace SC
{
struct BaseTest;
}

struct SC::BaseTest : public SC::TestCase
{
    BaseTest(SC::TestReport& report) : TestCase(report, "BaseTest")
    {
        if (test_section("new/delete"))
        {
            int* a = new int(2);
            SC_TEST_EXPECT(a[0] == 2);
            delete a;
            int* b = new int[2];
            delete[] b;
        }
        if (test_section("Assert::printBacktrace"))
        {
            SC_TEST_EXPECT(Assert::printBacktrace());
            size_t frames = Assert::printBacktrace(0, static_cast<size_t>(MaxValue()));
            SC_TEST_EXPECT(frames == 0);
        }
        if (test_section("Assert::captureBacktrace"))
        {
            void*    traceBuffer[10];
            uint32_t hash   = 0;
            size_t   frames = Assert::captureBacktrace(2, traceBuffer, sizeof(traceBuffer), &hash);
            SC_TEST_EXPECT(hash != 0);
            SC_TEST_EXPECT(frames != 0);
            constexpr auto maxVal = static_cast<size_t>(static_cast<int>(MaxValue())) + 1;
            frames                = Assert::captureBacktrace(2, nullptr, maxVal * sizeof(void*), &hash);
            SC_TEST_EXPECT(frames == 0);
        }
        
    }
};
