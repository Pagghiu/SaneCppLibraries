// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "String.h"
#include "Test.h"

namespace SC
{
struct StringTest;
}

struct SC::StringTest : public SC::TestCase
{
    StringTest(SC::TestReport& report) : TestCase(report, "StringTest")
    {
        using namespace SC;

        if (test_section("construction_comparison"))
        {
            StringView sv  = "Test String";
            String     str = String("Test String");
            SC_TEST_EXPECT(str == sv);
            // SC_TEST_EXPECT(sv == str);
            SC_TEST_EXPECT(str != "ASD");
            SC_TEST_EXPECT(str == "Test String");
            SC_TEST_EXPECT(str == str);
            SC_TEST_EXPECT(str != String("ASD"));
            // SC_TEST_EXPECT(str < "Zest Ztring");
        }
        if (test_section("compareASCII"))
        {
            StringView sv[3] = {
                StringView("3"),
                StringView("1"),
                StringView("2"),
            };
            bubbleSort(sv, sv + 3, [](StringView a, StringView b) { return a < b; });
            SC_TEST_EXPECT(sv[0] == "1");
            SC_TEST_EXPECT(sv[1] == "2");
            SC_TEST_EXPECT(sv[2] == "3");
            bubbleSort(sv, sv + 3, [](StringView a, StringView b) { return a.compareASCII(b) == Comparison::Bigger; });
            SC_TEST_EXPECT(sv[0] == "3");
            SC_TEST_EXPECT(sv[1] == "2");
            SC_TEST_EXPECT(sv[2] == "1");
        }
    }
};
