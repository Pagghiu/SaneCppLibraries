// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Console.h"
#include "String.h"
#include "Test.h"

namespace SC
{
struct ConsoleTest;
}

struct SC::ConsoleTest : public SC::TestCase
{
    ConsoleTest(SC::TestReport& report) : TestCase(report, "ConsoleTest")
    {
        using namespace SC;

        if (test_section("printAssertion"))
        {
            printAssertion("a!=b", "FileName.cpp", "Function", 12);
        }
        if (test_section("printUTF8"))
        {
            String str = StringView("Test Test\n");
            Console::printUTF8(str);
            Console::printUTF8(str.view());
        }
    }
};
