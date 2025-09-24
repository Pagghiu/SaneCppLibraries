// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Containers/VectorSet.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Testing/Testing.h"

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
            simple();
        }
    }

    void simple();
};

void SC::VectorSetTest::simple()
{
    //! [VectorSetSnippet]
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
    //! [VectorSetSnippet]
}

namespace SC
{
void runVectorSetTest(SC::TestReport& report) { VectorSetTest test(report); }
} // namespace SC
