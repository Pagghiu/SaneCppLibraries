// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../Testing/Test.h"
#include "../Algorithms/AlgorithmSort.h"
#include "../Containers/SmallVector.h"
#include "String.h"
#include "StringBuilder.h"

namespace SC
{
struct SmallStringTest;
}

struct SC::SmallStringTest : public SC::TestCase
{
    SmallStringTest(SC::TestReport& report) : TestCase(report, "SmallStringTest")
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
            str = "Salver";
            SC_TEST_EXPECT(str == "Salver");
            // SC_TEST_EXPECT(str < "Zest Ztring");
        }
        if (test_section("compare"))
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
            bubbleSort(sv, sv + 3,
                       [](StringView a, StringView b) { return a.compare(b) == StringView::Comparison::Bigger; });
            SC_TEST_EXPECT(sv[0] == "3");
            SC_TEST_EXPECT(sv[1] == "2");
            SC_TEST_EXPECT(sv[2] == "1");
        }
        if (test_section("construction move SmallVector(heap)->Vector"))
        {
            String vec2;
            {
                SmallString<3> vec;
                SegmentHeader* vec1Header = SegmentHeader::getSegmentHeader(vec.data.items);
                SC_TEST_EXPECT(vec.assign("123"));
                SC_TEST_EXPECT(vec.data.size() == 4);

                vec2 = move(vec);
                SC_TEST_EXPECT(vec.data.items != nullptr);
                SegmentHeader* vec1Header2 = SegmentHeader::getSegmentHeader(vec.data.items);
                SC_TEST_EXPECT(vec1Header2 == vec1Header);
                SC_TEST_EXPECT(vec1Header2->isSmallVector);
                SC_TEST_EXPECT(vec1Header2->capacityBytes == 3 * sizeof(char));
            }
            SegmentHeader* vec2Header = SegmentHeader::getSegmentHeader(vec2.data.items);
            SC_TEST_EXPECT(not vec2Header->isSmallVector);
            SC_TEST_EXPECT(not vec2Header->isFollowedBySmallVector);
        }
        if (test_section("SmallString"))
        {
            // Test String ssignable to SmallString
            SmallString<10> ss10;
            String          normal("asd");
            ss10                 = normal;
            auto assertUpcasting = [this](String& s) { SC_TEST_EXPECT(s.sizeInBytesIncludingTerminator() == 4); };
            assertUpcasting(ss10);
            SC_TEST_EXPECT(ss10.view() == "asd");
            SC_TEST_EXPECT(SegmentHeader::getSegmentHeader(ss10.data.items)->isSmallVector);
            SC_TEST_EXPECT(SegmentHeader::getSegmentHeader(ss10.data.items)->capacityBytes == 10);
            // Test SmallString assignable to regular string
            SmallString<20> ss20;
            ss20   = "ASD22";
            normal = move(ss20);
            SC_TEST_EXPECT(normal.view() == "ASD22");
            SC_TEST_EXPECT(not SegmentHeader::getSegmentHeader(normal.data.items)->isSmallVector);
            SC_TEST_EXPECT(not SegmentHeader::getSegmentHeader(normal.data.items)->isFollowedBySmallVector);
        }
        if (test_section("SmallString Vector"))
        {
            SmallVector<char, 5> myStuff;
            StringView           test = "ASDF";
            (void)myStuff.append({test.bytesIncludingTerminator(), test.sizeInBytesIncludingTerminator()});
            SmallString<5> ss = SmallString<5>(move(myStuff), test.getEncoding());
            SC_TEST_EXPECT(ss.data.size() == 5);
            SC_TEST_EXPECT(ss.data.capacity() == 5);
            SC_TEST_EXPECT(SegmentHeader::getSegmentHeader(ss.data.items)->isSmallVector);
        }
        if (test_section("HexString"))
        {
            uint8_t bytes[4] = {0x12, 0x34, 0x56, 0x78};

            String        s;
            StringBuilder b(s);
            SC_TEST_EXPECT(b.appendHex({bytes, sizeof(bytes)}) and s.view() == "12345678");
        }
    }
};
