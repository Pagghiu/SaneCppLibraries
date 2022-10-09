#pragma once
#include "StringView.h"
#include "Test.h"

namespace SC
{
struct StringViewTest;
}

struct SC::StringViewTest : public SC::TestCase
{
    StringViewTest(SC::TestReport& report) : TestCase(report, "StringViewTest")
    {
        using namespace SC;

        if (test_section("construction"))
        {
            StringView s("asd");
            SC_TEST_EXPECT(s.getLengthInBytes() == 3);
            SC_TEST_EXPECT(s.isNullTerminated());
        }

        if (test_section("comparison"))
        {
            StringView other("asd");
            SC_TEST_EXPECT(other == "asd");
            SC_TEST_EXPECT(other != "das");
        }

        if (test_section("parseInt32"))
        {
            StringView other;
            int32_t    value;
            SC_TEST_EXPECT(not other.parseInt32(&value));
            other = "\0";
            SC_TEST_EXPECT(not other.parseInt32(&value));
            other = "+";
            SC_TEST_EXPECT(not other.parseInt32(&value));
            other = "-";
            SC_TEST_EXPECT(not other.parseInt32(&value));
            other = "+ ";
            SC_TEST_EXPECT(not other.parseInt32(&value));
            other = "+1";
            SC_TEST_EXPECT(other.parseInt32(&value));
            SC_TEST_EXPECT(value == 1);
            other = "-123";
            SC_TEST_EXPECT(other.parseInt32(&value));
            SC_TEST_EXPECT(value == -123);
            other = StringView("-456___", 4, false);
            SC_TEST_EXPECT(other.parseInt32(&value));
            SC_TEST_EXPECT(value == -456);
        }
    }
};
