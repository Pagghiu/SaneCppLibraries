// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Memory.h"
#include "Test.h"

namespace SC
{
struct MemoryTest;
}

struct SC::MemoryTest : public SC::TestCase
{
    MemoryTest(SC::TestReport& report) : TestCase(report, "MemoryTest")
    {
        using namespace SC;

        if (test_section("operators"))
        {
            int* a = new int(2);
            SC_TEST_EXPECT(a[0] == 2);
            delete a;
            int* b = new int[2];
            delete[] b;
        }
    }
};
