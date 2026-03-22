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
static constexpr StringView FixtureWorkspaceName      = "SCBuildFixtures";
static constexpr StringView FixtureProjectName        = "TinyConsoleProgram";
static constexpr StringView HeaderFixtureProjectName  = "HeaderDependencyProgram";
static constexpr StringView CompileFailureProjectName = "CompileFailureProgram";
static constexpr StringView LinkFailureProjectName    = "LinkFailureProgram";
static constexpr StringView StaticLibraryProjectName  = "FixtureStaticLibrary";
static constexpr StringView StaticLibraryConsumerName = "StaticLibraryConsumer";

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

            FileSystem::FileStat initialObjectStat;
            FileSystem::FileStat initialExecutableStat;
            SC_TRUST_RESULT(fs.stat(objectPath.view(), initialObjectStat));
            SC_TRUST_RESULT(fs.stat(executablePath.view(), initialExecutableStat));

            Thread::Sleep(20);
            SC_TRUST_RESULT(writeHeaderDependencyFixture(fs, sourceRoot.view(), "second"));
            {
                String headerPath = StringEncoding::Utf8;
                SC_TRUST_RESULT(Path::join(headerPath, {sourceRoot.view(), "shared.h"}));
                SC_TRUST_RESULT(
                    fs.setLastModifiedTime(headerPath.view(), TimeMs{Time::Realtime::now().milliseconds + 2000}));
            }

            SC_TEST_EXPECT(Build::Action::execute(action, configureHeaderDependencyProgram, FixtureWorkspaceName));

            FileSystem::FileStat rebuiltObjectStat;
            FileSystem::FileStat rebuiltExecutableStat;
            SC_TRUST_RESULT(fs.stat(objectPath.view(), rebuiltObjectStat));
            SC_TRUST_RESULT(fs.stat(executablePath.view(), rebuiltExecutableStat));
            SC_TEST_EXPECT(rebuiltObjectStat.modifiedTime.milliseconds > initialObjectStat.modifiedTime.milliseconds);
            SC_TEST_EXPECT(rebuiltExecutableStat.modifiedTime.milliseconds >
                           initialExecutableStat.modifiedTime.milliseconds);

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
    }
};

void runSCBuildTest(SC::TestReport& report) { SCBuildFixtureTest test(report); }
} // namespace SC
