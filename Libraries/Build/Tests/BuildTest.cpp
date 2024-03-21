// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Build/Build.h"
#include "../../FileSystem/Path.h"
#include "../../Testing/Testing.h"

namespace SC
{
struct BuildTest;
} // namespace SC

struct SC::BuildTest : public SC::TestCase
{
    BuildTest(SC::TestReport& report) : TestCase(report, "BuildTest")
    {
        using namespace SC;
        StringView projectName      = "SCUnitTest";
        String     targetDirectory  = report.applicationRootDirectory;
        String     sourcesDirectory = report.libraryRootDirectory;
        SC_TRUST_RESULT(Path::append(targetDirectory, {"../..", projectName}, Path::AsPosix));

        Build::Action action;
        action.action           = Build::Action::Configure;
        action.targetDirectory  = targetDirectory.view();
        action.libraryDirectory = sourcesDirectory.view();

        if (test_section("Visual Studio 2022"))
        {
            action.generator = Build::Generator::VisualStudio2022;
            SC_TEST_EXPECT(Build::executeAction(action));
        }
        if (test_section("XCode"))
        {
            action.generator = Build::Generator::XCode;
            SC_TEST_EXPECT(Build::executeAction(action));
        }
        if (test_section("Makefile"))
        {
            action.generator = Build::Generator::Make;
            SC_TEST_EXPECT(Build::executeAction(action));
        }
    }
};

namespace SC
{
void runBuildTest(SC::TestReport& report) { BuildTest test(report); }
} // namespace SC
