// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/Assert.h"
#include "../../Foundation/HeapBuffer.h"
#include "../../Foundation/Memory.h"
#include "../../Testing/Testing.h"

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
        if (test_section("Assert::print"))
        {
            Assert::print("a!=b", "FileName.cpp", "Function", 12);
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
        if (test_section("HeapBuffer"))
        {
            HeapBuffer buffer;
            SC_TEST_EXPECT(buffer.allocate(16));
            SC_TEST_EXPECT(buffer.data.sizeInBytes() == 16);
            for (size_t i = 0; i < 16; ++i)
            {
                buffer.data[i] = static_cast<char>(i);
            }
            SC_TEST_EXPECT(buffer.reallocate(32));
            SC_TEST_EXPECT(buffer.data.sizeInBytes() == 32);
            bool asExpected = true;
            for (size_t i = 0; i < 16; ++i)
            {
                asExpected = asExpected and (buffer.data[i] == static_cast<char>(i));
            }
            SC_TEST_EXPECT(asExpected);
        }
    }
};

namespace SC
{
void runBaseTest(SC::TestReport& report) { BaseTest test(report); }
} // namespace SC
