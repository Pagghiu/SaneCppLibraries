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

        SmallVector<char, 512 * sizeof(utf_char_t)> consoleBuffer;
        SmallVector<char, 512 * sizeof(utf_char_t)> formatBuffer;

        Console console(consoleBuffer, formatBuffer);

        if (test_section("printAssertion"))
        {
            printAssertion("a!=b", "FileName.cpp", "Function", 12);
        }
        if (test_section("print"))
        {
            String str = StringView("Test Test\n");
            console.print(str.view());
        }
    }
};
