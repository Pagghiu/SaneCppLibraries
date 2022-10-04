#pragma once
#include "stringView.h"
#include "test.h"

namespace sanecpp
{
struct StringViewTest;
}

struct sanecpp::StringViewTest : public sanecpp::TestCase
{
    StringViewTest(sanecpp::TestReport& report) : TestCase(report, "StringViewTest")
    {
        using namespace sanecpp;

        if (START_SECTION("construction"))
        {
            stringView s("asd");
            SANECPP_TEST_EXPECT(s.getLengthInBytes() == 3);
            SANECPP_TEST_EXPECT(s.isNullTerminated());
        }

        if (START_SECTION("comparison"))
        {
            stringView other("asd");
            SANECPP_TEST_EXPECT(other == "asd");
            SANECPP_TEST_EXPECT(other != "das");
        }

        if (START_SECTION("parseInt32"))
        {
            stringView other;
            int32_t    value;
            SANECPP_TEST_EXPECT(not other.parseInt32(&value));
            other = "\0";
            SANECPP_TEST_EXPECT(not other.parseInt32(&value));
            other = "+";
            SANECPP_TEST_EXPECT(not other.parseInt32(&value));
            other = "-";
            SANECPP_TEST_EXPECT(not other.parseInt32(&value));
            other = "+ ";
            SANECPP_TEST_EXPECT(not other.parseInt32(&value));
            other = "+1";
            SANECPP_TEST_EXPECT(other.parseInt32(&value));
            SANECPP_TEST_EXPECT(value == 1);
            other = "-123";
            SANECPP_TEST_EXPECT(other.parseInt32(&value));
            SANECPP_TEST_EXPECT(value == -123);
            other = stringView("-456___", 4, false);
            SANECPP_TEST_EXPECT(other.parseInt32(&value));
            SANECPP_TEST_EXPECT(value == -456);
        }
    }
};
