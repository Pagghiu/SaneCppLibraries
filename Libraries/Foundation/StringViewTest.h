// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Testing/Test.h"
#include "StringView.h"
#include "StringViewAlgorithms.h"

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
            SC_TEST_EXPECT(not other.parseInt32(value));
            other = "\0";
            SC_TEST_EXPECT(not other.parseInt32(value));
            other = "+";
            SC_TEST_EXPECT(not other.parseInt32(value));
            other = "-";
            SC_TEST_EXPECT(not other.parseInt32(value));
            other = "+ ";
            SC_TEST_EXPECT(not other.parseInt32(value));
            other = "+1";
            SC_TEST_EXPECT(other.parseInt32(value));
            SC_TEST_EXPECT(value == 1);
            other = "-123";
            SC_TEST_EXPECT(other.parseInt32(value));
            SC_TEST_EXPECT(value == -123);
            other = StringView("-456___", 4, false, StringEncoding::Ascii);
            SC_TEST_EXPECT(other.parseInt32(value));
            SC_TEST_EXPECT(value == -456);
            SC_TEST_EXPECT(StringView("0").parseInt32(value) and value == 0);
            SC_TEST_EXPECT(StringView("-0").parseInt32(value) and value == 0);
            SC_TEST_EXPECT(not StringView("").parseInt32(value));
        }

        if (test_section("parseFloat"))
        {
            StringView other;
            float      value;
            SC_TEST_EXPECT(not other.parseFloat(value));
            other = "\0";
            SC_TEST_EXPECT(not other.parseFloat(value));
            other = "+";
            SC_TEST_EXPECT(not other.parseFloat(value));
            other = "-";
            SC_TEST_EXPECT(not other.parseFloat(value));
            other = "+ ";
            SC_TEST_EXPECT(not other.parseFloat(value));
            other = "+1";
            SC_TEST_EXPECT(other.parseFloat(value));
            SC_TEST_EXPECT(value == 1.0f);
            other = "-123";
            SC_TEST_EXPECT(other.parseFloat(value));
            SC_TEST_EXPECT(value == -123.0f);
            other = StringView("-456___", 4, false, StringEncoding::Ascii);
            SC_TEST_EXPECT(other.parseFloat(value));
            SC_TEST_EXPECT(value == -456.0f);
            other = StringView("-456.2___", 6, false, StringEncoding::Ascii);
            SC_TEST_EXPECT(other.parseFloat(value));
            SC_TEST_EXPECT(StringView(".2").parseFloat(value) and value == 0.2f);
            SC_TEST_EXPECT(StringView("-.2").parseFloat(value) and value == -0.2f);
            SC_TEST_EXPECT(StringView(".0").parseFloat(value) and value == 0.0f);
            SC_TEST_EXPECT(StringView("-.0").parseFloat(value) and value == -0.0f);
            SC_TEST_EXPECT(StringView("0").parseFloat(value) and value == 0.0f);
            SC_TEST_EXPECT(StringView("-0").parseFloat(value) and value == -0.0f);
            SC_TEST_EXPECT(not StringView("-.").parseFloat(value));
            SC_TEST_EXPECT(not StringView("-..0").parseFloat(value));
            SC_TEST_EXPECT(not StringView("").parseFloat(value));
        }

        if (test_section("startsWith/endsWith"))
        {
            StringView test;
            for (int i = 0; i < 2; ++i)
            {
                if (i == 0)
                    test = StringView(L"Ciao_123");
                if (i == 1)
                    test = "Ciao_123"_a8;
                if (i == 2)
                    test = "Ciao_123"_u8;
                SC_TEST_EXPECT(test.startsWithChar('C'));
                SC_TEST_EXPECT(test.endsWithChar('3'));
                SC_TEST_EXPECT(test.startsWith("Ciao"));
                SC_TEST_EXPECT(test.startsWith("Ciao"_u8));
                SC_TEST_EXPECT(test.startsWith(L"Ciao"));
                SC_TEST_EXPECT(test.endsWith("123"));
                SC_TEST_EXPECT(test.endsWith(L"123"));
                SC_TEST_EXPECT(test.endsWith("123"_u8));
                SC_TEST_EXPECT(not test.startsWithChar('D'));
                SC_TEST_EXPECT(not test.endsWithChar('4'));
                SC_TEST_EXPECT(not test.startsWith("Cia_"));
                SC_TEST_EXPECT(not test.endsWith("1_3"));
            }

            StringView test2;
            SC_TEST_EXPECT(not test2.startsWithChar('a'));
            SC_TEST_EXPECT(not test2.endsWithChar('a'));
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
            SC_TEST_EXPECT(str.sliceStartEnd(0, 3) == "123");
            SC_TEST_EXPECT(str.sliceStartLength(4, 3) == "567");
            SC_TEST_EXPECT(str.sliceStartEnd(4, 7) == "567");
            SC_TEST_EXPECT(str.sliceStart(4) == "567");
            SC_TEST_EXPECT(str.sliceEnd(4) == "123");

            SC_TEST_EXPECT("myTest___"_a8.trimEndingChar('_') == "myTest");
            SC_TEST_EXPECT("myTest"_a8.trimEndingChar('_') == "myTest");
            SC_TEST_EXPECT("___myTest"_a8.trimStartingChar('_') == "myTest");
            SC_TEST_EXPECT("_myTest"_a8.trimStartingChar('_') == "myTest");
        }
        if (test_section("split"))
        {
            {
                StringViewTokenizer tokenizer("_123__567___");
                int                 numInvocations = 0;
                while (tokenizer.tokenizeNext('_', StringViewTokenizer::SkipEmpty))
                {
                    numInvocations++;
                    if (tokenizer.numSplitsNonEmpty == 1)
                    {
                        SC_TEST_EXPECT(tokenizer.component == "123");
                    }
                    else if (tokenizer.numSplitsNonEmpty == 2)
                    {
                        SC_TEST_EXPECT(tokenizer.component == "567");
                    }
                }
                SC_TEST_EXPECT(numInvocations == 2);
                SC_TEST_EXPECT(tokenizer.numSplitsNonEmpty == 2);
                SC_TEST_EXPECT(tokenizer.numSplitsTotal == 6);
            }
            {
                SC_TEST_EXPECT(StringViewTokenizer("___").countTokens('_').numSplitsNonEmpty == 0);
                SC_TEST_EXPECT(StringViewTokenizer("___").countTokens('_').numSplitsTotal == 3);
            }
            {
                SC_TEST_EXPECT(StringViewTokenizer("").countTokens('_').numSplitsNonEmpty == 0);
                SC_TEST_EXPECT(StringViewTokenizer("").countTokens('_').numSplitsTotal == 0);
            }
        }
        if (test_section("isInteger"))
        {
            SC_TEST_EXPECT("0"_a8.isIntegerNumber());
            SC_TEST_EXPECT(not ""_a8.isIntegerNumber());
            SC_TEST_EXPECT(not "-"_a8.isIntegerNumber());
            SC_TEST_EXPECT(not "."_a8.isIntegerNumber());
            SC_TEST_EXPECT(not "-."_a8.isIntegerNumber());
            SC_TEST_EXPECT("-34"_a8.isIntegerNumber());
            SC_TEST_EXPECT("+12"_a8.isIntegerNumber());
            SC_TEST_EXPECT(not "+12$"_a8.isIntegerNumber());
            SC_TEST_EXPECT(not "$+12"_a8.isIntegerNumber());
            SC_TEST_EXPECT(not "+$12"_a8.isIntegerNumber());
        }
        if (test_section("isFloating"))
        {
            SC_TEST_EXPECT("0"_a8.isFloatingNumber());
            SC_TEST_EXPECT(not ""_a8.isFloatingNumber());
            SC_TEST_EXPECT(not "-"_a8.isFloatingNumber());
            SC_TEST_EXPECT(not "."_a8.isFloatingNumber());
            SC_TEST_EXPECT(not "-."_a8.isFloatingNumber());
            SC_TEST_EXPECT("-34"_a8.isFloatingNumber());
            SC_TEST_EXPECT("+12"_a8.isFloatingNumber());
            SC_TEST_EXPECT(not "+12$"_a8.isFloatingNumber());
            SC_TEST_EXPECT(not "$+12"_a8.isFloatingNumber());
            SC_TEST_EXPECT(not "+$12"_a8.isFloatingNumber());
            SC_TEST_EXPECT("-34."_a8.isFloatingNumber());
            SC_TEST_EXPECT("-34.0"_a8.isFloatingNumber());
            SC_TEST_EXPECT("0.34"_a8.isFloatingNumber());
            SC_TEST_EXPECT(not "-34.0_"_a8.isFloatingNumber());
        }
        if (test_section("contains"))
        {
            StringView asd = "123 456"_a8;
            SC_TEST_EXPECT(asd.containsString("123"_a8));
            SC_TEST_EXPECT(asd.containsString("456"_a8));
            SC_TEST_EXPECT(not asd.containsString("124"_a8));
            SC_TEST_EXPECT(not asd.containsString("4567"_a8));
            size_t overlapPoints = 0;
            SC_TEST_EXPECT(not asd.fullyOverlaps("123___", overlapPoints) and overlapPoints == 3);
        }
        if (test_section("wildcard"))
        {
            SC_TEST_EXPECT(StringAlgorithms::matchWildcard("1?3", "123"));
            SC_TEST_EXPECT(StringAlgorithms::matchWildcard("1*3", "12223"));
            SC_TEST_EXPECT(StringAlgorithms::matchWildcard("*2", "12"));
            SC_TEST_EXPECT(not StringAlgorithms::matchWildcard("*1", "12"));
            SC_TEST_EXPECT(not StringAlgorithms::matchWildcard("*1", "112"));
            SC_TEST_EXPECT(not StringAlgorithms::matchWildcard("**1", "112"));
            SC_TEST_EXPECT(not StringAlgorithms::matchWildcard("*?1", "112"));
            SC_TEST_EXPECT(StringAlgorithms::matchWildcard("1*", "12123"));
            SC_TEST_EXPECT(StringAlgorithms::matchWildcard("*/myString", "myString/myString/myString"));
            SC_TEST_EXPECT(StringAlgorithms::matchWildcard("**/myString", "myString/myString/myString"));
            SC_TEST_EXPECT(not StringAlgorithms::matchWildcard("*/String", "myString/myString/myString"));
            SC_TEST_EXPECT(StringAlgorithms::matchWildcard("*/Directory/File.cpp", "/Root/Directory/File.cpp"));
        }
    }
};
