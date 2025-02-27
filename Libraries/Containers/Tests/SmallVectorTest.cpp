// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
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
            SegmentHeader* vecHeader = vec.unsafeGetHeader();
            SC_TEST_EXPECT(vecHeader->isInlineBuffer);
        }
        if (test_section("resize stack heap"))
        {
            SmallVector<int, 3> vec;
            SC_TEST_EXPECT(vec.resize(3));
            SegmentHeader* header;
            header = vec.unsafeGetHeader();
            SC_TEST_EXPECT(header->isInlineBuffer);
            SC_TEST_EXPECT(not header->isFollowedByInlineBuffer);
            SC_TEST_EXPECT(vec.resize(4));
            header = vec.unsafeGetHeader();
            SC_TEST_EXPECT(not header->isInlineBuffer);
            SC_TEST_EXPECT(header->isFollowedByInlineBuffer);
            SC_TEST_EXPECT(vec.resize(3));
            SC_TEST_EXPECT(vec.shrink_to_fit());
            header = vec.unsafeGetHeader();
            SC_TEST_EXPECT(header->isInlineBuffer);
            SC_TEST_EXPECT(not header->isFollowedByInlineBuffer);
        }
        if (test_section("construction copy stack"))
        {
            SmallVector<int, 3> vec2;
            {
                SmallVector<int, 3> vec;
                addItems(vec, 3);
                SC_TEST_EXPECT(vec.isInlineBuffer() and vec.size() == 3);
                SC_TEST_EXPECT(vec.push_back(3));
                SC_TEST_EXPECT(not vec.isInlineBuffer());
                SC_TEST_EXPECT(vec.pop_back());
                SC_TEST_EXPECT(vec.shrink_to_fit());
                SC_TEST_EXPECT(vec.isInlineBuffer() and vec.size() == 3);
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
            SegmentHeader* vec2Header = vec2.unsafeGetHeader();
            SC_TEST_EXPECT(not vec2Header->isInlineBuffer);
            SC_TEST_EXPECT(vec2Header->isFollowedByInlineBuffer);
            SC_TEST_EXPECT(vec2.size() == 4);
            SC_TEST_EXPECT(not vec2.isInlineBuffer());
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
            SegmentHeader* vec2Header = vec2.unsafeGetHeader();
            SC_TEST_EXPECT(not vec2Header->isInlineBuffer);
            SC_TEST_EXPECT(not vec2Header->isFollowedByInlineBuffer);
            checkItems(vec2, 3);
        }
        if (test_section("construction move SmallVector(heap)->Vector"))
        {
            Vector<int> vec2;
            {
                SmallVector<int, 3> vec;
                SegmentHeader*      vec1Header = vec.unsafeGetHeader();
                addItems(vec, 4);
                SC_TEST_EXPECT(vec.size() == 4);

                vec2 = move(vec);
                SC_TEST_EXPECT(vec.data() != nullptr);
                SegmentHeader* vec1Header2 = vec.unsafeGetHeader();
                SC_TEST_EXPECT(vec1Header2 == vec1Header);
                SC_TEST_EXPECT(vec1Header2->isInlineBuffer);
                SC_TEST_EXPECT(vec1Header2->capacityBytes == 3 * sizeof(int));
            }
            SegmentHeader* vec2Header = vec2.unsafeGetHeader();
            SC_TEST_EXPECT(not vec2Header->isInlineBuffer);
            SC_TEST_EXPECT(not vec2Header->isFollowedByInlineBuffer);
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
                SC_TEST_EXPECT(vec.data() == nullptr);
            }
            SegmentHeader* vec2Header = vec2.unsafeGetHeader();
            SC_TEST_EXPECT(not vec2Header->isInlineBuffer);
            SC_TEST_EXPECT(vec2Header->isFollowedByInlineBuffer);
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
                SC_TEST_EXPECT(vec.data() == nullptr);
            }
            SegmentHeader* vec2Header = vec2.unsafeGetHeader();
            SC_TEST_EXPECT(not vec2Header->isInlineBuffer);
            SC_TEST_EXPECT(vec2Header->isFollowedByInlineBuffer);
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
                SegmentHeader* vec1Header = vec.unsafeGetHeader();
                SC_TEST_EXPECT(vec1Header != nullptr);
#ifndef __clang_analyzer__
                SC_TEST_EXPECT(vec1Header->isInlineBuffer);
#endif // not __clang_analyzer__
            }
            SegmentHeader* vec2Header = vec2.unsafeGetHeader();
            SC_TEST_EXPECT(vec2Header->isInlineBuffer);
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
                SegmentHeader* vec1Header = vec.unsafeGetHeader();
                SC_TEST_EXPECT(vec1Header != nullptr);
#ifndef __clang_analyzer__
                SC_TEST_EXPECT(vec1Header->isInlineBuffer);
#endif // not __clang_analyzer__
            }
            SegmentHeader* vec2Header = vec2.unsafeGetHeader();
            SC_TEST_EXPECT(not vec2Header->isInlineBuffer);
            SC_TEST_EXPECT(vec2Header->isFollowedByInlineBuffer);
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
                SegmentHeader* vec1Header = vec.unsafeGetHeader();
                SC_TEST_EXPECT(vec1Header != nullptr);
#ifndef __clang_analyzer__
                SC_TEST_EXPECT(vec1Header->isInlineBuffer);
#endif // not __clang_analyzer__
            }
            SegmentHeader* vec2Header = vec2.unsafeGetHeader();
            SC_TEST_EXPECT(vec2Header->isInlineBuffer);
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
                SegmentHeader* vec1Header = vec.unsafeGetHeader();
                SC_TEST_EXPECT(vec1Header != nullptr);
#ifndef __clang_analyzer__
                SC_TEST_EXPECT(vec1Header->isInlineBuffer);
#endif // not __clang_analyzer__
            }
            SegmentHeader* vec2Header = vec2.unsafeGetHeader();
            SC_TEST_EXPECT(not vec2Header->isInlineBuffer);
            SC_TEST_EXPECT(vec2Header->isFollowedByInlineBuffer);
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
            SC_TEST_EXPECT(c.data.capacity() == 3); // because c1 is empty
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

    bool smallVectorSnippet();
};

bool SC::SmallVectorTest::smallVectorSnippet()
{
    //! [SmallVectorSnippet]
    auto pushThreeIntegers = [](Vector<int>& myVector) -> bool
    {
        SC_TRY(myVector.push_back(1));
        SC_TRY(myVector.push_back(2));
        SC_TRY(myVector.push_back(3));
        return true;
    };
    //...

    SmallVector<int, 3> mySmallVector;
    SC_TRY(pushThreeIntegers(mySmallVector)); // <-- No heap allocation will happen

    // ... later on

    SC_TRY(mySmallVector.push_back(4)); // <-- Vector is now moved to heap

    // ... later on

    SC_TRY(mySmallVector.pop_back()); // <-- Vector is moved back to SmallVector inline storage
    //! [SmallVectorSnippet]
    return true;
}

namespace SC
{
void runSmallVectorTest(SC::TestReport& report) { SmallVectorTest test(report); }
} // namespace SC
