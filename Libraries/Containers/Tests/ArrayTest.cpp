// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../../Containers/Array.h"
#include "../../Algorithms/AlgorithmBubbleSort.h"
#include "../../Containers/Vector.h"
#include "../../Testing/Test.h"

namespace SC
{
struct ArrayTest;
}

struct SC::ArrayTest : public SC::TestCase
{
    ArrayTest(SC::TestReport& report) : TestCase(report, "ArrayTest")
    {
        using namespace SC;

        const StringView testString("Ciao");

        if (test_section("resize"))
        {
            Array<int, 10> arr;
            SC_TEST_EXPECT(not arr.reserve(11));
            SC_TEST_EXPECT(arr.reserve(10));
            SC_TEST_EXPECT(arr.size() == 0);
            SC_TEST_EXPECT(arr.capacity() == 10);
            SC_TEST_EXPECT(arr.resize(10, 3));
            SC_TEST_EXPECT(arr.size() == 10);
            SC_TEST_EXPECT(arr.capacity() == 10);
            size_t numFailures = 0;
            for (auto item : arr)
            {
                if (item != 3)
                {
                    numFailures++;
                }
            }
            SC_TEST_EXPECT(numFailures == 0);
            SC_TEST_EXPECT(arr.resize(1));
            SC_TEST_EXPECT(arr.size() == 1);
            SC_TEST_EXPECT(arr.capacity() == 10);
            SC_TEST_EXPECT(arr.shrink_to_fit());
            SC_TEST_EXPECT(arr.size() == 1);
            SC_TEST_EXPECT(arr.capacity() == 10);
            SC_TEST_EXPECT(arr.pop_front());
            SC_TEST_EXPECT(arr.size() == 0);
        }

        if (test_section("push_back"))
        {
            Array<Vector<char>, 10> arr;
            {
                Vector<char> str;
                SC_TEST_EXPECT(
                    str.append({testString.bytesIncludingTerminator(), testString.sizeInBytesIncludingTerminator()}));
                SC_TEST_EXPECT(arr.push_back(str));
                SC_TEST_EXPECT(arr.push_back(str));
            }
            StringView sv(arr[1].data(), arr[1].size() - 1, true, StringEncoding::Ascii);
            SC_TEST_EXPECT(sv == testString);
            SC_TEST_EXPECT(arr.resize(10));
            SC_TEST_EXPECT(not arr.push_back(arr[0]));
        }
        if (test_section("construction"))
        {
            Array<Vector<char>, 10> arr;
            Vector<char>            str;
            SC_TEST_EXPECT(
                str.append({testString.bytesIncludingTerminator(), testString.sizeInBytesIncludingTerminator()}));

            SC_TEST_EXPECT(arr.resize(2, str));
            Array<Vector<char>, 11> arr2 = arr;
            SC_TEST_EXPECT(arr2.size() == 2);
            SC_TEST_EXPECT(arr2.capacity() == 11);
            StringView sv(arr2.back().data(), arr2.back().size() - 1, true, StringEncoding::Ascii);
            SC_TEST_EXPECT(sv == testString);

            Array<Vector<char>, 2> arr3;
            SC_TEST_EXPECT(arr3.appendMove(arr));
            sv = StringView(arr3.back().data(), arr3.back().size() - 1, true, StringEncoding::Ascii);
            SC_TEST_EXPECT(sv == testString);
        }
        if (test_section("assignment"))
        {
            Array<int, 10> myArr1, myArr2;
            SC_TEST_EXPECT(myArr2.resize(5, 5));
            SC_TEST_EXPECT(myArr1.resize(10, 12));
            myArr2 = myArr1;
            SC_TEST_EXPECT(myArr2.size() == 10);
            SC_TEST_EXPECT(myArr2.capacity() == 10);
            size_t failedComparisons = 0;
            for (size_t idx = 0; idx < 10; ++idx)
            {
                if (myArr2[idx] != 12)
                {
                    failedComparisons++;
                }
            }
            SC_TEST_EXPECT(failedComparisons == 0);
            myArr1 = move(myArr2);
        }
        if (test_section("append"))
        {
            Array<size_t, 3> v0 = {1, 2, 3};
            Array<size_t, 6> v1 = {1, 2, 3};
            Array<size_t, 3> v2 = {4, 5, 6};
            SC_TEST_EXPECT(not v0.append(v2.toSpanConst()));
            SC_TEST_EXPECT(v1.append(v2.toSpanConst()));
            for (size_t idx = 1; idx <= 6; ++idx)
            {
                SC_TEST_EXPECT(v1[idx - 1] == idx);
            }
        }
        if (test_section("sort"))
        {
            Array<int, 3> elements;
            SC_TRUST_RESULT(elements.push_back(1));
            SC_TRUST_RESULT(elements.push_back(0));
            SC_TRUST_RESULT(elements.push_back(2));
            Algorithms::bubbleSort(elements.begin(), elements.end());
            SC_TEST_EXPECT(elements[0] == 0);
            SC_TEST_EXPECT(elements[1] == 1);
            SC_TEST_EXPECT(elements[2] == 2);
        }
    }
};

namespace SC
{
void runArrayTest(SC::TestReport& report) { ArrayTest test(report); }
} // namespace SC
