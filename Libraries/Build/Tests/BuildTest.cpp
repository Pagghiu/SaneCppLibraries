// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../../Build/Build.h"
#include "../../FileSystem/Path.h"
#include "../../Testing/Test.h"

// Include root build file to test project generation
#include "../../../SCBuild.cpp"

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
            SC_TEST_EXPECT(
                SCBuild::generate(Build::Generator::VisualStudio2022, targetDirectory.view(), sourcesDirectory.view()));
        }
        if (test_section("XCode"))
        {
            SC_TEST_EXPECT(
                SCBuild::generate(Build::Generator::XCode14, targetDirectory.view(), sourcesDirectory.view()));
        }
    }
};

namespace SC
{
void runBuildTest(SC::TestReport& report) { BuildTest test(report); }
} // namespace SC
