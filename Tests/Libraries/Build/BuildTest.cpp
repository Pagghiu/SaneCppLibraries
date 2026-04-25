// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Tools/SC-build/Build.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Time/Time.h"

namespace SC
{
struct BuildTest;

namespace
{
static constexpr StringView MakefileStaticLibraryWorkspaceName = "MakefileStaticLibraryWorkspace";
static constexpr StringView MakefileStaticLibraryProjectName   = "WriterStaticLibrary";

static Result configureMakefileStaticLibrary(Build::Definition& definition, const Build::Parameters& parameters)
{
    Build::Workspace workspace = {MakefileStaticLibraryWorkspaceName};
    Build::Project   project   = {Build::TargetType::StaticLibrary, MakefileStaticLibraryProjectName};

    SC_TRY(project.setRootDirectory(parameters.directories.projectDirectory.view()));
    SC_TRY(project.addPresetConfiguration(Build::Configuration::Preset::Debug, parameters));
    SC_TRY(project.addPresetConfiguration(Build::Configuration::Preset::Release, parameters));
    SC_TRY(project.addFiles("Tests/SCBuildTest/Fixture/TinyConsoleProgram", "main.cpp"));

    SC_TRY(workspace.projects.push_back(move(project)));
    SC_TRY(definition.workspaces.push_back(move(workspace)));
    return Result(true);
}

static Result computeGeneratedMakefilePath(const Build::Directories& directories, Build::Platform::Type platform,
                                           String& makefilePath)
{
    SC_TRY(
        Path::join(makefilePath, {directories.projectsDirectory.view(), "Make", MakefileStaticLibraryWorkspaceName}));
    if (platform == Build::Platform::Linux)
    {
        SC_TRY(Path::append(makefilePath, {"linux", "Makefile"}, Path::AsPosix));
    }
    else
    {
        SC_TRY(Path::append(makefilePath, {"apple", "Makefile"}, Path::AsPosix));
    }
    return Result(true);
}
} // namespace
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
        SC_TRUST_RESULT(Path::join(directories.buildCacheDirectory, {buildDir.view(), "_BuildCache"}));
        SC_TRUST_RESULT(Path::join(directories.packagesCacheDirectory, {buildDir.view(), "_PackagesCache"}));
        SC_TRUST_RESULT(Path::join(directories.packagesInstallDirectory, {buildDir.view(), "_Packages"}));

        directories.libraryDirectory = report.libraryRootDirectory.view();
        directories.projectDirectory = report.libraryRootDirectory.view();

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

        const bool makefileStaticLibraryMacOS = test_section("Makefile static library (macOS)");
        const bool makefileStaticLibraryLinux = test_section("Makefile static library (Linux)");
        if (makefileStaticLibraryMacOS or makefileStaticLibraryLinux)
        {
            FileSystem fileSystem;
            SC_TRUST_RESULT(fileSystem.init("."));

            Build::Action staticLibraryAction;
            staticLibraryAction.action                  = Build::Action::Configure;
            staticLibraryAction.configurationName       = "Debug";
            staticLibraryAction.workspaceName           = MakefileStaticLibraryWorkspaceName;
            staticLibraryAction.parameters.directories  = directories;
            staticLibraryAction.parameters.generator    = Build::Generator::Make;
            staticLibraryAction.parameters.architecture = Build::Architecture::Any;
            staticLibraryAction.parameters.execution    = action.parameters.execution;
            staticLibraryAction.parameters.toolchain    = action.parameters.toolchain;

            if (makefileStaticLibraryLinux)
            {
                staticLibraryAction.parameters.platform = Build::Platform::Linux;
            }
            else
            {
                staticLibraryAction.parameters.platform = Build::Platform::Apple;
            }

            SC_TEST_EXPECT(Build::Action::execute(staticLibraryAction, configureMakefileStaticLibrary,
                                                  MakefileStaticLibraryWorkspaceName));

            String makefilePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeGeneratedMakefilePath(staticLibraryAction.parameters.directories,
                                                         staticLibraryAction.parameters.platform, makefilePath));
            SC_TEST_EXPECT(fileSystem.existsAndIsFile(makefilePath.view()));

            String makefileContents = StringEncoding::Utf8;
            SC_TRUST_RESULT(fileSystem.read(makefilePath.view(), makefileContents));

            SC_TEST_EXPECT(StringView(makefileContents.view()).containsString("libWriterStaticLibrary.a"));
            SC_TEST_EXPECT(
                StringView(makefileContents.view())
                    .containsString("ar rcs $(WriterStaticLibrary_TARGET_DIR)/$(WriterStaticLibrary_TARGET_NAME) "
                                    "$(WriterStaticLibrary_OBJECT_FILES)"));
            SC_TEST_EXPECT(not StringView(makefileContents.view())
                                   .containsString("$(CXX) -o $(WriterStaticLibrary_TARGET_DIR)/$(WriterStaticLibrary_"
                                                   "TARGET_NAME) $(WriterStaticLibrary_OBJECT_FILES) "
                                                   "$(WriterStaticLibrary_LDFLAGS)"));
            SC_TEST_EXPECT(StringView(makefileContents.view())
                               .containsString("Cannot run static library target 'WriterStaticLibrary'"));

            staticLibraryAction.action      = Build::Action::Run;
            staticLibraryAction.projectName = MakefileStaticLibraryProjectName;
            Result runResult = Build::Action::execute(staticLibraryAction, configureMakefileStaticLibrary,
                                                      MakefileStaticLibraryWorkspaceName);
            SC_TEST_EXPECT(not runResult);
        }
    }
};

namespace SC
{
void runBuildTest(SC::TestReport& report) { BuildTest test(report); }
} // namespace SC
