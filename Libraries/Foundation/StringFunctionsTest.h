#pragma once
#include "StringFunctions.h"
#include "Test.h"

namespace SC
{
struct StringFunctionsTest;
}

struct SC::StringFunctionsTest : public SC::TestCase
{
    StringFunctionsTest(SC::TestReport& report) : TestCase(report, "StringFunctionsTest")
    {
        using namespace SC;

        if (test_section("view"))
        {
            StringView str = "123_567";
            auto       ops = str.functions<text::StringIteratorASCII>();
            SC_TEST_EXPECT(ops.offsetLength(7, 0) == "");
            SC_TEST_EXPECT(ops.offsetLength(0, 3) == "123");
            SC_TEST_EXPECT(ops.fromTo(0, 3) == "123");
            SC_TEST_EXPECT(ops.offsetLength(4, 3) == "567");
            SC_TEST_EXPECT(ops.fromTo(4, 7) == "567");
        }
    }
};
