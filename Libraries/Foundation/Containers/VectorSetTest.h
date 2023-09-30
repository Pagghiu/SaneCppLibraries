// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../Testing/Test.h"
#include "../Strings/String.h"
#include "VectorSet.h"

namespace SC
{
struct VectorSetTest;
}

struct SC::VectorSetTest : public SC::TestCase
{
    VectorSetTest(SC::TestReport& report) : TestCase(report, "VectorSetTest")
    {
        using namespace SC;
        if (test_section("simple"))
        {
            VectorSet<String> setOfStrings;
            SC_TEST_EXPECT(setOfStrings.insert("123"_a8));
            SC_TEST_EXPECT(setOfStrings.insert("123"_a8));
            SC_TEST_EXPECT(setOfStrings.contains("123"_a8));
            SC_TEST_EXPECT(setOfStrings.insert("456"_a8));
            SC_TEST_EXPECT(setOfStrings.contains("123"_a8));
            SC_TEST_EXPECT(setOfStrings.contains("456"_a8));
            SC_TEST_EXPECT(setOfStrings.size() == 2);
            SC_TEST_EXPECT(setOfStrings.remove("123"_a8));
            SC_TEST_EXPECT(setOfStrings.size() == 1);
            SC_TEST_EXPECT(setOfStrings.contains("456"_a8));
            SC_TEST_EXPECT(not setOfStrings.contains("123"_a8));

            for (auto& item : setOfStrings)
            {
                SC_TEST_EXPECT(item == "456");
            }
        }
    }
};
