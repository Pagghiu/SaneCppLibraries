#pragma once
#include "memory.h"
#include "test.h"

namespace sanecpp
{
struct MemoryTest;
}

struct sanecpp::MemoryTest : public sanecpp::TestCase
{
    MemoryTest(sanecpp::TestReport& report) : TestCase(report, "MemoryTest")
    {
        using namespace sanecpp;

        if (START_SECTION("operators"))
        {
            int* a = new int(2);
            SANECPP_TEST_EXPECT(a[0] == 2);
            delete a;
            int* b = new int[2];
            delete[] b;
        }
    }
};
