// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "SmallVector.h"
#include "Test.h"

namespace SC
{
struct SmallVectorTest;
}
template <typename T, int N>
struct SC::SmallVector : public Vector<T>
{
    Array<T, N> buffer;
    SmallVector()
    {
        SegmentHeader* header         = SegmentHeader::getSegmentHeader(buffer.items);
        header->options.isSmallVector = true;
        Vector<T>::items              = buffer.items;
    }
    SmallVector(SmallVector&& other) : Vector<T>(forward<Vector<T>>(other)) {}
    SmallVector(const SmallVector& other) : Vector<T>(other) {}
    SmallVector& operator=(SmallVector&& other)
    {
        Vector<T>::operator=(forward<Vector<T>>(other));
        return *this;
    }
    SmallVector& operator=(const SmallVector& other)
    {
        Vector<T>::operator=(other);
        return *this;
    }

    SmallVector(Vector<T>&& other) : Vector<T>(forward<Vector<T>>(other)) {}
    SmallVector(const Vector<T>& other) : Vector<T>(other) {}
    SmallVector& operator=(Vector<T>&& other)
    {
        Vector<T>::operator=(forward<Vector<T>>(other));
        return *this;
    }
    SmallVector& operator=(const Vector<T>& other)
    {
        Vector<T>::operator=(other);
        return *this;
    }
};

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
            SC_TEST_EXPECT(vecHeader->options.isSmallVector);
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
            SC_TEST_EXPECT(not vec2Header->options.isSmallVector);
            SC_TEST_EXPECT(vec2Header->options.isFollowedBySmallVector);
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
            SC_TEST_EXPECT(not vec2Header->options.isSmallVector);
            SC_TEST_EXPECT(not vec2Header->options.isFollowedBySmallVector);
            checkItems(vec2, 3);
        }
        if (test_section("construction move SmallVector(heap)->Vector"))
        {
            Vector<int> vec2;
            {
                SmallVector<int, 3> vec;
                addItems(vec, 4);
                SC_TEST_EXPECT(vec.size() == 4);

                vec2                      = move(vec);
                SegmentHeader* vec1Header = SegmentHeader::getSegmentHeader(vec.items);
                SC_TEST_EXPECT(vec1Header->options.isSmallVector);
            }
            SegmentHeader* vec2Header = SegmentHeader::getSegmentHeader(vec2.items);
            SC_TEST_EXPECT(not vec2Header->options.isSmallVector);
            SC_TEST_EXPECT(not vec2Header->options.isFollowedBySmallVector);
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
            SC_TEST_EXPECT(not vec2Header->options.isSmallVector);
            SC_TEST_EXPECT(vec2Header->options.isFollowedBySmallVector);
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
            SC_TEST_EXPECT(not vec2Header->options.isSmallVector);
            SC_TEST_EXPECT(vec2Header->options.isFollowedBySmallVector);
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
                SC_TEST_EXPECT(vec.size() == 0);
                SC_TEST_EXPECT(vec2.size() == 3);
                SegmentHeader* vec1Header = SegmentHeader::getSegmentHeader(vec.items);
                SC_TEST_EXPECT(vec1Header != nullptr);
                SC_TEST_EXPECT(vec1Header->options.isSmallVector);
            }
            SegmentHeader* vec2Header = SegmentHeader::getSegmentHeader(vec2.items);
            SC_TEST_EXPECT(vec2Header->options.isSmallVector);
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
                SC_TEST_EXPECT(vec.size() == 0);
                SC_TEST_EXPECT(vec2.size() == 4);
                SegmentHeader* vec1Header = SegmentHeader::getSegmentHeader(vec.items);
                SC_TEST_EXPECT(vec1Header != nullptr);
                SC_TEST_EXPECT(vec1Header->options.isSmallVector);
            }
            SegmentHeader* vec2Header = SegmentHeader::getSegmentHeader(vec2.items);
            SC_TEST_EXPECT(not vec2Header->options.isSmallVector);
            SC_TEST_EXPECT(vec2Header->options.isFollowedBySmallVector);
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
                SC_TEST_EXPECT(vec.size() == 0);
                SC_TEST_EXPECT(vec2.size() == 3);
                SegmentHeader* vec1Header = SegmentHeader::getSegmentHeader(vec.items);
                SC_TEST_EXPECT(vec1Header != nullptr);
                SC_TEST_EXPECT(vec1Header->options.isSmallVector);
            }
            SegmentHeader* vec2Header = SegmentHeader::getSegmentHeader(vec2.items);
            SC_TEST_EXPECT(vec2Header->options.isSmallVector);
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
                SC_TEST_EXPECT(vec1Header->options.isSmallVector);
            }
            SegmentHeader* vec2Header = SegmentHeader::getSegmentHeader(vec2.items);
            SC_TEST_EXPECT(not vec2Header->options.isSmallVector);
            SC_TEST_EXPECT(vec2Header->options.isFollowedBySmallVector);
            checkItems(vec2, 4);
        }
    }

    template <typename Container>
    void checkItems(Container& container, int numItems)
    {
        for (int idx = 0; idx < numItems; ++idx)
        {
            SC_TEST_EXPECT(container[idx] == idx);
        }
    }

    template <typename Container>
    void addItems(Container& container, int numItems)
    {
        for (int idx = 0; idx < numItems; ++idx)
        {
            SC_TEST_EXPECT(container.push_back(idx));
        }
    }
};
