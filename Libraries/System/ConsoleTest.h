// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Containers/SmallVector.h"
#include "../Foundation/Strings/String.h"
#include "../Testing/Test.h"
#include "Console.h"

namespace SC
{
struct ConsoleTest;
}

struct SC::ConsoleTest : public SC::TestCase
{
    ConsoleTest(SC::TestReport& report) : TestCase(report, "ConsoleTest")
    {
        using namespace SC;

        SmallVector<char, 512 * sizeof(utf_char_t)> consoleConversionBuffer;

        Console console(consoleConversionBuffer);

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
