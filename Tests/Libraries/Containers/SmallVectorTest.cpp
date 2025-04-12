// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Containers/Vector.h"
#include "Libraries/Testing/Testing.h"

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
            SC_TEST_EXPECT(vec.isInline());
        }
        if (test_section("resize stack heap"))
        {
            SmallVector<int, 3> vec;
            SC_TEST_EXPECT(vec.resize(3));
            SC_TEST_EXPECT(vec.isInline());
            SC_TEST_EXPECT(vec.resize(4));
            SC_TEST_EXPECT(not vec.isInline());
            SC_TEST_EXPECT(vec.resize(3));
            SC_TEST_EXPECT(vec.shrink_to_fit());
            SC_TEST_EXPECT(vec.isInline());
        }
        if (test_section("construction copy stack"))
        {
            SmallVector<int, 3> vec2;
            {
                SmallVector<int, 3> vec;
                addItems(vec, 3);
                SC_TEST_EXPECT(vec.isInline() and vec.size() == 3);
                SC_TEST_EXPECT(vec.push_back(3));
                SC_TEST_EXPECT(not vec.isInline());
                SC_TEST_EXPECT(vec.pop_back());
                SC_TEST_EXPECT(vec.shrink_to_fit());
                SC_TEST_EXPECT(vec.isInline() and vec.size() == 3);
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
            SC_TEST_EXPECT(not vec2.isInline());
            SC_TEST_EXPECT(vec2.size() == 4);
            SC_TEST_EXPECT(not vec2.isInline());
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
            SC_TEST_EXPECT(not vec2.isInline());
            checkItems(vec2, 3);
        }
        if (test_section("construction move SmallVector(heap)->Vector"))
        {
            Vector<int> vec4;
            {
                SmallVector<int, 3> smallVec3;
                addItems(smallVec3, 4);
                SC_TEST_EXPECT(smallVec3.size() == 4);

                vec4 = move(smallVec3);
                SC_TEST_EXPECT(smallVec3.data() != nullptr);
                SC_TEST_EXPECT(smallVec3.isInline());
                SC_TEST_EXPECT(smallVec3.capacity() == 3); // restored initial capacity
            }
            SC_TEST_EXPECT(not vec4.isInline());
            checkItems(vec4, 4);
        }
        if (test_section("construction move Vector->SmallVector(heap)"))
        {
            SmallVector<int, 3> smallVec3;
            {
                Vector<int> vec4;
                addItems(vec4, 4);
                SC_TEST_EXPECT(vec4.size() == 4);
                smallVec3 = move(vec4);
                SC_TEST_EXPECT(vec4.data() == nullptr);
            }
            SC_TEST_EXPECT(not smallVec3.isInline());
            checkItems(smallVec3, 4);
        }
        if (test_section("construction move Vector->SmallVector(stack)"))
        {
            SmallVector<int, 3> smallVec3;
            {
                Vector<int> vec3;
                addItems(vec3, 3);
                SC_TEST_EXPECT(vec3.size() == 3);
                smallVec3 = move(vec3);
                SC_TEST_EXPECT(vec3.data() == nullptr);
            }
            SC_TEST_EXPECT(not smallVec3.isInline());
            SC_TEST_EXPECT(smallVec3.size() == 3);
            checkItems(smallVec3, 3);
        }
        if (test_section("construction move SmallVector(stack)->SmallVector(stack)"))
        {
            SmallVector<int, 3> smallVec3A;
            {
                SmallVector<int, 3> smallVec3B;
                addItems(smallVec3B, 3);
                SC_TEST_EXPECT(smallVec3B.size() == 3);
                smallVec3A = move(smallVec3B);
#ifndef __clang_analyzer__
                SC_TEST_EXPECT(smallVec3B.size() == 0);
#endif // not __clang_analyzer__
                SC_TEST_EXPECT(smallVec3A.size() == 3);
#ifndef __clang_analyzer__
                SC_TEST_EXPECT(smallVec3B.isInline());
#endif // not __clang_analyzer__
            }
            SC_TEST_EXPECT(smallVec3A.isInline());
            checkItems(smallVec3A, 3);
        }
        if (test_section("construction move SmallVector(heap)->SmallVector(stack)"))
        {
            SmallVector<int, 3> smallVec3A;
            {
                SmallVector<int, 3> smallVec3B;
                addItems(smallVec3B, 4);
                SC_TEST_EXPECT(smallVec3B.size() == 4);
                smallVec3A = move(smallVec3B);
#ifndef __clang_analyzer__
                SC_TEST_EXPECT(smallVec3B.size() == 0);
#endif // not __clang_analyzer__
                SC_TEST_EXPECT(smallVec3A.size() == 4);
#ifndef __clang_analyzer__
                SC_TEST_EXPECT(smallVec3B.isInline());
#endif // not __clang_analyzer__
            }
            SC_TEST_EXPECT(not smallVec3A.isInline());
            checkItems(smallVec3A, 4);
        }
        if (test_section("construction move SmallVector(stack)->SmallVector(stack)"))
        {
            SmallVector<int, 3> smallVec3A;
            {
                SmallVector<int, 3> smallVec3B;
                addItems(smallVec3B, 3);
                SC_TEST_EXPECT(smallVec3B.size() == 3);
                smallVec3A = move(smallVec3B);
#ifndef __clang_analyzer__
                SC_TEST_EXPECT(smallVec3B.size() == 0);
#endif // not __clang_analyzer__
                SC_TEST_EXPECT(smallVec3A.size() == 3);
#ifndef __clang_analyzer__
                SC_TEST_EXPECT(smallVec3B.isInline());
#endif // not __clang_analyzer__
            }
            SC_TEST_EXPECT(smallVec3A.isInline());
            checkItems(smallVec3A, 3);
        }
        if (test_section("construction move SmallVector(heap)->SmallVector(stack)"))
        {
            SmallVector<int, 4> smallVec4;
            {
                SmallVector<int, 3> smallVec3;
                addItems(smallVec3, 4);
                SC_TEST_EXPECT(smallVec3.size() == 4);
                smallVec4 = move(smallVec3);
                SC_TEST_EXPECT(smallVec3.size() == 0);
                SC_TEST_EXPECT(smallVec4.size() == 4);
#ifndef __clang_analyzer__
                SC_TEST_EXPECT(smallVec3.isInline());
#endif // not __clang_analyzer__
            }
            SC_TEST_EXPECT(not smallVec4.isInline());
            checkItems(smallVec4, 4);
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
