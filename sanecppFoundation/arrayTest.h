#pragma once
#include "array.h"
#include "test.h"
#include "vector.h"

namespace sanecpp
{
struct ArrayTest;
}

struct sanecpp::ArrayTest : public sanecpp::TestCase
{
    ArrayTest(sanecpp::TestReport& report) : TestCase(report, "ArrayTest")
    {
        using namespace sanecpp;

        const stringView testString("Ciao");

        if (START_SECTION("resize"))
        {
            array<int, 10> arr;
            SANECPP_TEST_EXPECT(not arr.reserve(11));
            SANECPP_TEST_EXPECT(arr.reserve(10));
            SANECPP_TEST_EXPECT(arr.size() == 0);
            SANECPP_TEST_EXPECT(arr.capacity() == 10);
            SANECPP_TEST_EXPECT(arr.resize(10, 3));
            SANECPP_TEST_EXPECT(arr.size() == 10);
            SANECPP_TEST_EXPECT(arr.capacity() == 10);
            size_t numFailures = 0;
            for (auto item : arr)
            {
                if (item != 3)
                {
                    numFailures++;
                }
            }
            SANECPP_TEST_EXPECT(numFailures == 0);
            SANECPP_TEST_EXPECT(arr.resize(1));
            SANECPP_TEST_EXPECT(arr.size() == 1);
            SANECPP_TEST_EXPECT(arr.capacity() == 10);
            SANECPP_TEST_EXPECT(arr.shrink_to_fit());
            SANECPP_TEST_EXPECT(arr.size() == 1);
            SANECPP_TEST_EXPECT(arr.capacity() == 10);
        }

        if (START_SECTION("push_back"))
        {
            array<vector<char>, 10> arr;
            {
                vector<char> str;
                SANECPP_TEST_EXPECT(str.appendCopy(testString.getText(), testString.getLengthInBytes() + 1));
                SANECPP_TEST_EXPECT(arr.push_back(str));
                SANECPP_TEST_EXPECT(arr.push_back(str));
            }
            stringView sv(arr[1].data(), arr[1].size() - 1, true);
            SANECPP_TEST_EXPECT(sv == testString);
            SANECPP_TEST_EXPECT(arr.resize(10));
            SANECPP_TEST_EXPECT(not arr.push_back(arr[0]));
        }
        if (START_SECTION("construction"))
        {
            array<vector<char>, 10> arr;
            vector<char>            str;
            SANECPP_TEST_EXPECT(str.appendCopy(testString.getText(), testString.getLengthInBytes() + 1));

            SANECPP_TEST_EXPECT(arr.resize(2, str));
            array<vector<char>, 11> arr2 = arr;
            SANECPP_TEST_EXPECT(arr2.size() == 2);
            SANECPP_TEST_EXPECT(arr2.capacity() == 11);
            stringView sv(arr2.back().data(), arr2.back().size() - 1, true);
            SANECPP_TEST_EXPECT(sv == testString);

            array<vector<char>, 2> arr3;
            SANECPP_TEST_EXPECT(arr3.appendCopy(arr));
            sv = stringView(arr3.back().data(), arr3.back().size() - 1, true);
            SANECPP_TEST_EXPECT(sv == testString);
        }
        if (START_SECTION("assignment"))
        {
            array<int, 10> myArr1, myArr2;
            SANECPP_TEST_EXPECT(myArr2.resize(5, 5));
            SANECPP_TEST_EXPECT(myArr1.resize(10, 12));
            myArr2 = myArr1;
            SANECPP_TEST_EXPECT(myArr2.size() == 10);
            SANECPP_TEST_EXPECT(myArr2.capacity() == 10);
            size_t numTestsFailed = 0;
            for (size_t idx = 0; idx < 10; ++idx)
            {
                if (myArr2[idx] != 12)
                {
                    numTestsFailed++;
                }
            }
            SANECPP_TEST_EXPECT(numTestsFailed == 0);
            myArr1 = move(myArr2);
        }
    }
};
