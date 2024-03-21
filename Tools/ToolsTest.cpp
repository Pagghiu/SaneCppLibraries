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
        using namespace SC::Tools;
        SmallString<256> outputDirectory;
        (void)StringBuilder(outputDirectory).format("{0}/_Build", report.libraryRootDirectory);
        Tool::Arguments arguments{report.console,
                                  report.libraryRootDirectory,
                                  report.libraryRootDirectory,
                                  outputDirectory.view(),
                                  StringView(),
                                  StringView(),
                                  {}};
        if (test_section("clang-format execute"))
        {
            arguments.tool   = "format";
            arguments.action = "execute";
            SC_TEST_EXPECT(runFormatTool(arguments));
        }
        if (test_section("clang-format check"))
        {
            arguments.tool   = "format";
            arguments.action = "check";
            SC_TEST_EXPECT(runFormatTool(arguments));
        }
    }
};
void runSupportToolsTest(SC::TestReport& report) { SupportToolsTest test(report); }
} // namespace SC
