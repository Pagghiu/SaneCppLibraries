// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
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
            auto       ops = str.functions<StringIteratorASCII>();
            SC_TEST_EXPECT(ops.offsetLength(7, 0) == "");
            SC_TEST_EXPECT(ops.offsetLength(0, 3) == "123");
            SC_TEST_EXPECT(ops.fromTo(0, 3) == "123");
            SC_TEST_EXPECT(ops.offsetLength(4, 3) == "567");
            SC_TEST_EXPECT(ops.fromTo(4, 7) == "567");
        }
        if (test_section("split"))
        {
            {
                StringView str   = "_123_567___";
                auto       ops   = str.functions<StringIteratorASCII>();
                int        index = 0;

                auto numSplits = ops.split('_',
                                           [&](StringView v)
                                           {
                                               switch (index)
                                               {
                                               case 0: SC_TEST_EXPECT(v == "123"); break;
                                               case 1: SC_TEST_EXPECT(v == "567"); break;
                                               }
                                               index++;
                                           });
                SC_TEST_EXPECT(index == 2);
                SC_TEST_EXPECT(numSplits == 2);
            }
            {
                StringView str       = "___";
                auto       ops       = str.functions<StringIteratorASCII>();
                auto       numSplits = ops.split('_', [&](StringView v) {}, {SplitOptions::SkipSeparator});
                SC_TEST_EXPECT(numSplits == 3);
            }
            {
                StringView str       = "";
                auto       ops       = str.functions<StringIteratorASCII>();
                auto       numSplits = ops.split('_', [&](StringView v) {}, {SplitOptions::SkipSeparator});
                SC_TEST_EXPECT(numSplits == 0);
            }
        }
    }
};
