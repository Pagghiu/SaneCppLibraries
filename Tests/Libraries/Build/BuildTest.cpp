// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Tools/SC-build/Build.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Time/Time.h"

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
            String targetDirectory = report.applicationRootDirectory.view();
            SC_TRUST_RESULT(Path::append(targetDirectory, {"../..", "_Tests"}, Path::AsNative));
            SmallString<128> runDirectory;
            SC_TRUST_RESULT(StringBuilder::format(runDirectory, "run-{}-{}", Time::Realtime::now().milliseconds,
                                                  reinterpret_cast<size_t>(this)));
            SC_TRUST_RESULT(Path::append(targetDirectory, {runDirectory.view()}, Path::AsNative));
            // Normalizing is not strictly necessary but it helps when debugging the test
            SC_TRUST_RESULT(Path::normalize(buildDir, targetDirectory.view(), Path::AsNative));
        }
        Build::Action action;
        action.action = Build::Action::Configure;

        Build::Directories& directories = action.parameters.directories;
        SC_TRUST_RESULT(Path::join(directories.projectsDirectory, {buildDir.view(), "_Projects"}));
        SC_TRUST_RESULT(Path::join(directories.outputsDirectory, {buildDir.view(), "_Outputs"}));
        SC_TRUST_RESULT(Path::join(directories.intermediatesDirectory, {buildDir.view(), "_Intermediates"}));
        SC_TRUST_RESULT(Path::join(directories.packagesCacheDirectory, {buildDir.view(), "_PackagesCache"}));
        SC_TRUST_RESULT(Path::join(directories.packagesInstallDirectory, {buildDir.view(), "_Packages"}));

        directories.libraryDirectory = report.libraryRootDirectory.view();

        if (test_section("Visual Studio 2022"))
        {
            action.parameters.generator = Build::Generator::VisualStudio2022;
            action.parameters.platform  = Build::Platform::Windows;
            SC_TEST_EXPECT(Build::executeAction(action));
        }
        if (test_section("XCode"))
        {
            action.parameters.generator = Build::Generator::XCode;
            action.parameters.platform  = Build::Platform::Apple;
            SC_TEST_EXPECT(Build::executeAction(action));
        }
        if (test_section("Makefile (macOS)"))
        {
            action.parameters.generator = Build::Generator::Make;
            action.parameters.platform  = Build::Platform::Apple;
            SC_TEST_EXPECT(Build::executeAction(action));
        }
        if (test_section("Makefile (Linux)"))
        {
            action.parameters.generator = Build::Generator::Make;
            action.parameters.platform  = Build::Platform::Linux;
            SC_TEST_EXPECT(Build::executeAction(action));
        }
    }
};

namespace SC
{
void runBuildTest(SC::TestReport& report) { BuildTest test(report); }
} // namespace SC
