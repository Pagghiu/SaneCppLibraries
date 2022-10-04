#pragma once
#include "limits.h"
#include "os.h"
#include "test.h"

namespace sanecpp
{
struct OsTest;
}

struct sanecpp::OsTest : public sanecpp::TestCase
{
    OsTest(sanecpp::TestReport& report) : TestCase(report, "OsTest")
    {
        using namespace sanecpp;

        if (START_SECTION("printBacktrace"))
        {
            SANECPP_TEST_EXPECT(os::printBacktrace());
            size_t frames = os::printBacktrace(0, -1);
            SANECPP_TEST_EXPECT(frames == 0);
        }
        if (START_SECTION("captureBacktrace"))
        {
            void*    traceBuffer[10];
            uint32_t hash   = 0;
            size_t   frames = os::captureBacktrace(2, traceBuffer, sizeof(traceBuffer), &hash);
            SANECPP_TEST_EXPECT(hash != 0);
            SANECPP_TEST_EXPECT(frames != 0);
            constexpr auto maxVal = static_cast<size_t>(static_cast<int>(MaxValue())) + 1;
            frames                = os::captureBacktrace(2, nullptr, maxVal * sizeof(void*), &hash);
            SANECPP_TEST_EXPECT(frames == 0);
        }
    }
};
