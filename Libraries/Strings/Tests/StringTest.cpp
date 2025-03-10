// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../String.h"
#include "../../Testing/Testing.h"

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
            SC_TEST_EXPECT(str.view() == sv);
            SC_TEST_EXPECT(str.owns(str.view().sliceStart(1)));
            String str2 = "Another String";
            SC_TEST_EXPECT(not str.owns(str2.view().sliceStart(1)));
            SC_TEST_EXPECT(str != "ASD");
            SC_TEST_EXPECT(str == "Test String");
            SC_TEST_EXPECT(str == str);
            SC_TEST_EXPECT(str != String("ASD"));
            str = "Salver";
            SC_TEST_EXPECT(str == "Salver");
            SC_TEST_EXPECT(str < "Zest string");
        }

        if (test_section("SmallString / String"))
        {
            String vec2;
            {
                SmallString<3> vec;
                SC_TEST_EXPECT(vec.assign("123"));
                SC_TEST_EXPECT(vec.data.size() == 4);

                vec2 = move(vec);
                SC_TEST_EXPECT(vec.data.isInline());
            }
            SC_TEST_EXPECT(not vec2.data.isInline());
        }

        if (test_section("SmallString"))
        {
            // Test String assignment to SmallString
            SmallString<10> ss10;
            String          normal("asd");
            ss10                 = normal;
            auto assertUpCasting = [this](String& s) { SC_TEST_EXPECT(s.sizeInBytesIncludingTerminator() == 4); };
            assertUpCasting(ss10);
            SC_TEST_EXPECT(ss10.view() == "asd");
            SC_TEST_EXPECT(ss10.data.isInline());
            SC_TEST_EXPECT(ss10.data.capacity() == 10);
            // Test SmallString assignment to regular string
            SmallString<20> ss20;
            ss20   = "ASD22";
            normal = move(ss20);
            SC_TEST_EXPECT(normal.view() == "ASD22");
            SC_TEST_EXPECT(not normal.data.isInline());
        }

        if (test_section("SmallString Buffer"))
        {
            SmallBuffer<5> myStuff;
            StringView     test = "ASDF";
            (void)myStuff.append({test.bytesIncludingTerminator(), test.sizeInBytesIncludingTerminator()});
            SmallString<5> ss = SmallString<5>(move(myStuff), test.getEncoding());
            SC_TEST_EXPECT(ss.data.size() == 5);
            SC_TEST_EXPECT(ss.data.capacity() == 5);
            SC_TEST_EXPECT(ss.data.isInline());
        }
    }
};

namespace SC
{
void runStringTest(SC::TestReport& report) { StringTest test(report); }
} // namespace SC
