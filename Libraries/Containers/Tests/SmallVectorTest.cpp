// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../SmallVector.h"
#include "../../Testing/Testing.h"

namespace SC
{
struct SmallVectorTest;
}

struct SC::SmallVectorTest : public SC::TestCase
{
    SmallVectorTest(SC::TestReport& report) : TestCase(report, "SmallVectorTest")
    {
        using namespace SC;
        if (test_section("shrink_to_fit"))
        {
            SmallVector<int, 3> vec;
            addItems(vec, 2);
            SC_TEST_EXPECT(vec.shrink_to_fit());
            SC_TEST_EXPECT(vec.capacity() == 3);
            SC_TEST_EXPECT(vec.size() == 2);
            SegmentHeader* vecHeader = SegmentHeader::getSegmentHeader(vec.items);
            SC_TEST_EXPECT(vecHeader->isSmallVector);
        }
        if (test_section("resize stack heap"))
        {
            SmallVector<int, 3> vec;
            SC_TEST_EXPECT(vec.resize(3));
            SegmentHeader* header;
            header = SegmentHeader::getSegmentHeader(vec.items);
            SC_TEST_EXPECT(header->isSmallVector);
            SC_TEST_EXPECT(not header->isFollowedBySmallVector);
            SC_TEST_EXPECT(vec.resize(4));
            header = SegmentHeader::getSegmentHeader(vec.items);
            SC_TEST_EXPECT(not header->isSmallVector);
            SC_TEST_EXPECT(header->isFollowedBySmallVector);
            SC_TEST_EXPECT(vec.resize(3));
            SC_TEST_EXPECT(vec.shrink_to_fit());
            header = SegmentHeader::getSegmentHeader(vec.items);
            SC_TEST_EXPECT(header->isSmallVector);
            SC_TEST_EXPECT(not header->isFollowedBySmallVector);
        }
        if (test_section("construction copy stack"))
        {
            SmallVector<int, 3> vec2;
            {
                SmallVector<int, 3> vec;
                addItems(vec, 3);
                SC_TEST_EXPECT(vec.buffer.size() == 3);
                SC_TEST_EXPECT(vec.push_back(3));
                SC_TEST_EXPECT(vec.buffer.size() == 0);
                SC_TEST_EXPECT(vec.pop_back());
                SC_TEST_EXPECT(vec.shrink_to_fit());
                SC_TEST_EXPECT(vec.buffer.size() == 3);
                vec2 = vec;
            }
            SC_TEST_EXPECT(vec2.size() == 3);
            checkItems(vec2, 3);
        }
        if (test_section("construction copy heap"))
        {
            SmallVector<int, 3> vec2;
            {
                SmallVector<int, 3> vec;
                addItems(vec, 4);
                SC_TEST_EXPECT(vec.size() == 4);
                vec2 = vec;
            }
            SegmentHeader* vec2Header = SegmentHeader::getSegmentHeader(vec2.items);
            SC_TEST_EXPECT(not vec2Header->isSmallVector);
            SC_TEST_EXPECT(vec2Header->isFollowedBySmallVector);
            SC_TEST_EXPECT(vec2.size() == 4);
            SC_TEST_EXPECT(vec2.buffer.size() == 0);
            checkItems(vec2, 4);
        }
        if (test_section("construction move SmallVector(stack)->Vector"))
        {
            Vector<int> vec2;
            {
                SmallVector<int, 3> vec;
                addItems(vec, 3);
                SC_TEST_EXPECT(vec.size() == 3);
                vec2 = move(vec);
            }
            SegmentHeader* vec2Header = SegmentHeader::getSegmentHeader(vec2.items);
            SC_TEST_EXPECT(not vec2Header->isSmallVector);
            SC_TEST_EXPECT(not vec2Header->isFollowedBySmallVector);
            checkItems(vec2, 3);
        }
        if (test_section("construction move SmallVector(heap)->Vector"))
        {
            Vector<int> vec2;
            {
                SmallVector<int, 3> vec;
                SegmentHeader*      vec1Header = SegmentHeader::getSegmentHeader(vec.items);
                addItems(vec, 4);
                SC_TEST_EXPECT(vec.size() == 4);

                vec2 = move(vec);
                SC_TEST_EXPECT(vec.items != nullptr);
                SegmentHeader* vec1Header2 = SegmentHeader::getSegmentHeader(vec.items);
                SC_TEST_EXPECT(vec1Header2 == vec1Header);
                SC_TEST_EXPECT(vec1Header2->isSmallVector);
                SC_TEST_EXPECT(vec1Header2->capacityBytes == 3 * sizeof(int));
            }
            SegmentHeader* vec2Header = SegmentHeader::getSegmentHeader(vec2.items);
            SC_TEST_EXPECT(not vec2Header->isSmallVector);
            SC_TEST_EXPECT(not vec2Header->isFollowedBySmallVector);
            checkItems(vec2, 4);
        }
        if (test_section("construction move Vector->SmallVector(heap)"))
        {
            SmallVector<int, 3> vec2;
            {
                Vector<int> vec;
                addItems(vec, 4);
                SC_TEST_EXPECT(vec.size() == 4);
                vec2 = move(vec);
                SC_TEST_EXPECT(vec.items == nullptr);
            }
            SegmentHeader* vec2Header = SegmentHeader::getSegmentHeader(vec2.items);
            SC_TEST_EXPECT(not vec2Header->isSmallVector);
            SC_TEST_EXPECT(vec2Header->isFollowedBySmallVector);
            checkItems(vec2, 4);
        }
        if (test_section("construction move Vector->SmallVector(stack)"))
        {
            SmallVector<int, 3> vec2;
            {
                Vector<int> vec;
                addItems(vec, 3);
                SC_TEST_EXPECT(vec.size() == 3);
                vec2 = move(vec);
                SC_TEST_EXPECT(vec.items == nullptr);
            }
            SegmentHeader* vec2Header = SegmentHeader::getSegmentHeader(vec2.items);
            SC_TEST_EXPECT(not vec2Header->isSmallVector);
            SC_TEST_EXPECT(vec2Header->isFollowedBySmallVector);
            SC_TEST_EXPECT(vec2.size() == 3);
            checkItems(vec2, 3);
        }
        if (test_section("construction move SmallVector(stack)->SmallVector(stack)"))
        {
            SmallVector<int, 3> vec2;
            {
                SmallVector<int, 3> vec;
                addItems(vec, 3);
                SC_TEST_EXPECT(vec.size() == 3);
                vec2 = move(vec);
#ifndef __clang_analyzer__
                SC_TEST_EXPECT(vec.size() == 0);
#endif // not __clang_analyzer__
                SC_TEST_EXPECT(vec2.size() == 3);
                SegmentHeader* vec1Header = SegmentHeader::getSegmentHeader(vec.items);
                SC_TEST_EXPECT(vec1Header != nullptr);
#ifndef __clang_analyzer__
                SC_TEST_EXPECT(vec1Header->isSmallVector);
#endif // not __clang_analyzer__
            }
            SegmentHeader* vec2Header = SegmentHeader::getSegmentHeader(vec2.items);
            SC_TEST_EXPECT(vec2Header->isSmallVector);
            checkItems(vec2, 3);
        }
        if (test_section("construction move SmallVector(heap)->SmallVector(stack)"))
        {
            SmallVector<int, 3> vec2;
            {
                SmallVector<int, 3> vec;
                addItems(vec, 4);
                SC_TEST_EXPECT(vec.size() == 4);
                vec2 = move(vec);
#ifndef __clang_analyzer__
                SC_TEST_EXPECT(vec.size() == 0);
#endif // not __clang_analyzer__
                SC_TEST_EXPECT(vec2.size() == 4);
                SegmentHeader* vec1Header = SegmentHeader::getSegmentHeader(vec.items);
                SC_TEST_EXPECT(vec1Header != nullptr);
#ifndef __clang_analyzer__
                SC_TEST_EXPECT(vec1Header->isSmallVector);
#endif // not __clang_analyzer__
            }
            SegmentHeader* vec2Header = SegmentHeader::getSegmentHeader(vec2.items);
            SC_TEST_EXPECT(not vec2Header->isSmallVector);
            SC_TEST_EXPECT(vec2Header->isFollowedBySmallVector);
            checkItems(vec2, 4);
        }
        if (test_section("construction move SmallVector(stack)->SmallVector(stack)"))
        {
            SmallVector<int, 3> vec2;
            {
                SmallVector<int, 3> vec;
                addItems(vec, 3);
                SC_TEST_EXPECT(vec.size() == 3);
                vec2 = move(vec);
#ifndef __clang_analyzer__
                SC_TEST_EXPECT(vec.size() == 0);
#endif // not __clang_analyzer__
                SC_TEST_EXPECT(vec2.size() == 3);
                SegmentHeader* vec1Header = SegmentHeader::getSegmentHeader(vec.items);
                SC_TEST_EXPECT(vec1Header != nullptr);
#ifndef __clang_analyzer__
                SC_TEST_EXPECT(vec1Header->isSmallVector);
#endif // not __clang_analyzer__
            }
            SegmentHeader* vec2Header = SegmentHeader::getSegmentHeader(vec2.items);
            SC_TEST_EXPECT(vec2Header->isSmallVector);
            checkItems(vec2, 3);
        }
        if (test_section("construction move SmallVector(heap)->SmallVector(stack)"))
        {
            SmallVector<int, 4> vec2;
            {
                SmallVector<int, 3> vec;
                addItems(vec, 4);
                SC_TEST_EXPECT(vec.size() == 4);
                vec2 = move(vec);
                SC_TEST_EXPECT(vec.size() == 0);
                SC_TEST_EXPECT(vec2.size() == 4);
                SegmentHeader* vec1Header = SegmentHeader::getSegmentHeader(vec.items);
                SC_TEST_EXPECT(vec1Header != nullptr);
#ifndef __clang_analyzer__
                SC_TEST_EXPECT(vec1Header->isSmallVector);
#endif // not __clang_analyzer__
            }
            SegmentHeader* vec2Header = SegmentHeader::getSegmentHeader(vec2.items);
            SC_TEST_EXPECT(not vec2Header->isSmallVector);
            SC_TEST_EXPECT(vec2Header->isFollowedBySmallVector);
            checkItems(vec2, 4);
        }
        if (test_section("move operations"))
        {
            struct Container
            {
                SmallVector<int, 3> data;
            };
            Container c;
            SC_TEST_EXPECT(c.data.capacity() == 3);
            Container c1 = move(c);
            SC_TEST_EXPECT(c1.data.capacity() == 3);

            SC_TEST_EXPECT(c1.data.reserve(5));
            c = move(c1);
            SC_TEST_EXPECT(c.data.capacity() == 5);
        }
    }

    template <typename Container>
    void checkItems(Container& container, size_t numItems)
    {
        for (size_t idx = 0; idx < numItems; ++idx)
        {
            SC_TEST_EXPECT(container[idx] == static_cast<int>(idx));
        }
    }

    template <typename Container>
    void addItems(Container& container, size_t numItems)
    {
        for (size_t idx = 0; idx < numItems; ++idx)
        {
            SC_TEST_EXPECT(container.push_back(static_cast<int>(idx)));
        }
    }
};

namespace SC
{
void runSmallVectorTest(SC::TestReport& report) { SmallVectorTest test(report); }
} // namespace SC
