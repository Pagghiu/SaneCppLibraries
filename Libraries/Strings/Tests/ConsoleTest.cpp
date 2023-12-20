// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Console.h"
#include "../../Containers/SmallVector.h"
#include "../../Strings/String.h"
#include "../../Testing/Testing.h"

//! [testingSnippet]
namespace SC
{
struct ConsoleTest;
}

struct SC::ConsoleTest : public SC::TestCase
{
    ConsoleTest(SC::TestReport& report) : TestCase(report, "ConsoleTest")
    {
        using namespace SC;

        SmallVector<char, 512 * sizeof(native_char_t)> consoleConversionBuffer;

        Console console(consoleConversionBuffer);

        if (test_section("print"))
        {
            String str = StringView("Test Test\n");
            console.print(str.view());
        }
    }
};

namespace SC
{
void runConsoleTest(SC::TestReport& report) { ConsoleTest test(report); }
} // namespace SC
//! [testingSnippet]
