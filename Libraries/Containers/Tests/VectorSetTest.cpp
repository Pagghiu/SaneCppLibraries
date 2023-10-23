// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../VectorSet.h"
#include "../../Strings/String.h"
#include "../../Testing/Test.h"

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
            SC_TEST_EXPECT(setOfStrings.insert("123"));
            SC_TEST_EXPECT(setOfStrings.insert("123"));
            SC_TEST_EXPECT(setOfStrings.contains("123"));
            SC_TEST_EXPECT(setOfStrings.insert("456"));
            SC_TEST_EXPECT(setOfStrings.contains("123"));
            SC_TEST_EXPECT(setOfStrings.contains("456"));
            SC_TEST_EXPECT(setOfStrings.size() == 2);
            SC_TEST_EXPECT(setOfStrings.remove("123"));
            SC_TEST_EXPECT(setOfStrings.size() == 1);
            SC_TEST_EXPECT(setOfStrings.contains("456"));
            SC_TEST_EXPECT(not setOfStrings.contains("123"));

            for (auto& item : setOfStrings)
            {
                SC_TEST_EXPECT(item == "456");
            }
        }
    }
};

namespace SC
{
void runVectorSetTest(SC::TestReport& report) { VectorSetTest test(report); }
} // namespace SC
