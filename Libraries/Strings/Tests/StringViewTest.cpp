// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../StringView.h"
#include "../../Algorithms/AlgorithmBubbleSort.h"
#include "../../Testing/Testing.h"

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
            other = StringView({"-456___", 4}, false, StringEncoding::Ascii);
            SC_TEST_EXPECT(other.parseInt32(value));
            SC_TEST_EXPECT(value == -456);
            SC_TEST_EXPECT(StringView("0").parseInt32(value) and value == 0);
            SC_TEST_EXPECT(StringView("-0").parseInt32(value) and value == 0);
            SC_TEST_EXPECT(not StringView("").parseInt32(value));
#if SC_COMPILER_MSVC
            SC_TEST_EXPECT(StringView(L"214").parseInt32(value) and value == 214);
            SC_TEST_EXPECT(StringView(L"+214").parseInt32(value) and value == +214);
            SC_TEST_EXPECT(StringView(L"-214").parseInt32(value) and value == -214);
            SC_TEST_EXPECT(not StringView(L"a214").parseInt32(value));
            SC_TEST_EXPECT(not StringView(L"-a14").parseInt32(value));
#else
            SC_TEST_EXPECT(StringView("\x32\x00\x31\x00\x34\x00"_u16).parseInt32(value) and value == 214);
            SC_TEST_EXPECT(StringView("\x2b\x00\x32\x00\x31\x00\x34\x00"_u16).parseInt32(value) and value == +214);
            SC_TEST_EXPECT(StringView("\x2d\x00\x32\x00\x31\x00\x34\x00"_u16).parseInt32(value) and value == -214);
            SC_TEST_EXPECT(not StringView("\x61\x00\x32\x00\x31\x00\x34\x00"_u16).parseInt32(value));
            SC_TEST_EXPECT(not StringView("\x2d\x00\x61\x00\x31\x00\x34\x00"_u16).parseInt32(value));
#endif
            SC_TEST_EXPECT(not StringView("1234567891234567").parseInt32(value)); // Too long for int32
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
            other = StringView({"-456___", 4}, false, StringEncoding::Ascii);
            SC_TEST_EXPECT(other.parseFloat(value));
            SC_TEST_EXPECT(value == -456.0f);
            other = StringView({"-456.2___", 6}, false, StringEncoding::Ascii);
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
                    test = StringView("\x43\x00\x69\x00\x61\x00\x6f\x00\x5f\x00\x31\x00\x32\x00\x33\x00\x00"_u16);
                if (i == 1)
                    test = "Ciao_123"_a8;
                if (i == 2)
                    test = "Ciao_123"_u8;
                SC_TEST_EXPECT(test.startsWithAnyOf({'C', '_'}));
                SC_TEST_EXPECT(test.endsWithAnyOf({'3', 'z'}));
                SC_TEST_EXPECT(test.startsWith("Ciao"));
                SC_TEST_EXPECT(test.startsWith("Ciao"_u8));
                SC_TEST_EXPECT(test.startsWith("\x43\x00\x69\x00\x61\x00\x6f\x00\x00"_u16));
                SC_TEST_EXPECT(test.endsWith("\x31\x00\x32\x00\x33\x00\x00"_u16));
                SC_TEST_EXPECT(test.endsWith("123"));
                SC_TEST_EXPECT(test.endsWith("123"_u8));
                SC_TEST_EXPECT(not test.startsWithAnyOf({'D', '_'}));
                SC_TEST_EXPECT(not test.endsWithAnyOf({'4', 'z'}));
                SC_TEST_EXPECT(not test.startsWith("Cia_"));
                SC_TEST_EXPECT(not test.endsWith("1_3"));
            }

            StringView test2;
            SC_TEST_EXPECT(not test2.startsWithAnyOf({'a', '_'}));
            SC_TEST_EXPECT(not test2.endsWithAnyOf({'a', 'z'}));
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

            SC_TEST_EXPECT("myTest_\n__"_a8.trimEndAnyOf({'_', '\n'}) == "myTest");
            SC_TEST_EXPECT("myTest"_a8.trimEndAnyOf({'_'}) == "myTest");
            SC_TEST_EXPECT("_\n__myTest"_a8.trimStartAnyOf({'_', '\n'}) == "myTest");
            SC_TEST_EXPECT("_myTest"_a8.trimStartAnyOf({'_'}) == "myTest");
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
            SC_TEST_EXPECT(asd.containsString("123"));
            SC_TEST_EXPECT(asd.containsString("456"));
            SC_TEST_EXPECT(not asd.containsString("124"));
            SC_TEST_EXPECT(not asd.containsString("4567"));
            size_t overlapPoints = 0;
            SC_TEST_EXPECT(not asd.fullyOverlaps("123___", overlapPoints) and overlapPoints == 3);
        }
        if (test_section("compare"))
        {
            StringView sv[3] = {
                StringView("3"),
                StringView("1"),
                StringView("2"),
            };
            Algorithms::bubbleSort(sv, sv + 3, [](StringView a, StringView b) { return a < b; });
            SC_TEST_EXPECT(sv[0] == "1");
            SC_TEST_EXPECT(sv[1] == "2");
            SC_TEST_EXPECT(sv[2] == "3");
            Algorithms::bubbleSort(
                sv, sv + 3, [](StringView a, StringView b) { return a.compare(b) == StringView::Comparison::Bigger; });
            SC_TEST_EXPECT(sv[0] == "3");
            SC_TEST_EXPECT(sv[1] == "2");
            SC_TEST_EXPECT(sv[2] == "1");
        }
        if (test_section("compare UTF"))
        {
            // àèìòù (1 UTF16-LE sequence, 2 UTF8 sequence)
            SC_TEST_EXPECT("\xc3\xa0\xc3\xa8\xc3\xac\xc3\xb2\xc3\xb9"_u8.compare(
                               "\xe0\x0\xe8\x0\xec\x0\xf2\x0\xf9\x0"_u16) == StringView::Comparison::Equals);

            // 日本語語語 (1 UTF16-LE sequence, 3 UTF8 sequence)
            StringView stringUtf8  = StringView("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe8\xaa\x9e\xe8\xaa\x9e"_u8);
            StringView stringUtf16 = StringView("\xE5\x65\x2C\x67\x9E\x8a\x9E\x8a\x9E\x8a\x00"_u16); // LE

            SC_TEST_EXPECT(stringUtf8.compare(stringUtf16) == StringView::Comparison::Equals);
            SC_TEST_EXPECT(stringUtf16.compare(stringUtf8) == StringView::Comparison::Equals);
            SC_TEST_EXPECT(stringUtf8 == stringUtf16);
            SC_TEST_EXPECT(stringUtf16 == stringUtf8);
            // U+24B62 (2 UTF16-LE sequence, 4 UTF8 sequence)
            SC_TEST_EXPECT("\xf0\xa4\xad\xa2"_u8.compare("\x52\xD8\x62\xDF\x00"_u16) == StringView::Comparison::Equals);
            StringView aASCII = "A"_a8;
            StringView bUTF8  = "B"_u8;
            StringView aUTF8  = "A"_u8;
            StringView cUTF16 = "C\0\0"_u16;
            StringView aUTF16 = "A\0\0"_u16;
            SC_TEST_EXPECT(aASCII.compare(bUTF8) == StringView::Comparison::Smaller);
            SC_TEST_EXPECT(bUTF8.compare(aASCII) == StringView::Comparison::Bigger);
            SC_TEST_EXPECT(bUTF8.compare(cUTF16) == StringView::Comparison::Smaller);
            SC_TEST_EXPECT(cUTF16.compare(bUTF8) == StringView::Comparison::Bigger);
            SC_TEST_EXPECT(cUTF16.compare(aASCII) == StringView::Comparison::Bigger);
            SC_TEST_EXPECT(aASCII.compare(cUTF16) == StringView::Comparison::Smaller);
            SC_TEST_EXPECT(aASCII.compare(aUTF8) == StringView::Comparison::Equals);
            SC_TEST_EXPECT(aUTF8.compare(aASCII) == StringView::Comparison::Equals);
            SC_TEST_EXPECT(aASCII.compare(aUTF16) == StringView::Comparison::Equals);
            SC_TEST_EXPECT(aUTF16.compare(aASCII) == StringView::Comparison::Equals);
            SC_TEST_EXPECT(aUTF8.compare(aUTF16) == StringView::Comparison::Equals);
            SC_TEST_EXPECT(aUTF16.compare(aUTF8) == StringView::Comparison::Equals);
        }
        if (test_section("wildcard"))
        {
            SC_TEST_EXPECT(StringAlgorithms::matchWildcard("", ""));
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

namespace SC
{
void runStringViewTest(SC::TestReport& report) { StringViewTest test(report); }
} // namespace SC
