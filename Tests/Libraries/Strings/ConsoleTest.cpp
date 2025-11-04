// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Strings/Console.h"
#include "Libraries/Containers/Vector.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Testing/Testing.h"

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

        // Totally optional conversion buffer for UTF conversions on Windows (default is 256 bytes)
        char optionalConversionBuffer[512];

        Console console(optionalConversionBuffer);

        if (test_section("print"))
        {
            String str = StringView("Test Test\n");
            console.print(str.view());
            console.print({});
            SC_TEST_EXPECT(console.print("test {}", 1));
            SC_TEST_EXPECT(not console.print("test {}"_u16, 1));
            SC_TEST_EXPECT(console.print("test {}", StringSpan("1")));
            SC_TEST_EXPECT(not console.print("test {}"_u16, StringSpan("1")));
        }
        if (test_section("printError"))
        {
            console.printError("Test Error\n");
            console.printErrorLine("Test Error Line");
            console.flushStdErr();
        }
    }
};

namespace SC
{
void runConsoleTest(SC::TestReport& report) { ConsoleTest test(report); }
} // namespace SC
//! [testingSnippet]
