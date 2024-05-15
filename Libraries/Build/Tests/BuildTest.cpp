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
        String buildDir;
        {
            String             targetDirectory = report.applicationRootDirectory;
            Vector<StringView> components;
            SC_TRUST_RESULT(Path::append(targetDirectory, {"../..", "_Tests"}, Path::AsNative));
            // Normalizing is not strictly necessary but it helps when debugging the test
            SC_TRUST_RESULT(Path::normalize(targetDirectory.view(), components, &buildDir, Path::AsNative));
        }
        Build::Action action;
        action.action = Build::Action::Configure;

        SC_TRUST_RESULT(Path::join(action.directories.projectsDirectory, {buildDir.view(), "_Projects"}));
        SC_TRUST_RESULT(Path::join(action.directories.outputsDirectory, {buildDir.view(), "_Outputs"}));
        SC_TRUST_RESULT(Path::join(action.directories.intermediatesDirectory, {buildDir.view(), "_Intermediates"}));
        SC_TRUST_RESULT(Path::join(action.directories.packagesCacheDirectory, {buildDir.view(), "_PackageCache"}));
        SC_TRUST_RESULT(Path::join(action.directories.packagesInstallDirectory, {buildDir.view(), "_Packages"}));

        action.directories.libraryDirectory = report.libraryRootDirectory;

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
