// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Atomic.h"
#include "Test.h"

namespace SC
{
struct AtomicTest;
}

struct SC::AtomicTest : public SC::TestCase
{
    AtomicTest(SC::TestReport& report) : TestCase(report, "AtomicTest")
    {
        using namespace SC;

        if (test_section("atomic<bool>"))
        {
            Atomic<bool> test = true;

            SC_TEST_EXPECT(test.load());
            test.exchange(false);
            SC_TEST_EXPECT(not test.load());
        }
        if (test_section("atomic<int32>"))
        {
            Atomic<int32_t> test = 10;

            SC_TEST_EXPECT(test.load(memory_order_relaxed) == 10);
            SC_TEST_EXPECT(test.fetch_add(1) == 10);
            SC_TEST_EXPECT(test.load() == 11);
        }
    }
};
