// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Tools.h"
#include "Libraries/Strings/SmallString.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
namespace SC
{
struct SupportToolsTest : public TestCase
{
    SupportToolsTest(SC::TestReport& report) : TestCase(report, "SupportToolsTest")
    {
        SmallString<256> outputDirectory;
        (void)StringBuilder(outputDirectory).format("{0}/_Build", report.libraryRootDirectory);
        ToolsArguments arguments{report.console, report.libraryRootDirectory, outputDirectory.view()};
        if (test_section("install clang"))
        {
            const char* args[] = {"install"};
            arguments.argv     = args;
            arguments.argc     = sizeof(args) / sizeof(args[0]);
            SC_TEST_EXPECT(runFormatCommand(arguments));
        }
        if (test_section("format with clang-format"))
        {
            const char* args[] = {"execute"};
            arguments.argv     = args;
            arguments.argc     = sizeof(args) / sizeof(args[0]);
            SC_TEST_EXPECT(runFormatCommand(arguments));
        }
    }
};
void runSupportToolsTest(SC::TestReport& report) { SupportToolsTest test(report); }
} // namespace SC
