// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Process/Process.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/Threading.h"
#include "Libraries/Time/Time.h"
#include "Tools/SC-build/Build.h"

namespace SC
{
namespace
{
static constexpr StringView FixtureWorkspaceName           = "SCBuildFixtures";
static constexpr StringView FixtureProjectName             = "TinyConsoleProgram";
static constexpr StringView HeaderFixtureProjectName       = "HeaderDependencyProgram";
static constexpr StringView CompileFailureProjectName      = "CompileFailureProgram";
static constexpr StringView LinkFailureProjectName         = "LinkFailureProgram";
static constexpr StringView StaticLibraryProjectName       = "FixtureStaticLibrary";
static constexpr StringView StaticLibraryConsumerName      = "StaticLibraryConsumer";
static constexpr StringView WorkspaceLibraryProjectName    = "WorkspaceStaticLibrary";
static constexpr StringView WorkspaceExecutableProjectName = "WorkspaceExecutable";
static constexpr StringView CustomDriverSourceRootName     = "CustomDriverFixture";

static native_char_t DynamicFixtureProjectRootStorage[1024] = {};
static StringView    DynamicFixtureProjectRoot;
static native_char_t DynamicLinkedLibraryPathStorage[1024] = {};
static StringView    DynamicLinkedLibraryPath;

static Build::Platform::Type getBuildPlatform()
{
    switch (HostPlatform)
    {
    case SC::Platform::Apple: return Build::Platform::Apple;
    case SC::Platform::Linux: return Build::Platform::Linux;
    case SC::Platform::Windows: return Build::Platform::Windows;
    case SC::Platform::Emscripten: return Build::Platform::Wasm;
    }
    Assert::unreachable();
}

static Build::Architecture::Type getBuildArchitecture()
{
    switch (HostInstructionSet)
    {
    case InstructionSet::ARM64: return Build::Architecture::Arm64;
    case InstructionSet::Intel64: return Build::Architecture::Intel64;
    case InstructionSet::Intel32: return Build::Architecture::Intel32;
    }
    Assert::unreachable();
}

static Result detectCompilerName(const Build::Toolchain& toolchain, StringView& compilerName)
{
    switch (toolchain.family)
    {
    case Build::Toolchain::Clang: compilerName = "clang"; return Result(true);
    case Build::Toolchain::GCC: compilerName = "gcc"; return Result(true);
    case Build::Toolchain::ZigCC: compilerName = "zigcc"; return Result(true);
    case Build::Toolchain::CustomDriver:
        compilerName = Path::basename(toolchain.compilerCpp.view(), Path::AsNative);
        return Result(true);
    case Build::Toolchain::HostDefault:
        if (HostPlatform == SC::Platform::Apple)
        {
            compilerName = "clang";
            return Result(true);
        }
        {
            Process probeProcess;
            String  output = StringEncoding::Utf8;
            if (probeProcess.exec({"clang++", "--version"}, output) and probeProcess.getExitStatus() == 0)
            {
                compilerName = "clang";
            }
            else
            {
                compilerName = "gcc";
            }
            return Result(true);
        }
    case Build::Toolchain::MSVC:
    case Build::Toolchain::ClangCL: return Result::Error("MSVC-style native backend is not implemented yet");
    }
    Assert::unreachable();
}

static Result createFixtureDirectories(TestReport& report, String& buildRoot, Build::Directories& directories)
{
    String targetDirectory = report.applicationRootDirectory.view();
    SC_TRY(Path::append(targetDirectory, {"../..", "_Tests"}, Path::AsNative));

    SmallString<128> runDirectory;
    SC_TRY(StringBuilder::format(runDirectory, "scbuild-{}-{}", Time::Realtime::now().milliseconds,
                                 reinterpret_cast<size_t>(&report)));
    SC_TRY(Path::append(targetDirectory, {runDirectory.view()}, Path::AsNative));
    SC_TRY(Path::normalize(buildRoot, targetDirectory.view(), Path::AsNative));

    SC_TRY(Path::join(directories.projectsDirectory, {buildRoot.view(), "_Projects"}));
    SC_TRY(Path::join(directories.outputsDirectory, {buildRoot.view(), "_Outputs"}));
    SC_TRY(Path::join(directories.intermediatesDirectory, {buildRoot.view(), "_Intermediates"}));
    SC_TRY(Path::join(directories.buildCacheDirectory, {buildRoot.view(), "_BuildCache"}));
    SC_TRY(Path::join(directories.packagesCacheDirectory, {buildRoot.view(), "_PackagesCache"}));
    SC_TRY(Path::join(directories.packagesInstallDirectory, {buildRoot.view(), "_Packages"}));
    directories.libraryDirectory = report.libraryRootDirectory.view();
    return Result(true);
}

static Result setDynamicFixtureProjectRoot(StringView sourceRoot)
{
    StringSpan::NativeWritable writable = {{DynamicFixtureProjectRootStorage, sizeof(DynamicFixtureProjectRootStorage)},
                                           0};
    SC_TRY(sourceRoot.writeNullTerminatedTo(writable));
    DynamicFixtureProjectRoot = writable.view();
    return Result(true);
}

static Result setDynamicLinkedLibraryPath(StringView libraryPath)
{
    StringSpan::NativeWritable writable = {{DynamicLinkedLibraryPathStorage, sizeof(DynamicLinkedLibraryPathStorage)},
                                           0};
    SC_TRY(libraryPath.writeNullTerminatedTo(writable));
    DynamicLinkedLibraryPath = writable.view();
    return Result(true);
}

static Result resolveHostToolPath(StringView toolName, String& toolPath)
{
    Process process;
    String  output = StringEncoding::Utf8;
    SC_TRY(process.exec({"which", toolName}, output));
    SC_TRY_MSG(process.getExitStatus() == 0, "Cannot locate host tool");
    SC_TRY(toolPath.assign(StringView(output.view()).trimWhiteSpaces()));
    return Result(true);
}

static Result writeToolWrapperScript(FileSystem& fs, StringView scriptPath, StringView logPath, StringView toolPath)
{
    String scriptContents = StringEncoding::Utf8;
    SC_TRY(StringBuilder::format(scriptContents,
                                 "#!/bin/sh\n"
                                 "printf '%s\\n' \"$*\" >> \"{}\"\n"
                                 "exec \"{}\" \"$@\"\n",
                                 logPath, toolPath));
    SC_TRY(fs.writeString(scriptPath, scriptContents.view()));
    SC_TRY(fs.chmod(scriptPath, 0755u));
    return Result(true);
}

static Result configureTinyConsoleProgram(Build::Definition& definition, const Build::Parameters& parameters)
{
    Build::Workspace workspace = {FixtureWorkspaceName};
    Build::Project   project   = {Build::TargetType::ConsoleExecutable, FixtureProjectName};

    SC_TRY(project.setRootDirectory(parameters.directories.libraryDirectory.view()));
    SC_TRY(project.addPresetConfiguration(Build::Configuration::Preset::Debug, parameters));
    SC_TRY(project.addPresetConfiguration(Build::Configuration::Preset::Release, parameters));
    SC_TRY(project.addFiles("Tests/SCBuildTest/Fixture/TinyConsoleProgram", "main.cpp"));

    SC_TRY(workspace.projects.push_back(move(project)));
    SC_TRY(definition.workspaces.push_back(move(workspace)));
    return Result(true);
}

static Result configureDynamicFixtureProgram(Build::Definition& definition, const Build::Parameters& parameters,
                                             Build::TargetType::Type targetType, StringView projectName);

static Result configureDynamicFixtureProgram(Build::Definition& definition, const Build::Parameters& parameters,
                                             StringView projectName);

static Result configureHeaderDependencyProgram(Build::Definition& definition, const Build::Parameters& parameters)
{
    return configureDynamicFixtureProgram(definition, parameters, HeaderFixtureProjectName);
}

static Result configureCompileFailureProgram(Build::Definition& definition, const Build::Parameters& parameters)
{
    return configureDynamicFixtureProgram(definition, parameters, CompileFailureProjectName);
}

static Result configureLinkFailureProgram(Build::Definition& definition, const Build::Parameters& parameters)
{
    return configureDynamicFixtureProgram(definition, parameters, LinkFailureProjectName);
}

static Result configureDynamicFixtureProgram(Build::Definition& definition, const Build::Parameters& parameters,
                                             StringView projectName)
{
    return configureDynamicFixtureProgram(definition, parameters, Build::TargetType::ConsoleExecutable, projectName);
}

static Result configureStaticLibraryProgram(Build::Definition& definition, const Build::Parameters& parameters)
{
    return configureDynamicFixtureProgram(definition, parameters, Build::TargetType::StaticLibrary,
                                          StaticLibraryProjectName);
}

static Result configureStaticLibraryConsumerProgram(Build::Definition& definition, const Build::Parameters& parameters)
{
    SC_TRY_MSG(not DynamicFixtureProjectRoot.isEmpty(), "Dynamic fixture root is not initialized");
    SC_TRY_MSG(not DynamicLinkedLibraryPath.isEmpty(), "Static library path is not initialized");

    Build::Workspace workspace = {FixtureWorkspaceName};
    Build::Project   project   = {Build::TargetType::ConsoleExecutable, StaticLibraryConsumerName};

    SC_TRY(project.setRootDirectory(DynamicFixtureProjectRoot));
    SC_TRY(project.addPresetConfiguration(Build::Configuration::Preset::Debug, parameters));
    SC_TRY(project.addPresetConfiguration(Build::Configuration::Preset::Release, parameters));
    SC_TRY(project.addLinkLibraries({DynamicLinkedLibraryPath}));
    SC_TRY(project.addFiles(".", "*.cpp"));

    SC_TRY(workspace.projects.push_back(move(project)));
    SC_TRY(definition.workspaces.push_back(move(workspace)));
    return Result(true);
}

static Result configureWorkspaceDependencyProgram(Build::Definition& definition, const Build::Parameters& parameters)
{
    SC_TRY_MSG(not DynamicFixtureProjectRoot.isEmpty(), "Dynamic fixture root is not initialized");

    Build::Workspace workspace = {FixtureWorkspaceName};

    String libraryRoot    = StringEncoding::Utf8;
    String executableRoot = StringEncoding::Utf8;
    SC_TRY(Path::join(libraryRoot, {DynamicFixtureProjectRoot, "Library"}));
    SC_TRY(Path::join(executableRoot, {DynamicFixtureProjectRoot, "Executable"}));

    Build::Project libraryProject = {Build::TargetType::StaticLibrary, WorkspaceLibraryProjectName};
    SC_TRY(libraryProject.setRootDirectory(libraryRoot.view()));
    SC_TRY(libraryProject.addPresetConfiguration(Build::Configuration::Preset::Debug, parameters));
    SC_TRY(libraryProject.addPresetConfiguration(Build::Configuration::Preset::Release, parameters));
    SC_TRY(libraryProject.addFiles(".", "*.cpp"));

    Build::Project executableProject = {Build::TargetType::ConsoleExecutable, WorkspaceExecutableProjectName};
    SC_TRY(executableProject.setRootDirectory(executableRoot.view()));
    SC_TRY(executableProject.addPresetConfiguration(Build::Configuration::Preset::Debug, parameters));
    SC_TRY(executableProject.addPresetConfiguration(Build::Configuration::Preset::Release, parameters));
    SC_TRY(executableProject.addLinkLibraries({WorkspaceLibraryProjectName}));
    SC_TRY(executableProject.addFiles(".", "*.cpp"));

    SC_TRY(workspace.projects.push_back(move(libraryProject)));
    SC_TRY(workspace.projects.push_back(move(executableProject)));
    SC_TRY(definition.workspaces.push_back(move(workspace)));
    return Result(true);
}

static Result configureCustomDriverDependencyProgram(Build::Definition& definition, const Build::Parameters& parameters)
{
    SC_TRY_MSG(not DynamicFixtureProjectRoot.isEmpty(), "Dynamic fixture root is not initialized");

    Build::Workspace workspace = {FixtureWorkspaceName};

    String libraryRoot    = StringEncoding::Utf8;
    String executableRoot = StringEncoding::Utf8;
    SC_TRY(Path::join(libraryRoot, {DynamicFixtureProjectRoot, "Library"}));
    SC_TRY(Path::join(executableRoot, {DynamicFixtureProjectRoot, "Executable"}));

    Build::Project libraryProject = {Build::TargetType::StaticLibrary, WorkspaceLibraryProjectName};
    SC_TRY(libraryProject.setRootDirectory(libraryRoot.view()));
    SC_TRY(libraryProject.addPresetConfiguration(Build::Configuration::Preset::Debug, parameters));
    SC_TRY(libraryProject.addPresetConfiguration(Build::Configuration::Preset::Release, parameters));
    SC_TRY(libraryProject.addFiles(".", "*.c"));
    SC_TRY(libraryProject.addFiles(".", "*.cpp"));

    Build::Project executableProject = {Build::TargetType::ConsoleExecutable, WorkspaceExecutableProjectName};
    SC_TRY(executableProject.setRootDirectory(executableRoot.view()));
    SC_TRY(executableProject.addPresetConfiguration(Build::Configuration::Preset::Debug, parameters));
    SC_TRY(executableProject.addPresetConfiguration(Build::Configuration::Preset::Release, parameters));
    SC_TRY(executableProject.addLinkLibraries({WorkspaceLibraryProjectName}));
    SC_TRY(executableProject.addFiles(".", "*.cpp"));

    SC_TRY(workspace.projects.push_back(move(libraryProject)));
    SC_TRY(workspace.projects.push_back(move(executableProject)));
    SC_TRY(definition.workspaces.push_back(move(workspace)));
    return Result(true);
}

static Result configureDynamicFixtureProgram(Build::Definition& definition, const Build::Parameters& parameters,
                                             Build::TargetType::Type targetType, StringView projectName)
{
    SC_TRY_MSG(not DynamicFixtureProjectRoot.isEmpty(), "Dynamic fixture root is not initialized");

    Build::Workspace workspace = {FixtureWorkspaceName};
    Build::Project   project   = {targetType, projectName};

    SC_TRY(project.setRootDirectory(DynamicFixtureProjectRoot));
    SC_TRY(project.addPresetConfiguration(Build::Configuration::Preset::Debug, parameters));
    SC_TRY(project.addPresetConfiguration(Build::Configuration::Preset::Release, parameters));
    SC_TRY(project.addIncludePaths({"."}));
    SC_TRY(project.addFiles(".", "*.cpp"));

    SC_TRY(workspace.projects.push_back(move(project)));
    SC_TRY(definition.workspaces.push_back(move(workspace)));
    return Result(true);
}

static Result computeBuildDirectoryName(const Build::Action& action, String& buildDirectory)
{
    StringView compilerName;
    SC_TRY(detectCompilerName(action.parameters.toolchain, compilerName));

    StringView targetOS;
    switch (action.parameters.platform)
    {
    case Build::Platform::Apple: targetOS = "macOS"; break;
    case Build::Platform::Linux: targetOS = "linux"; break;
    case Build::Platform::Windows: targetOS = "windows"; break;
    case Build::Platform::Wasm: targetOS = "wasm"; break;
    case Build::Platform::Unknown: return Result::Error("Unknown platform");
    }

    StringView targetArchitecture;
    switch (action.parameters.architecture)
    {
    case Build::Architecture::Arm64: targetArchitecture = "arm64"; break;
    case Build::Architecture::Intel64: targetArchitecture = "x86_64"; break;
    case Build::Architecture::Intel32: targetArchitecture = "x86"; break;
    case Build::Architecture::Wasm: targetArchitecture = "wasm32"; break;
    case Build::Architecture::Any: targetArchitecture = "Any"; break;
    }

    SC_TRY(StringBuilder::format(buildDirectory, "{}-{}-Native-{}-{}", targetOS, targetArchitecture, compilerName,
                                 action.configurationName));
    return Result(true);
}

static Result computeArtifactPath(const Build::Action& action, StringView projectName,
                                  Build::TargetType::Type targetType, String& executablePath)
{
    String buildDirectory = StringEncoding::Utf8;
    SC_TRY(computeBuildDirectoryName(action, buildDirectory));

    String artifactName = StringEncoding::Utf8;
    switch (targetType)
    {
    case Build::TargetType::ConsoleExecutable:
    case Build::TargetType::GUIApplication: SC_TRY(artifactName.assign(projectName)); break;
    case Build::TargetType::StaticLibrary:
        if (projectName.startsWith("lib"))
        {
            SC_TRY(StringBuilder::format(artifactName, "{}.a", projectName));
        }
        else
        {
            SC_TRY(StringBuilder::format(artifactName, "lib{}.a", projectName));
        }
        break;
    }

    SC_TRY(Path::join(executablePath, {action.parameters.directories.outputsDirectory.view(), buildDirectory.view(),
                                       artifactName.view()}));
    return Result(true);
}

static Result computeExecutablePath(const Build::Action& action, StringView projectName, String& executablePath)
{
    return computeArtifactPath(action, projectName, Build::TargetType::ConsoleExecutable, executablePath);
}

static Result computeObjectPath(const Build::Action& action, StringView projectName, StringView sourceName,
                                String& objectPath)
{
    String buildDirectory = StringEncoding::Utf8;
    SC_TRY(computeBuildDirectoryName(action, buildDirectory));

    String objectName = StringEncoding::Utf8;
    SC_TRY(StringBuilder::format(objectName, "{}.o", sourceName));
    SC_TRY(Path::join(objectPath, {action.parameters.directories.intermediatesDirectory.view(), projectName,
                                   buildDirectory.view(), objectName.view()}));
    return Result(true);
}

static Result writeHeaderDependencyFixture(FileSystem& fs, StringView sourceRoot, StringView message)
{
    SC_TRY(fs.makeDirectoryRecursive(sourceRoot));

    String headerPath = StringEncoding::Utf8;
    String sourcePath = StringEncoding::Utf8;
    SC_TRY(Path::join(headerPath, {sourceRoot, "shared.h"}));
    SC_TRY(Path::join(sourcePath, {sourceRoot, "main.cpp"}));

    String headerContents = StringEncoding::Utf8;
    SC_TRY(StringBuilder::format(headerContents, "#pragma once\n#define FIXTURE_MESSAGE \"{}\"\n", message));
    SC_TRY(fs.writeString(headerPath.view(), headerContents.view()));

    static constexpr StringView sourceContents = "#include <stdio.h>\n"
                                                 "#include \"shared.h\"\n"
                                                 "\n"
                                                 "int main()\n"
                                                 "{\n"
                                                 "    puts(FIXTURE_MESSAGE);\n"
                                                 "    return 0;\n"
                                                 "}\n";
    SC_TRY(fs.writeString(sourcePath.view(), sourceContents));
    return Result(true);
}

static Result writeSourceFixture(FileSystem& fs, StringView sourceRoot, StringView sourceContents)
{
    SC_TRY(fs.makeDirectoryRecursive(sourceRoot));

    String sourcePath = StringEncoding::Utf8;
    SC_TRY(Path::join(sourcePath, {sourceRoot, "main.cpp"}));
    SC_TRY(fs.writeString(sourcePath.view(), sourceContents));
    return Result(true);
}

static Result writeStaticLibraryFixture(FileSystem& fs, StringView sourceRoot)
{
    SC_TRY(fs.makeDirectoryRecursive(sourceRoot));

    String sourcePath = StringEncoding::Utf8;
    SC_TRY(Path::join(sourcePath, {sourceRoot, "library.cpp"}));
    SC_TRY(fs.writeString(sourcePath.view(), "int fixture_library_value()\n"
                                             "{\n"
                                             "    return 42;\n"
                                             "}\n"));
    return Result(true);
}

static Result writeStaticLibraryConsumerFixture(FileSystem& fs, StringView sourceRoot)
{
    SC_TRY(fs.makeDirectoryRecursive(sourceRoot));

    String sourcePath = StringEncoding::Utf8;
    SC_TRY(Path::join(sourcePath, {sourceRoot, "main.cpp"}));
    SC_TRY(fs.writeString(sourcePath.view(), "#include <stdio.h>\n"
                                             "\n"
                                             "int fixture_library_value();\n"
                                             "\n"
                                             "int main()\n"
                                             "{\n"
                                             "    printf(\"%d\\n\", fixture_library_value());\n"
                                             "    return 0;\n"
                                             "}\n"));
    return Result(true);
}

static Result writeWorkspaceDependencyFixture(FileSystem& fs, StringView sourceRoot)
{
    String libraryRoot      = StringEncoding::Utf8;
    String executableRoot   = StringEncoding::Utf8;
    String librarySource    = StringEncoding::Utf8;
    String executableSource = StringEncoding::Utf8;
    SC_TRY(Path::join(libraryRoot, {sourceRoot, "Library"}));
    SC_TRY(Path::join(executableRoot, {sourceRoot, "Executable"}));
    SC_TRY(fs.makeDirectoryRecursive(libraryRoot.view()));
    SC_TRY(fs.makeDirectoryRecursive(executableRoot.view()));

    SC_TRY(Path::join(librarySource, {libraryRoot.view(), "library.cpp"}));
    SC_TRY(fs.writeString(librarySource.view(), "int workspace_dependency_value()\n"
                                                "{\n"
                                                "    return 7;\n"
                                                "}\n"));

    SC_TRY(Path::join(executableSource, {executableRoot.view(), "main.cpp"}));
    SC_TRY(fs.writeString(executableSource.view(), "#include <stdio.h>\n"
                                                   "\n"
                                                   "int workspace_dependency_value();\n"
                                                   "\n"
                                                   "int main()\n"
                                                   "{\n"
                                                   "    printf(\"%d\\n\", workspace_dependency_value());\n"
                                                   "    return 0;\n"
                                                   "}\n"));
    return Result(true);
}

static Result writeCustomDriverFixture(FileSystem& fs, StringView sourceRoot)
{
    String libraryRoot      = StringEncoding::Utf8;
    String executableRoot   = StringEncoding::Utf8;
    String libraryCSource   = StringEncoding::Utf8;
    String libraryCppSource = StringEncoding::Utf8;
    String executableSource = StringEncoding::Utf8;
    SC_TRY(Path::join(libraryRoot, {sourceRoot, "Library"}));
    SC_TRY(Path::join(executableRoot, {sourceRoot, "Executable"}));
    SC_TRY(fs.makeDirectoryRecursive(libraryRoot.view()));
    SC_TRY(fs.makeDirectoryRecursive(executableRoot.view()));

    SC_TRY(Path::join(libraryCSource, {libraryRoot.view(), "helper.c"}));
    SC_TRY(fs.writeString(libraryCSource.view(), "int custom_driver_c_value(void)\n"
                                                 "{\n"
                                                 "    return 5;\n"
                                                 "}\n"));

    SC_TRY(Path::join(libraryCppSource, {libraryRoot.view(), "library.cpp"}));
    SC_TRY(fs.writeString(libraryCppSource.view(), "extern \"C\" int custom_driver_c_value(void);\n"
                                                   "\n"
                                                   "int workspace_dependency_value()\n"
                                                   "{\n"
                                                   "    return custom_driver_c_value() + 8;\n"
                                                   "}\n"));

    SC_TRY(Path::join(executableSource, {executableRoot.view(), "main.cpp"}));
    SC_TRY(fs.writeString(executableSource.view(), "#include <stdio.h>\n"
                                                   "\n"
                                                   "int workspace_dependency_value();\n"
                                                   "\n"
                                                   "int main()\n"
                                                   "{\n"
                                                   "    printf(\"%d\\n\", workspace_dependency_value());\n"
                                                   "    return 0;\n"
                                                   "}\n"));
    return Result(true);
}

static Build::Action makeNativeCompileAction(const Build::Directories& directories, StringView projectName)
{
    Build::Action action;
    action.action                  = Build::Action::Compile;
    action.workspaceName           = FixtureWorkspaceName;
    action.projectName             = projectName;
    action.configurationName       = "Debug";
    action.parameters.generator    = Build::Generator::Native;
    action.parameters.platform     = getBuildPlatform();
    action.parameters.architecture = getBuildArchitecture();
    action.parameters.directories  = directories;
    return action;
}

static Result runBuiltProgram(StringView executablePath, String& stdoutOutput)
{
    StringSpan processArguments[] = {executablePath};
    Process    process;
    SC_TRY(process.exec({processArguments, 1}, stdoutOutput));
    SC_TRY_MSG(process.getExitStatus() == 0, "Fixture program exited with non-zero status");
    return Result(true);
}
} // namespace

struct SCBuildFixtureTest : public SC::TestCase
{
    SCBuildFixtureTest(SC::TestReport& report) : TestCase(report, "SCBuildTest")
    {
        String fixtureDirectory = report.libraryRootDirectory.view();
        SC_TRUST_RESULT(
            Path::append(fixtureDirectory, {"Tests", "SCBuildTest", "Fixture", "TinyConsoleProgram"}, Path::AsNative));

        if (test_section("fixture layout"))
        {
            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            SC_TEST_EXPECT(fs.exists(fixtureDirectory.view()));

            String mainSource = fixtureDirectory.view();
            SC_TRUST_RESULT(Path::append(mainSource, {"main.cpp"}, Path::AsNative));
            SC_TEST_EXPECT(fs.exists(mainSource.view()));

            String expectedOutput = fixtureDirectory.view();
            SC_TRUST_RESULT(Path::append(expectedOutput, {"stdout.txt"}, Path::AsNative));
            SC_TEST_EXPECT(fs.exists(expectedOutput.view()));
        }

        if (test_section("native backend builds and runs fixture"))
        {
            SC_TRUST_RESULT(HostPlatform == SC::Platform::Apple or HostPlatform == SC::Platform::Linux
                                ? Result(true)
                                : Result::Error("Native backend fixture test is POSIX-only for now"));

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            Build::Action action = makeNativeCompileAction(directories, FixtureProjectName);

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram, FixtureWorkspaceName));

            String executablePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, FixtureProjectName, executablePath));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));
            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));

            String stdoutOutput = StringEncoding::Utf8;
            SC_TEST_EXPECT(runBuiltProgram(executablePath.view(), stdoutOutput));

            String expectedOutput = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read("Tests/SCBuildTest/Fixture/TinyConsoleProgram/stdout.txt", expectedOutput));
            SC_TEST_EXPECT(stdoutOutput == expectedOutput.view());
        }

        if (test_section("native backend skips up-to-date work in verbose mode"))
        {
            SC_TRUST_RESULT(HostPlatform == SC::Platform::Apple or HostPlatform == SC::Platform::Linux
                                ? Result(true)
                                : Result::Error("Native backend fixture test is POSIX-only for now"));

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            Build::Action action                = makeNativeCompileAction(directories, FixtureProjectName);
            action.parameters.execution.verbose = true;

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram, FixtureWorkspaceName));

            String executablePath = StringEncoding::Utf8;
            String objectPath     = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, FixtureProjectName, executablePath));
            SC_TRUST_RESULT(computeObjectPath(action, FixtureProjectName,
                                              "Tests/SCBuildTest/Fixture/TinyConsoleProgram/main.cpp", objectPath));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            FileSystem::FileStat initialObjectStat;
            FileSystem::FileStat initialExecutableStat;
            SC_TRUST_RESULT(fs.stat(objectPath.view(), initialObjectStat));
            SC_TRUST_RESULT(fs.stat(executablePath.view(), initialExecutableStat));

            Thread::Sleep(20);
            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram, FixtureWorkspaceName));

            FileSystem::FileStat finalObjectStat;
            FileSystem::FileStat finalExecutableStat;
            SC_TRUST_RESULT(fs.stat(objectPath.view(), finalObjectStat));
            SC_TRUST_RESULT(fs.stat(executablePath.view(), finalExecutableStat));

            SC_TEST_EXPECT(finalObjectStat.modifiedTime.milliseconds == initialObjectStat.modifiedTime.milliseconds);
            SC_TEST_EXPECT(finalExecutableStat.modifiedTime.milliseconds ==
                           initialExecutableStat.modifiedTime.milliseconds);
        }

        if (test_section("native backend rebuilds after header change"))
        {
            SC_TRUST_RESULT(HostPlatform == SC::Platform::Apple or HostPlatform == SC::Platform::Linux
                                ? Result(true)
                                : Result::Error("Native backend fixture test is POSIX-only for now"));

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String sourceRoot = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(sourceRoot, {buildRoot.view(), "HeaderDependencyFixture"}));
            SC_TRUST_RESULT(writeHeaderDependencyFixture(fs, sourceRoot.view(), "first"));
            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(sourceRoot.view()));

            Build::Action action = makeNativeCompileAction(directories, HeaderFixtureProjectName);

            SC_TEST_EXPECT(Build::Action::execute(action, configureHeaderDependencyProgram, FixtureWorkspaceName));

            String executablePath = StringEncoding::Utf8;
            String objectPath     = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, HeaderFixtureProjectName, executablePath));
            SC_TRUST_RESULT(computeObjectPath(action, HeaderFixtureProjectName, "main.cpp", objectPath));
            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));
            SC_TEST_EXPECT(fs.existsAndIsFile(objectPath.view()));

            String stdoutOutput = StringEncoding::Utf8;
            SC_TEST_EXPECT(runBuiltProgram(executablePath.view(), stdoutOutput));
            SC_TEST_EXPECT(stdoutOutput == "first\n");

            Thread::Sleep(20);
            SC_TRUST_RESULT(writeHeaderDependencyFixture(fs, sourceRoot.view(), "second"));
            {
                String headerPath = StringEncoding::Utf8;
                SC_TRUST_RESULT(Path::join(headerPath, {sourceRoot.view(), "shared.h"}));
                SC_TRUST_RESULT(
                    fs.setLastModifiedTime(headerPath.view(), TimeMs{Time::Realtime::now().milliseconds + 2000}));
            }

            SC_TEST_EXPECT(Build::Action::execute(action, configureHeaderDependencyProgram, FixtureWorkspaceName));

            SC_TEST_EXPECT(fs.existsAndIsFile(objectPath.view()));
            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));

            stdoutOutput = "";
            SC_TEST_EXPECT(runBuiltProgram(executablePath.view(), stdoutOutput));
            SC_TEST_EXPECT(stdoutOutput == "second\n");
        }

        if (test_section("native backend reports compile failures"))
        {
            SC_TRUST_RESULT(HostPlatform == SC::Platform::Apple or HostPlatform == SC::Platform::Linux
                                ? Result(true)
                                : Result::Error("Native backend fixture test is POSIX-only for now"));

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String sourceRoot = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(sourceRoot, {buildRoot.view(), "CompileFailureFixture"}));
            SC_TRUST_RESULT(writeSourceFixture(fs, sourceRoot.view(),
                                               "#include <stdio.h>\n"
                                               "int main()\n"
                                               "{\n"
                                               "    this_will_not_compile(\n"
                                               "}\n"));
            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(sourceRoot.view()));

            Build::Action action = makeNativeCompileAction(directories, CompileFailureProjectName);

            Result compileResult = Build::Action::execute(action, configureCompileFailureProgram, FixtureWorkspaceName);
            SC_TEST_EXPECT(not compileResult);

            String executablePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, CompileFailureProjectName, executablePath));
            SC_TEST_EXPECT(not fs.existsAndIsFile(executablePath.view()));
        }

        if (test_section("native backend reports link failures"))
        {
            SC_TRUST_RESULT(HostPlatform == SC::Platform::Apple or HostPlatform == SC::Platform::Linux
                                ? Result(true)
                                : Result::Error("Native backend fixture test is POSIX-only for now"));

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String sourceRoot = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(sourceRoot, {buildRoot.view(), "LinkFailureFixture"}));
            SC_TRUST_RESULT(writeSourceFixture(fs, sourceRoot.view(),
                                               "extern int missing_symbol();\n"
                                               "\n"
                                               "int main()\n"
                                               "{\n"
                                               "    return missing_symbol();\n"
                                               "}\n"));
            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(sourceRoot.view()));

            Build::Action action = makeNativeCompileAction(directories, LinkFailureProjectName);

            Result compileResult = Build::Action::execute(action, configureLinkFailureProgram, FixtureWorkspaceName);
            SC_TEST_EXPECT(not compileResult);

            String executablePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, LinkFailureProjectName, executablePath));
            SC_TEST_EXPECT(not fs.existsAndIsFile(executablePath.view()));
        }

        if (test_section("native backend builds static libraries"))
        {
            SC_TRUST_RESULT(HostPlatform == SC::Platform::Apple or HostPlatform == SC::Platform::Linux
                                ? Result(true)
                                : Result::Error("Native backend fixture test is POSIX-only for now"));

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String libraryRoot  = StringEncoding::Utf8;
            String consumerRoot = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(libraryRoot, {buildRoot.view(), "StaticLibraryFixture"}));
            SC_TRUST_RESULT(Path::join(consumerRoot, {buildRoot.view(), "StaticLibraryConsumerFixture"}));
            SC_TRUST_RESULT(writeStaticLibraryFixture(fs, libraryRoot.view()));
            SC_TRUST_RESULT(writeStaticLibraryConsumerFixture(fs, consumerRoot.view()));

            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(libraryRoot.view()));
            Build::Action libraryAction = makeNativeCompileAction(directories, StaticLibraryProjectName);
            SC_TEST_EXPECT(Build::Action::execute(libraryAction, configureStaticLibraryProgram, FixtureWorkspaceName));

            String libraryPath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeArtifactPath(libraryAction, StaticLibraryProjectName,
                                                Build::TargetType::StaticLibrary, libraryPath));
            SC_TEST_EXPECT(fs.existsAndIsFile(libraryPath.view()));

            SC_TRUST_RESULT(setDynamicLinkedLibraryPath(libraryPath.view()));
            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(consumerRoot.view()));

            Build::Action consumerAction = makeNativeCompileAction(directories, StaticLibraryConsumerName);
            SC_TEST_EXPECT(
                Build::Action::execute(consumerAction, configureStaticLibraryConsumerProgram, FixtureWorkspaceName));

            String executablePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(consumerAction, StaticLibraryConsumerName, executablePath));
            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));

            String stdoutOutput = StringEncoding::Utf8;
            SC_TEST_EXPECT(runBuiltProgram(executablePath.view(), stdoutOutput));
            SC_TEST_EXPECT(stdoutOutput == "42\n");
        }

        if (test_section("native backend builds workspace dependencies first"))
        {
            SC_TRUST_RESULT(HostPlatform == SC::Platform::Apple or HostPlatform == SC::Platform::Linux
                                ? Result(true)
                                : Result::Error("Native backend fixture test is POSIX-only for now"));

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String sourceRoot = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(sourceRoot, {buildRoot.view(), "WorkspaceDependencyFixture"}));
            SC_TRUST_RESULT(writeWorkspaceDependencyFixture(fs, sourceRoot.view()));
            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(sourceRoot.view()));

            Build::Action action = makeNativeCompileAction(directories, WorkspaceExecutableProjectName);
            SC_TEST_EXPECT(Build::Action::execute(action, configureWorkspaceDependencyProgram, FixtureWorkspaceName));

            String executablePath = StringEncoding::Utf8;
            String libraryPath    = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, WorkspaceExecutableProjectName, executablePath));
            SC_TRUST_RESULT(computeArtifactPath(action, WorkspaceLibraryProjectName, Build::TargetType::StaticLibrary,
                                                libraryPath));

            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));
            SC_TEST_EXPECT(fs.existsAndIsFile(libraryPath.view()));

            String stdoutOutput = StringEncoding::Utf8;
            SC_TEST_EXPECT(runBuiltProgram(executablePath.view(), stdoutOutput));
            SC_TEST_EXPECT(stdoutOutput == "7\n");
        }

        if (test_section("native backend routes custom driver toolchains"))
        {
            SC_TRUST_RESULT(HostPlatform == SC::Platform::Apple or HostPlatform == SC::Platform::Linux
                                ? Result(true)
                                : Result::Error("Custom-driver fixture test is POSIX-only for now"));

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String sourceRoot    = StringEncoding::Utf8;
            String toolchainRoot = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(sourceRoot, {buildRoot.view(), CustomDriverSourceRootName}));
            SC_TRUST_RESULT(Path::join(toolchainRoot, {sourceRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(writeCustomDriverFixture(fs, sourceRoot.view()));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolchainRoot.view()));
            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(sourceRoot.view()));

            String hostCompilerC   = StringEncoding::Utf8;
            String hostCompilerCpp = StringEncoding::Utf8;
            String hostArchiver    = StringEncoding::Utf8;
            SC_TRUST_RESULT(resolveHostToolPath("clang", hostCompilerC));
            SC_TRUST_RESULT(resolveHostToolPath("clang++", hostCompilerCpp));
            SC_TRUST_RESULT(resolveHostToolPath("ar", hostArchiver));

            String compilerCLogPath   = StringEncoding::Utf8;
            String compilerCppLogPath = StringEncoding::Utf8;
            String archiverLogPath    = StringEncoding::Utf8;
            String compilerCWrapper   = StringEncoding::Utf8;
            String compilerCppWrapper = StringEncoding::Utf8;
            String archiverWrapper    = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(compilerCLogPath, {toolchainRoot.view(), "compiler-c.log"}));
            SC_TRUST_RESULT(Path::join(compilerCppLogPath, {toolchainRoot.view(), "compiler-cpp.log"}));
            SC_TRUST_RESULT(Path::join(archiverLogPath, {toolchainRoot.view(), "archiver.log"}));
            SC_TRUST_RESULT(Path::join(compilerCWrapper, {toolchainRoot.view(), "compiler-c.sh"}));
            SC_TRUST_RESULT(Path::join(compilerCppWrapper, {toolchainRoot.view(), "compiler-cpp.sh"}));
            SC_TRUST_RESULT(Path::join(archiverWrapper, {toolchainRoot.view(), "archiver.sh"}));

            SC_TRUST_RESULT(fs.writeString(compilerCLogPath.view(), ""));
            SC_TRUST_RESULT(fs.writeString(compilerCppLogPath.view(), ""));
            SC_TRUST_RESULT(fs.writeString(archiverLogPath.view(), ""));
            SC_TRUST_RESULT(
                writeToolWrapperScript(fs, compilerCWrapper.view(), compilerCLogPath.view(), hostCompilerC.view()));
            SC_TRUST_RESULT(writeToolWrapperScript(fs, compilerCppWrapper.view(), compilerCppLogPath.view(),
                                                   hostCompilerCpp.view()));
            SC_TRUST_RESULT(
                writeToolWrapperScript(fs, archiverWrapper.view(), archiverLogPath.view(), hostArchiver.view()));

            Build::Action action               = makeNativeCompileAction(directories, WorkspaceExecutableProjectName);
            action.parameters.toolchain.family = Build::Toolchain::CustomDriver;
            SC_TRUST_RESULT(action.parameters.toolchain.compilerC.assign(compilerCWrapper.view()));
            SC_TRUST_RESULT(action.parameters.toolchain.compilerCpp.assign(compilerCppWrapper.view()));
            SC_TRUST_RESULT(action.parameters.toolchain.linker.assign(compilerCppWrapper.view()));
            SC_TRUST_RESULT(action.parameters.toolchain.archiver.assign(archiverWrapper.view()));

            SC_TEST_EXPECT(
                Build::Action::execute(action, configureCustomDriverDependencyProgram, FixtureWorkspaceName));

            String executablePath = StringEncoding::Utf8;
            String libraryPath    = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, WorkspaceExecutableProjectName, executablePath));
            SC_TRUST_RESULT(computeArtifactPath(action, WorkspaceLibraryProjectName, Build::TargetType::StaticLibrary,
                                                libraryPath));
            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));
            SC_TEST_EXPECT(fs.existsAndIsFile(libraryPath.view()));

            String stdoutOutput = StringEncoding::Utf8;
            SC_TEST_EXPECT(runBuiltProgram(executablePath.view(), stdoutOutput));
            SC_TEST_EXPECT(stdoutOutput == "13\n");

            String compilerCLog   = StringEncoding::Utf8;
            String compilerCppLog = StringEncoding::Utf8;
            String archiverLog    = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(compilerCLogPath.view(), compilerCLog));
            SC_TRUST_RESULT(fs.read(compilerCppLogPath.view(), compilerCppLog));
            SC_TRUST_RESULT(fs.read(archiverLogPath.view(), archiverLog));

            SC_TEST_EXPECT(StringView(compilerCLog.view()).containsString("helper.c"));
            SC_TEST_EXPECT(StringView(compilerCppLog.view()).containsString("library.cpp"));
            SC_TEST_EXPECT(StringView(compilerCppLog.view()).containsString("main.cpp"));
            SC_TEST_EXPECT(StringView(archiverLog.view()).containsString("libWorkspaceStaticLibrary.a"));
        }
    }
};

void runSCBuildTest(SC::TestReport& report) { SCBuildFixtureTest test(report); }
} // namespace SC
