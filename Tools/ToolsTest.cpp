// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Tools.h"
#include "../Libraries/Strings/SmallString.h"
#include "../Libraries/Strings/StringBuilder.h"
#include "../Libraries/Testing/Testing.h"
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

        StringView args[10];
        if (test_section("coverage"))
        {
            arguments.tool      = "build";
            arguments.action    = "coverage";
            args[0]             = "SCTest";
            args[1]             = "DebugCoverage";
            arguments.arguments = {args, 2};
            SC_TEST_EXPECT(runBuildTool(arguments));
        }
        if (test_section("compile"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "Debug";
            arguments.arguments = {args, 2};
            SC_TEST_EXPECT(runBuildTool(arguments));
        }
        if (test_section("run"))
        {
            arguments.tool      = "build";
            arguments.action    = "run";
            args[0]             = "SCTest";
            args[1]             = "Debug";
            arguments.arguments = {args, 2};
            SC_TEST_EXPECT(runBuildTool(arguments));
        }
        if (test_section("build documentation"))
        {
            arguments.tool      = "build";
            arguments.action    = "documentation";
            arguments.arguments = {};
            SC_TEST_EXPECT(runBuildTool(arguments));
        }
        if (test_section("install doxygen-awesome-css"))
        {
            arguments.tool      = "package";
            arguments.action    = "install";
            args[0]             = "doxygen-awesome-css";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments));
        }
        if (test_section("install doxygen"))
        {
            arguments.tool      = "package";
            arguments.action    = "install";
            args[0]             = "doxygen";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments));
        }
        if (test_section("install clang"))
        {
            arguments.tool      = "package";
            arguments.action    = "install";
            args[0]             = "clang";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments));
        }
        if (test_section("clang-format execute"))
        {
            arguments.tool      = "format";
            arguments.action    = "execute";
            args[0]             = "clang";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runFormatTool(arguments));
        }
        if (test_section("clang-format check"))
        {
            arguments.tool      = "format";
            arguments.action    = "check";
            args[0]             = "clang";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runFormatTool(arguments));
        }
    }
};
void runSupportToolsTest(SC::TestReport& report) { SupportToolsTest test(report); }
} // namespace SC
