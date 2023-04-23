// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Testing/Test.h"
#include "StringView.h"

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
            SC_TEST_EXPECT(s.sizeInBytes() == 3);
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
            other = StringView("-456___", 4, false, StringEncoding::Ascii);
            SC_TEST_EXPECT(other.parseInt32(&value));
            SC_TEST_EXPECT(value == -456);
        }

        if (test_section("startsWith/endsWith"))
        {
            StringView test("Ciao_123");
            SC_TEST_EXPECT(test.startsWith('C'));
            SC_TEST_EXPECT(test.endsWith('3'));
            SC_TEST_EXPECT(test.startsWith("Ciao"));
            SC_TEST_EXPECT(test.endsWith("123"));
            SC_TEST_EXPECT(not test.startsWith('D'));
            SC_TEST_EXPECT(not test.endsWith('4'));
            SC_TEST_EXPECT(not test.startsWith("Cia_"));
            SC_TEST_EXPECT(not test.endsWith("1_3"));

            StringView test2;
            SC_TEST_EXPECT(not test2.startsWith('a'));
            SC_TEST_EXPECT(not test2.endsWith('a'));
            SC_TEST_EXPECT(test2.startsWith(""));
            SC_TEST_EXPECT(not test2.startsWith("A"));
            SC_TEST_EXPECT(test2.endsWith(""));
            SC_TEST_EXPECT(not test2.endsWith("A"));
        }

        if (test_section("view"))
        {
            StringView str = "123_567";
            SC_TEST_EXPECT(str.sliceStartLength(7, 0) == "");
            SC_TEST_EXPECT(str.sliceStartLength(0, 3) == "123");
            SC_TEST_EXPECT(str.sliceStartEnd<StringIteratorASCII>(0, 3) == "123");
            SC_TEST_EXPECT(str.sliceStartLength(4, 3) == "567");
            SC_TEST_EXPECT(str.sliceStartEnd<StringIteratorASCII>(4, 7) == "567");
        }
        if (test_section("split"))
        {
            {
                StringView str   = "_123_567___";
                int        index = 0;

                auto numSplits = str.splitASCII('_',
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
                auto       numSplits = str.splitASCII('_', [&](StringView) {}, {SplitOptions::SkipSeparator});
                SC_TEST_EXPECT(numSplits == 3);
            }
            {
                StringView str       = "";
                auto       numSplits = str.splitASCII('_', [&](StringView) {}, {SplitOptions::SkipSeparator});
                SC_TEST_EXPECT(numSplits == 0);
            }
        }
    }
};
