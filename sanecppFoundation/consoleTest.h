#pragma once
#include "console.h"
#include "test.h"

namespace sanecpp
{
struct ConsoleTest;
}

struct sanecpp::ConsoleTest : public sanecpp::TestCase
{
    ConsoleTest(sanecpp::TestReport& report) : TestCase(report, "ConsoleTest")
    {
        using namespace sanecpp;

        if (START_SECTION("printAssertion"))
        {
            printAssertion("a!=b", "FileName.cpp", "Function", 12);
        }
    }
};
