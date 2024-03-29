// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../SmallString.h"
#include "../../Containers/SmallVector.h"
#include "../../Testing/Testing.h"
#include "../StringBuilder.h"

namespace SC
{
struct SmallStringTest;
}

struct SC::SmallStringTest : public SC::TestCase
{
    SmallStringTest(SC::TestReport& report) : TestCase(report, "SmallStringTest")
    {
        using namespace SC;
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
            // Test String assignment to SmallString
            SmallString<10> ss10;
            String          normal("asd");
            ss10                 = normal;
            auto assertUpCasting = [this](String& s) { SC_TEST_EXPECT(s.sizeInBytesIncludingTerminator() == 4); };
            assertUpCasting(ss10);
            SC_TEST_EXPECT(ss10.view() == "asd");
            SC_TEST_EXPECT(SegmentHeader::getSegmentHeader(ss10.data.items)->isSmallVector);
            SC_TEST_EXPECT(SegmentHeader::getSegmentHeader(ss10.data.items)->capacityBytes == 10);
            // Test SmallString assignment to regular string
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
    }
};

namespace SC
{
void runSmallStringTest(SC::TestReport& report) { SmallStringTest test(report); }
} // namespace SC
