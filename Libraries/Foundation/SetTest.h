// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Testing/Test.h"
#include "Set.h"
#include "String.h"

namespace SC
{
struct SetTest;
}

struct SC::SetTest : public SC::TestCase
{
    SetTest(SC::TestReport& report) : TestCase(report, "SetTest")
    {
        using namespace SC;
        if (test_section("simple"))
        {
            Set<String> setOfStrings;
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
