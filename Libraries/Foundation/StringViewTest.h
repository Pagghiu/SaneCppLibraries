// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
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
            SC_TEST_EXPECT(s.sizeInBytesWithoutTerminator() == 3);
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

        if (test_section("startsWith/endsWith"))
        {
            StringView test("Ciao_123");
            SC_TEST_EXPECT(test.startsWith('C'));
            SC_TEST_EXPECT(test.endsWith('3'));
            SC_TEST_EXPECT(test.startsWith("Ciao"_sv));
            SC_TEST_EXPECT(test.endsWith("123"_sv));
            SC_TEST_EXPECT(not test.startsWith('D'));
            SC_TEST_EXPECT(not test.endsWith('4'));
            SC_TEST_EXPECT(not test.startsWith("Cia_"_sv));
            SC_TEST_EXPECT(not test.endsWith("1_3"_sv));

            StringView test2;
            SC_TEST_EXPECT(not test2.startsWith('a'));
            SC_TEST_EXPECT(not test2.endsWith('a'));
            SC_TEST_EXPECT(test2.startsWith(""_sv));
            SC_TEST_EXPECT(not test2.startsWith("A"_sv));
            SC_TEST_EXPECT(test2.endsWith(""_sv));
            SC_TEST_EXPECT(not test2.endsWith("A"_sv));
        }
    }
};
