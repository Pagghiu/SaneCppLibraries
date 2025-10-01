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

        if (test_section("Limits (coverage)"))
        {
            uint8_t  maxU8  = MaxValue();
            uint16_t maxU16 = MaxValue();
            uint32_t maxU32 = MaxValue();
            uint64_t maxU64 = MaxValue();
            int8_t   maxI8  = MaxValue();
            int16_t  maxI16 = MaxValue();
            int32_t  maxI32 = MaxValue();
            int64_t  maxI64 = MaxValue();
            ssize_t  maxSS  = MaxValue();
            size_t   maxS   = MaxValue();
            float    maxF   = MaxValue();
            double   maxD   = MaxValue();

            SC_COMPILER_UNUSED(maxU8);
            SC_COMPILER_UNUSED(maxU16);
            SC_COMPILER_UNUSED(maxU32);
            SC_COMPILER_UNUSED(maxU64);
            SC_COMPILER_UNUSED(maxI8);
            SC_COMPILER_UNUSED(maxI16);
            SC_COMPILER_UNUSED(maxI32);
            SC_COMPILER_UNUSED(maxI64);
            SC_COMPILER_UNUSED(maxSS);
            SC_COMPILER_UNUSED(maxS);
            SC_COMPILER_UNUSED(maxF);
            SC_COMPILER_UNUSED(maxD);
        }
    }
};

namespace SC
{
void runBaseTest(SC::TestReport& report) { BaseTest test(report); }
} // namespace SC
