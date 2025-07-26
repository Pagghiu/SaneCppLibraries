// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Foundation/Assert.h"
#include "Libraries/Memory/Memory.h"
#include "Libraries/Testing/Limits.h"
#include "Libraries/Testing/Testing.h"

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
            Assert::printBacktrace("a!=b", "FileName.cpp", "Function", 12);
        }
    }
};

namespace SC
{
void runBaseTest(SC::TestReport& report) { BaseTest test(report); }
} // namespace SC
