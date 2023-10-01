// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../Testing/Test.h"
#include "../Strings/String.h"
#include "Optional.h"

namespace SC
{
struct OptionalTest;
}

struct SC::OptionalTest : public SC::TestCase
{
    OptionalTest(SC::TestReport& report) : TestCase(report, "OptionalTest")
    {
        using namespace SC;
        if (test_section("Optional"))
        {
            Optional<String> optString = String("ASD");
            SC_TEST_EXPECT(optString.hasValue());
            const String* value;
            SC_TEST_EXPECT(optString.get(value) and value->view() == "ASD");
            String extracted;
            SC_TEST_EXPECT(optString.moveTo(extracted) and extracted.view() == "ASD");
            SC_TEST_EXPECT(not optString.hasValue());
        }
    }
};
