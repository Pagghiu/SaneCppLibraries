// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Build/Build.h"
#include "../../FileSystem/Path.h"
#include "../../Testing/Testing.h"

namespace SC
{
namespace Build
{
// Defined in SC-build.cpp
Result executeAction(Build::Actions::Type action, Build::Generator::Type generator, StringView targetDirectory,
                     StringView sourcesDirectory);
} // namespace Build
} // namespace SC

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

        if (test_section("Visual Studio 2022"))
        {
            SC_TEST_EXPECT(Build::executeAction(Build::Actions::Configure, Build::Generator::VisualStudio2022,
                                                targetDirectory.view(), sourcesDirectory.view()));
        }
        if (test_section("XCode"))
        {
            SC_TEST_EXPECT(Build::executeAction(Build::Actions::Configure, Build::Generator::XCode,
                                                targetDirectory.view(), sourcesDirectory.view()));
        }
        if (test_section("Makefile"))
        {
            SC_TEST_EXPECT(Build::executeAction(Build::Actions::Configure, Build::Generator::Make,
                                                targetDirectory.view(), sourcesDirectory.view()));
        }
    }
};

namespace SC
{
void runBuildTest(SC::TestReport& report) { BuildTest test(report); }
} // namespace SC
