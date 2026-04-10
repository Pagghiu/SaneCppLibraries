// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Process/Process.h"
#include "Libraries/Strings/Console.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/Threading.h"
#include "Libraries/Time/Time.h"
#include "Tools/SC-build/Build.h"

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace SC
{
namespace
{
static constexpr StringView FixtureWorkspaceName           = "SCBuildFixtures";
static constexpr StringView FixtureProjectName             = "TinyConsoleProgram";
static constexpr StringView SmallFixtureProjectName        = "SmallSCProgram";
static constexpr StringView HeaderFixtureProjectName       = "HeaderDependencyProgram";
static constexpr StringView CompileFailureProjectName      = "CompileFailureProgram";
static constexpr StringView LinkFailureProjectName         = "LinkFailureProgram";
static constexpr StringView StaticLibraryProjectName       = "FixtureStaticLibrary";
static constexpr StringView StaticLibraryConsumerName      = "StaticLibraryConsumer";
static constexpr StringView WorkspaceLibraryProjectName    = "WorkspaceStaticLibrary";
static constexpr StringView WorkspaceExecutableProjectName = "WorkspaceExecutable";
static constexpr StringView IndependentProgramOneName      = "IndependentProgramOne";
static constexpr StringView IndependentProgramTwoName      = "IndependentProgramTwo";
#if SC_PLATFORM_APPLE or SC_PLATFORM_LINUX
static constexpr StringView CustomDriverSourceRootName = "CustomDriverFixture";
#endif

static native_char_t DynamicFixtureProjectRootStorage[1024] = {};
static StringView    DynamicFixtureProjectRoot;
static native_char_t DynamicLinkedLibraryPathStorage[1024] = {};
static StringView    DynamicLinkedLibraryPath;

static constexpr size_t SmallFixtureMaxBytesMacOS   = 128 * 1024;
static constexpr size_t SmallFixtureMaxBytesLinux   = 128 * 1024;
static constexpr size_t SmallFixtureMaxBytesWindows = 256 * 1024;

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

static Result verifyNativeBackendHostSupport()
{
#if SC_PLATFORM_APPLE or SC_PLATFORM_LINUX or SC_PLATFORM_WINDOWS
    return Result(true);
#else
    return Result::Error("Native backend fixture test is not supported on this host yet");
#endif
}

static Result detectCompilerName(const Build::Toolchain& toolchain, StringView& compilerName)
{
    switch (toolchain.family)
    {
    case Build::Toolchain::Clang: compilerName = "clang"; return Result(true);
    case Build::Toolchain::GCC: compilerName = "gcc"; return Result(true);
    case Build::Toolchain::LLVMMingw: compilerName = "llvm-mingw"; return Result(true);
    case Build::Toolchain::MSVC: compilerName = "msvc"; return Result(true);
    case Build::Toolchain::ClangCL: compilerName = "clang-cl"; return Result(true);
    case Build::Toolchain::CustomDriver:
        compilerName = Path::basename(toolchain.compilerCpp.view(), Path::AsNative);
        return Result(true);
    case Build::Toolchain::HostDefault:
#if SC_PLATFORM_WINDOWS
        compilerName = "msvc";
        return Result(true);
#elif SC_PLATFORM_APPLE
        compilerName = "clang";
        return Result(true);
#else
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
#endif
    }
    Assert::unreachable();
}

static size_t getSmallFixtureMaxBytes()
{
    switch (HostPlatform)
    {
    case SC::Platform::Apple: return SmallFixtureMaxBytesMacOS;
    case SC::Platform::Linux: return SmallFixtureMaxBytesLinux;
    case SC::Platform::Windows: return SmallFixtureMaxBytesWindows;
    case SC::Platform::Emscripten: break;
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

#if SC_PLATFORM_APPLE or SC_PLATFORM_LINUX
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

static Result writeNoisyToolWrapperScript(FileSystem& fs, StringView scriptPath, StringView logPath,
                                          StringView toolPath, StringView stdOutText, StringView stdErrText)
{
    String scriptContents = StringEncoding::Utf8;
    SC_TRY(StringBuilder::format(scriptContents,
                                 "#!/bin/sh\n"
                                 "printf '%s\\n' \"$*\" >> \"{}\"\n"
                                 "printf '%s\\n' '{}'\n"
                                 "printf '%s\\n' '{}' 1>&2\n"
                                 "exec \"{}\" \"$@\"\n",
                                 logPath, stdOutText, stdErrText, toolPath));
    SC_TRY(fs.writeString(scriptPath, scriptContents.view()));
    SC_TRY(fs.chmod(scriptPath, 0755u));
    return Result(true);
}

static Result writeLoggingOnlyWrapperScript(FileSystem& fs, StringView scriptPath, StringView logPath,
                                            StringView stdOutText = {})
{
    String scriptContents = StringEncoding::Utf8;
    if (stdOutText.isEmpty())
    {
        SC_TRY(StringBuilder::format(scriptContents,
                                     "#!/bin/sh\n"
                                     "printf '%s\\n' \"$*\" >> \"{}\"\n"
                                     "exit 0\n",
                                     logPath));
    }
    else
    {
        SC_TRY(StringBuilder::format(scriptContents,
                                     "#!/bin/sh\n"
                                     "printf '%s\\n' \"$*\" >> \"{}\"\n"
                                     "printf '%s\\n' '{}'\n"
                                     "exit 0\n",
                                     logPath, stdOutText));
    }
    SC_TRY(fs.writeString(scriptPath, scriptContents.view()));
    SC_TRY(fs.chmod(scriptPath, 0755u));
    return Result(true);
}
#endif

#if SC_PLATFORM_WINDOWS
static Result resolveVisualStudioLLVMToolPath(StringView executableName, String& toolPath)
{
    FileSystem fs;
    SC_TRY(fs.init("."));

    String vswherePath = StringEncoding::Utf8;
    SC_TRY(vswherePath.assign("C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe"));
    SC_TRY_MSG(fs.existsAndIsFile(vswherePath.view()), "Cannot locate vswhere.exe");

    Process process;
    String  output = StringEncoding::Utf8;
    SC_TRY(process.exec({vswherePath.view(), "-latest", "-property", "installationPath"}, output));
    SC_TRY_MSG(process.getExitStatus() == 0, "Cannot locate Visual Studio installation");

    String installPath = StringEncoding::Utf8;
    SC_TRY(installPath.assign(StringView(output.view()).trimWhiteSpaces()));
    SC_TRY_MSG(not installPath.isEmpty(), "Visual Studio installation path is empty");

    SC_TRY(Path::join(toolPath, {installPath.view(), "VC", "Tools", "Llvm", "bin", executableName}));
    SC_TRY_MSG(fs.existsAndIsFile(toolPath.view()), "Bundled LLVM tool is missing");
    return Result(true);
}
#endif

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

static Result configureSmallSCProgram(Build::Definition& definition, const Build::Parameters& parameters)
{
    Build::Workspace workspace = {FixtureWorkspaceName};
    Build::Project   project   = {Build::TargetType::ConsoleExecutable, SmallFixtureProjectName};

    SC_TRY(project.setRootDirectory(parameters.directories.libraryDirectory.view()));
    SC_TRY(project.addPresetConfiguration(Build::Configuration::Preset::Debug, parameters));
    SC_TRY(project.addPresetConfiguration(Build::Configuration::Preset::Release, parameters));
    SC_TRY(project.addIncludePaths({"."}));
    SC_TRY(project.addFile("Libraries/Foundation/Foundation.cpp"));
    SC_TRY(project.addFile("Libraries/Memory/Memory.cpp"));
    SC_TRY(project.addFile("Libraries/Strings/Strings.cpp"));
    SC_TRY(project.addFile("Tests/SCBuildTest/Fixture/SmallSCProgram/main.cpp"));
    if (parameters.platform == Build::Platform::Apple)
    {
        SC_TRY(project.addLinkFrameworks({"CoreFoundation"}));
    }

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

static Result configureIndependentWorkspacePrograms(Build::Definition& definition, const Build::Parameters& parameters)
{
    SC_TRY_MSG(not DynamicFixtureProjectRoot.isEmpty(), "Dynamic fixture root is not initialized");

    Build::Workspace workspace = {FixtureWorkspaceName};

    String programOneRoot = StringEncoding::Utf8;
    String programTwoRoot = StringEncoding::Utf8;
    SC_TRY(Path::join(programOneRoot, {DynamicFixtureProjectRoot, "ProgramOne"}));
    SC_TRY(Path::join(programTwoRoot, {DynamicFixtureProjectRoot, "ProgramTwo"}));

    Build::Project programOne = {Build::TargetType::ConsoleExecutable, IndependentProgramOneName};
    SC_TRY(programOne.setRootDirectory(programOneRoot.view()));
    SC_TRY(programOne.addPresetConfiguration(Build::Configuration::Preset::Debug, parameters));
    SC_TRY(programOne.addPresetConfiguration(Build::Configuration::Preset::Release, parameters));
    SC_TRY(programOne.addFiles(".", "*.cpp"));

    Build::Project programTwo = {Build::TargetType::ConsoleExecutable, IndependentProgramTwoName};
    SC_TRY(programTwo.setRootDirectory(programTwoRoot.view()));
    SC_TRY(programTwo.addPresetConfiguration(Build::Configuration::Preset::Debug, parameters));
    SC_TRY(programTwo.addPresetConfiguration(Build::Configuration::Preset::Release, parameters));
    SC_TRY(programTwo.addFiles(".", "*.cpp"));

    SC_TRY(workspace.projects.push_back(move(programOne)));
    SC_TRY(workspace.projects.push_back(move(programTwo)));
    SC_TRY(definition.workspaces.push_back(move(workspace)));
    return Result(true);
}

#if SC_PLATFORM_APPLE or SC_PLATFORM_LINUX
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
#endif

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
    case Build::TargetType::GUIApplication:
        if (action.parameters.platform == Build::Platform::Windows)
        {
            SC_TRY(StringBuilder::format(artifactName, "{}.exe", projectName));
        }
        else
        {
            SC_TRY(artifactName.assign(projectName));
        }
        break;
    case Build::TargetType::SharedLibrary:
        if (action.parameters.platform == Build::Platform::Windows)
        {
            SC_TRY(StringBuilder::format(artifactName, "{}.dll", projectName));
        }
        else if (action.parameters.platform == Build::Platform::Apple)
        {
            SC_TRY(StringBuilder::format(artifactName, "{}.dylib", projectName));
        }
        else
        {
            SC_TRY(StringBuilder::format(artifactName, "{}.so", projectName));
        }
        break;
    case Build::TargetType::StaticLibrary:
        if (action.parameters.platform == Build::Platform::Windows)
        {
            SC_TRY(StringBuilder::format(artifactName, "{}.lib", projectName));
        }
        else if (projectName.startsWith("lib"))
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

struct CapturedBuildOutput
{
    String stdOut = StringEncoding::Utf8;
    String stdErr = StringEncoding::Utf8;
};

struct CapturedProcessOutput
{
    String stdOut     = StringEncoding::Utf8;
    String stdErr     = StringEncoding::Utf8;
    int    exitStatus = -1;
};

static Result normalizeConsoleOutput(String& output)
{
#if SC_PLATFORM_WINDOWS
    String normalized = StringEncoding::Utf8;
    auto   builder    = StringBuilder::create(normalized);
    SC_TRY(builder.appendReplaceAll(output.view(), "\r\n", "\n"));
    builder.finalize();
    output = move(normalized);
#else
    SC_COMPILER_UNUSED(output);
#endif
    return Result(true);
}

static Result captureBuildActionOutput(const Build::Action& action, Build::Action::ConfigureFunction configure,
                                       StringView defaultWorkspaceName, Result& buildResult,
                                       CapturedBuildOutput& capturedOutput)
{
    FileSystem fs;
    SC_TRY(fs.init("."));

    String captureDirectory = StringEncoding::Utf8;
    String stdoutPath       = StringEncoding::Utf8;
    String stderrPath       = StringEncoding::Utf8;
    SC_TRY(Path::join(captureDirectory, {action.parameters.directories.intermediatesDirectory.view(), "_Captured"}));
    SC_TRY(fs.makeDirectoryRecursive(captureDirectory.view()));
    SC_TRY(Path::join(stdoutPath, {captureDirectory.view(), "stdout.txt"}));
    SC_TRY(Path::join(stderrPath, {captureDirectory.view(), "stderr.txt"}));
    SC_TRY(fs.writeString(stdoutPath.view(), ""));
    SC_TRY(fs.writeString(stderrPath.view(), ""));

    Console        redirectedConsole;
    Console* const previousConsole = globalConsole;
    globalConsole                  = &redirectedConsole;

#if SC_PLATFORM_WINDOWS
    HANDLE oldStdOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE oldStdErr = ::GetStdHandle(STD_ERROR_HANDLE);

    FileDescriptor stdoutFile;
    FileDescriptor stderrFile;
    SC_TRY(stdoutFile.open(stdoutPath.view(), FileOpen::WriteRead));
    SC_TRY(stderrFile.open(stderrPath.view(), FileOpen::WriteRead));

    void* stdoutHandle = nullptr;
    void* stderrHandle = nullptr;
    SC_TRY(stdoutFile.get(stdoutHandle, Result::Error("Captured stdout handle is invalid")));
    SC_TRY(stderrFile.get(stderrHandle, Result::Error("Captured stderr handle is invalid")));
    SC_TRY_MSG(::SetStdHandle(STD_OUTPUT_HANDLE, stdoutHandle) == TRUE, "SetStdHandle(stdout) failed");
    SC_TRY_MSG(::SetStdHandle(STD_ERROR_HANDLE, stderrHandle) == TRUE, "SetStdHandle(stderr) failed");

    Console windowsRedirectedConsole;
    globalConsole = &windowsRedirectedConsole;
    buildResult   = Build::Action::execute(action, configure, defaultWorkspaceName);
    globalConsole->flush();
    globalConsole->flushStdErr();

    globalConsole = previousConsole;
    SC_TRY_MSG(::SetStdHandle(STD_OUTPUT_HANDLE, oldStdOut) == TRUE, "Restore stdout handle failed");
    SC_TRY_MSG(::SetStdHandle(STD_ERROR_HANDLE, oldStdErr) == TRUE, "Restore stderr handle failed");

    SC_TRY(stdoutFile.seek(FileDescriptor::SeekStart, 0));
    SC_TRY(stderrFile.seek(FileDescriptor::SeekStart, 0));
    SC_TRY(stdoutFile.readUntilEOF(capturedOutput.stdOut));
    SC_TRY(stderrFile.readUntilEOF(capturedOutput.stdErr));
#else
    const int oldStdOut = ::dup(STDOUT_FILENO);
    SC_TRY_MSG(oldStdOut != -1, "dup(stdout) failed");
    const int oldStdErr = ::dup(STDERR_FILENO);
    if (oldStdErr == -1)
    {
        (void)::close(oldStdOut);
        return Result::Error("dup(stderr) failed");
    }

    const int stdoutDescriptor = ::open(stdoutPath.view().getNullTerminatedNative(), O_CREAT | O_TRUNC | O_RDWR, 0600);
    if (stdoutDescriptor == -1)
    {
        (void)::close(oldStdOut);
        (void)::close(oldStdErr);
        return Result::Error("open(stdout capture) failed");
    }
    const int stderrDescriptor = ::open(stderrPath.view().getNullTerminatedNative(), O_CREAT | O_TRUNC | O_RDWR, 0600);
    if (stderrDescriptor == -1)
    {
        (void)::close(stdoutDescriptor);
        (void)::close(oldStdOut);
        (void)::close(oldStdErr);
        return Result::Error("open(stderr capture) failed");
    }

    if (::dup2(stdoutDescriptor, STDOUT_FILENO) == -1 or ::dup2(stderrDescriptor, STDERR_FILENO) == -1)
    {
        (void)::close(stdoutDescriptor);
        (void)::close(stderrDescriptor);
        (void)::close(oldStdOut);
        (void)::close(oldStdErr);
        return Result::Error("dup2 capture redirect failed");
    }
    (void)::close(stdoutDescriptor);
    (void)::close(stderrDescriptor);

    buildResult = Build::Action::execute(action, configure, defaultWorkspaceName);
    globalConsole->flush();
    globalConsole->flushStdErr();

    globalConsole           = previousConsole;
    const int restoreStdOut = ::dup2(oldStdOut, STDOUT_FILENO);
    const int restoreStdErr = ::dup2(oldStdErr, STDERR_FILENO);
    (void)::close(oldStdOut);
    (void)::close(oldStdErr);
    SC_TRY_MSG(restoreStdOut != -1 and restoreStdErr != -1, "dup2 restore failed");

    SC_TRY(fs.read(stdoutPath.view(), capturedOutput.stdOut));
    SC_TRY(fs.read(stderrPath.view(), capturedOutput.stdErr));
#endif

    SC_TRY(normalizeConsoleOutput(capturedOutput.stdOut));
    SC_TRY(normalizeConsoleOutput(capturedOutput.stdErr));
    return Result(true);
}

#if SC_PLATFORM_APPLE
static Result captureRepositoryBuildCommand(TestReport& report, Span<const StringSpan> arguments,
                                            CapturedProcessOutput& capturedOutput)
{
    String scriptPath = StringEncoding::Utf8;
    SC_TRY(Path::join(scriptPath, {report.libraryRootDirectory.view(), "SC.sh"}));

    StringSpan processArguments[32];
    size_t     numArguments          = 0;
    processArguments[numArguments++] = scriptPath.view();
    for (const StringSpan argument : arguments)
    {
        SC_TRY_MSG(numArguments < sizeof(processArguments) / sizeof(processArguments[0]), "Too many process arguments");
        processArguments[numArguments++] = argument;
    }

    Process process;
    SC_TRY(process.setWorkingDirectory(report.libraryRootDirectory.view()));
    SC_TRY(process.exec({processArguments, numArguments}, capturedOutput.stdOut, {}, capturedOutput.stdErr));
    capturedOutput.exitStatus = process.getExitStatus();
    SC_TRY(normalizeConsoleOutput(capturedOutput.stdOut));
    SC_TRY(normalizeConsoleOutput(capturedOutput.stdErr));
    return Result(true);
}
#endif

#if SC_PLATFORM_WINDOWS
static Result computeWindowsImportLibraryPath(const Build::Action& action, StringView projectName, String& libraryPath)
{
    String buildDirectory = StringEncoding::Utf8;
    SC_TRY(computeBuildDirectoryName(action, buildDirectory));
    String artifactName = StringEncoding::Utf8;
    SC_TRY(StringBuilder::format(artifactName, "{}.lib", projectName));
    SC_TRY(Path::join(libraryPath, {action.parameters.directories.outputsDirectory.view(), buildDirectory.view(),
                                    artifactName.view()}));
    return Result(true);
}
#endif

static Result computeObjectPath(const Build::Action& action, StringView projectName, StringView sourceName,
                                String& objectPath)
{
    String buildDirectory = StringEncoding::Utf8;
    SC_TRY(computeBuildDirectoryName(action, buildDirectory));

    String objectName = StringEncoding::Utf8;
    SC_TRY(StringBuilder::format(objectName, "{}{}", sourceName,
                                 action.parameters.platform == Build::Platform::Windows ? ".obj" : ".o"));
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

static Result writeIndependentProgramFixture(FileSystem& fs, StringView sourceRoot, StringView message)
{
    SC_TRY(fs.makeDirectoryRecursive(sourceRoot));

    String sourcePath = StringEncoding::Utf8;
    SC_TRY(Path::join(sourcePath, {sourceRoot, "main.cpp"}));
    String sourceContents = StringEncoding::Utf8;
    SC_TRY(StringBuilder::format(sourceContents,
                                 "#include <stdio.h>\n"
                                 "\n"
                                 "int main()\n"
                                 "{{\n"
                                 "    puts(\"{}\");\n"
                                 "    return 0;\n"
                                 "}}\n",
                                 message));
    SC_TRY(fs.writeString(sourcePath.view(), sourceContents.view()));
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

#if SC_PLATFORM_APPLE or SC_PLATFORM_LINUX
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
#endif

static Build::Action makeNativeCompileAction(const Build::Directories& directories, StringView projectName,
                                             StringView configurationName = "Debug")
{
    Build::Action action;
    action.action                  = Build::Action::Compile;
    action.workspaceName           = FixtureWorkspaceName;
    action.projectName             = projectName;
    action.configurationName       = configurationName;
    action.parameters.generator    = Build::Generator::Native;
    action.parameters.platform     = getBuildPlatform();
    action.parameters.architecture = getBuildArchitecture();
    action.parameters.directories  = directories;
    return action;
}

#if SC_PLATFORM_APPLE or SC_PLATFORM_LINUX
static Build::Action makeGeneratedCompileAction(const Build::Directories& directories, StringView projectName,
                                                StringView configurationName = "Debug")
{
    Build::Action action        = makeNativeCompileAction(directories, projectName, configurationName);
    action.parameters.generator = Build::Generator::Make;
    return action;
}
#endif

static Result runBuiltProgram(StringView executablePath, String& stdoutOutput)
{
    StringSpan processArguments[] = {executablePath};
    Process    process;
    String     rawStdout = StringEncoding::Utf8;
    SC_TRY(process.exec({processArguments, 1}, rawStdout));
    SC_TRY_MSG(process.getExitStatus() == 0, "Fixture program exited with non-zero status");
#if SC_PLATFORM_WINDOWS
    SC_TRY(stdoutOutput.assign({}));
    SC_TRY(StringBuilder::create(stdoutOutput).appendReplaceAll(rawStdout.view(), "\r\n", "\n"));
#else
    SC_TRY(stdoutOutput.assign(rawStdout.view()));
#endif
    return Result(true);
}

static Result verifyNoSCExportsFromExecutable(StringView executablePath, StringView importLibraryPath = {})
{
#if SC_PLATFORM_WINDOWS
    FileSystem fs;
    SC_TRY(fs.init("."));
    SC_TRY_MSG(not importLibraryPath.isEmpty(), "Missing import library path");
    SC_TRY_MSG(not fs.existsAndIsFile(importLibraryPath), "Unexpected import library emitted for executable");
    SC_COMPILER_UNUSED(executablePath);
    return Result(true);
#else
    SC_COMPILER_UNUSED(importLibraryPath);
    String nmPath = StringEncoding::Utf8;
    SC_TRY(resolveHostToolPath("nm", nmPath));

    StringSpan argumentsStorage[5];
    size_t     numArguments          = 0;
    argumentsStorage[numArguments++] = nmPath.view();
#if SC_PLATFORM_APPLE
    argumentsStorage[numArguments++] = "-gU";
    argumentsStorage[numArguments++] = "-C";
#else
    argumentsStorage[numArguments++] = "-D";
    argumentsStorage[numArguments++] = "--defined-only";
    argumentsStorage[numArguments++] = "-C";
#endif
    argumentsStorage[numArguments++] = executablePath;

    Process process;
    String  output = StringEncoding::Utf8;
    SC_TRY(process.exec({argumentsStorage, numArguments}, output));
    SC_TRY_MSG(process.getExitStatus() == 0, "Failed to inspect executable exports");
    SC_TRY_MSG(not StringView(output.view()).containsString("SC::"), "Unexpected SC export from executable");
    return Result(true);
#endif
}
} // namespace

struct SCBuildFixtureTest : public SC::TestCase
{
    SCBuildFixtureTest(SC::TestReport& report) : TestCase(report, "SCBuildTest")
    {
        String tinyFixtureDirectory = report.libraryRootDirectory.view();
        SC_TRUST_RESULT(Path::append(tinyFixtureDirectory, {"Tests", "SCBuildTest", "Fixture", "TinyConsoleProgram"},
                                     Path::AsNative));
        String smallFixtureDirectory = report.libraryRootDirectory.view();
        SC_TRUST_RESULT(
            Path::append(smallFixtureDirectory, {"Tests", "SCBuildTest", "Fixture", "SmallSCProgram"}, Path::AsNative));

        if (test_section("fixture layout"))
        {
            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            SC_TEST_EXPECT(fs.exists(tinyFixtureDirectory.view()));
            SC_TEST_EXPECT(fs.exists(smallFixtureDirectory.view()));

            String mainSource = tinyFixtureDirectory.view();
            SC_TRUST_RESULT(Path::append(mainSource, {"main.cpp"}, Path::AsNative));
            SC_TEST_EXPECT(fs.exists(mainSource.view()));

            String expectedOutput = tinyFixtureDirectory.view();
            SC_TRUST_RESULT(Path::append(expectedOutput, {"stdout.txt"}, Path::AsNative));
            SC_TEST_EXPECT(fs.exists(expectedOutput.view()));

            mainSource = smallFixtureDirectory.view();
            SC_TRUST_RESULT(Path::append(mainSource, {"main.cpp"}, Path::AsNative));
            SC_TEST_EXPECT(fs.exists(mainSource.view()));

            expectedOutput = smallFixtureDirectory.view();
            SC_TRUST_RESULT(Path::append(expectedOutput, {"stdout.txt"}, Path::AsNative));
            SC_TEST_EXPECT(fs.exists(expectedOutput.view()));
        }

        if (test_section("native backend builds and runs fixture"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

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
            SC_TRUST_RESULT(normalizeConsoleOutput(expectedOutput));
            SC_TEST_EXPECT(stdoutOutput == expectedOutput.view());
        }

#if SC_PLATFORM_APPLE or SC_PLATFORM_LINUX
        if (test_section("native backend cross compiles Windows fixture with llvm-mingw"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            Build::Action action                         = makeNativeCompileAction(directories, FixtureProjectName);
            action.parameters.platform                   = Build::Platform::Windows;
            action.parameters.architecture               = Build::Architecture::Intel64;
            action.parameters.toolchain.family           = Build::Toolchain::LLVMMingw;
            action.parameters.targetMachine.platform     = Build::Platform::Windows;
            action.parameters.targetMachine.architecture = Build::Architecture::Intel64;
            action.parameters.targetMachine.environment  = Build::TargetEnvironment::WindowsGNU;
            SC_TRUST_RESULT(action.parameters.toolchain.targetTriple.assign("x86_64-w64-windows-gnu"));

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram, FixtureWorkspaceName));

            String executablePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, FixtureProjectName, executablePath));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));
            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));
        }

        if (test_section("native backend routes Windows runs through a Wine runner"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String toolRoot         = StringEncoding::Utf8;
            String runnerLog        = StringEncoding::Utf8;
            String runnerConsoleLog = StringEncoding::Utf8;
            String runnerPath       = StringEncoding::Utf8;
            String runnerConsole    = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(toolRoot, {buildRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(Path::join(runnerLog, {toolRoot.view(), "wine.log"}));
            SC_TRUST_RESULT(Path::join(runnerConsoleLog, {toolRoot.view(), "wineconsole.log"}));
            SC_TRUST_RESULT(Path::join(runnerPath, {toolRoot.view(), "wine"}));
            SC_TRUST_RESULT(Path::join(runnerConsole, {toolRoot.view(), "wineconsole"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolRoot.view()));
            SC_TRUST_RESULT(writeLoggingOnlyWrapperScript(fs, runnerPath.view(), runnerLog.view(), "wrapped"));
            SC_TRUST_RESULT(
                writeLoggingOnlyWrapperScript(fs, runnerConsole.view(), runnerConsoleLog.view(), "console"));

            Build::Action action                         = makeNativeCompileAction(directories, FixtureProjectName);
            action.action                                = Build::Action::Run;
            action.parameters.platform                   = Build::Platform::Windows;
            action.parameters.architecture               = Build::Architecture::Intel64;
            action.parameters.toolchain.family           = Build::Toolchain::LLVMMingw;
            action.parameters.targetMachine.platform     = Build::Platform::Windows;
            action.parameters.targetMachine.architecture = Build::Architecture::Intel64;
            action.parameters.targetMachine.environment  = Build::TargetEnvironment::WindowsGNU;
            action.parameters.runner.type                = Build::RunnerSpec::Wine;
            SC_TRUST_RESULT(action.parameters.toolchain.targetTriple.assign("x86_64-w64-windows-gnu"));
            SC_TRUST_RESULT(action.parameters.runner.executable.assign(runnerPath.view()));

            StringView forwardedArguments[] = {"--fixture", "runner"};
            action.additionalArguments      = forwardedArguments;

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram, FixtureWorkspaceName));

            String executablePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, FixtureProjectName, executablePath));
            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));

            String runnerInvocation = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(runnerLog.view(), runnerInvocation));
            SC_TEST_EXPECT(StringView(runnerInvocation.view()).containsString("reg add"));
            SC_TEST_EXPECT(StringView(runnerInvocation.view()).containsString("reg delete"));

            if (HostPlatform == SC::Platform::Linux)
            {
                String runnerConsoleInvocation = StringEncoding::Utf8;
                SC_TRUST_RESULT(fs.read(runnerConsoleLog.view(), runnerConsoleInvocation));
                SC_TEST_EXPECT(StringView(runnerConsoleInvocation.view()).containsString("--backend=curses"));
                SC_TEST_EXPECT(StringView(runnerConsoleInvocation.view()).containsString(executablePath.view()));
                SC_TEST_EXPECT(StringView(runnerConsoleInvocation.view()).containsString("--fixture runner"));
            }
            else
            {
                SC_TEST_EXPECT(StringView(runnerInvocation.view()).containsString(executablePath.view()));
                SC_TEST_EXPECT(StringView(runnerInvocation.view()).containsString("--fixture runner"));
            }
        }
#endif

#if SC_PLATFORM_APPLE
        if (test_section("native backend smoke-starts SCTest through Wine on macOS"))
        {
            CapturedProcessOutput capturedOutput;
            const StringSpan      arguments[] = {
                "build",    "run",   "SCTest", "--target", "windows-gnu-x86_64", "--runner",       "auto",
                "--output", "quiet", "--",     "--test",   "BaseTest",           "--test-section", "new/delete",
            };
            SC_TRUST_RESULT(captureRepositoryBuildCommand(report, arguments, capturedOutput));

            SC_TEST_EXPECT(capturedOutput.exitStatus == 0);
            SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view()).containsString("RUNNER = "));
            SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view()).containsString("SCTest.exe"));
            SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view())
                               .containsString("TestReport::Running single test \"BaseTest\""));
            SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view())
                               .containsString("TestReport::Running single section \"new/delete\""));
            SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view()).containsString("TOTAL Succeeded = 1"));
        }
#endif

        if (test_section("native backend keeps small fixture small and unexported"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            Build::Action action = makeNativeCompileAction(directories, SmallFixtureProjectName, "Release");

            SC_TEST_EXPECT(Build::Action::execute(action, configureSmallSCProgram, FixtureWorkspaceName));

            String executablePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, SmallFixtureProjectName, executablePath));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));
            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));

            String stdoutOutput = StringEncoding::Utf8;
            SC_TEST_EXPECT(runBuiltProgram(executablePath.view(), stdoutOutput));

            String expectedOutput = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read("Tests/SCBuildTest/Fixture/SmallSCProgram/stdout.txt", expectedOutput));
            SC_TRUST_RESULT(normalizeConsoleOutput(expectedOutput));
            SC_TEST_EXPECT(stdoutOutput == expectedOutput.view());

            FileSystem::FileStat executableStat;
            SC_TRUST_RESULT(fs.stat(executablePath.view(), executableStat));
            SC_TEST_EXPECT(executableStat.fileSize <= getSmallFixtureMaxBytes());

            String importLibraryPath = StringEncoding::Utf8;
#if SC_PLATFORM_WINDOWS
            SC_TRUST_RESULT(computeWindowsImportLibraryPath(action, SmallFixtureProjectName, importLibraryPath));
#endif
            SC_TEST_EXPECT(verifyNoSCExportsFromExecutable(executablePath.view(), importLibraryPath.view()));
        }

        if (test_section("native backend skips up-to-date work in verbose mode"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

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
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

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
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

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

        if (test_section("native backend groups quiet mode failures"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String sourceRoot = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(sourceRoot, {buildRoot.view(), "QuietFailureFixture"}));
            SC_TRUST_RESULT(writeSourceFixture(fs, sourceRoot.view(),
                                               "#include <stdio.h>\n"
                                               "int main()\n"
                                               "{\n"
                                               "    this_will_not_compile(\n"
                                               "}\n"));
            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(sourceRoot.view()));

            Build::Action action                   = makeNativeCompileAction(directories, CompileFailureProjectName);
            action.parameters.execution.outputMode = Build::OutputMode::Quiet;

            Result              buildResult = Result(true);
            CapturedBuildOutput capturedOutput;
            SC_TRUST_RESULT(captureBuildActionOutput(action, configureCompileFailureProgram, FixtureWorkspaceName,
                                                     buildResult, capturedOutput));
            SC_TEST_EXPECT(not buildResult);
            const StringView quietStdOut = capturedOutput.stdOut.view();
            SC_TEST_EXPECT(quietStdOut.isEmpty() or quietStdOut.bytesWithoutTerminator()[0] != '[');
            SC_TEST_EXPECT(not StringView(quietStdOut).containsString("\n[1/"));
            SC_TEST_EXPECT(StringView(quietStdOut).containsString("FAILED:"));
            SC_TEST_EXPECT(StringView(quietStdOut).containsString("Build Summary:"));
        }

        if (test_section("native backend reports link failures"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

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
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

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
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

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

        if (test_section("native backend builds independent workspace targets together"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String programOneRoot = StringEncoding::Utf8;
            String programTwoRoot = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(programOneRoot, {buildRoot.view(), "ProgramOne"}));
            SC_TRUST_RESULT(Path::join(programTwoRoot, {buildRoot.view(), "ProgramTwo"}));
            SC_TRUST_RESULT(writeIndependentProgramFixture(fs, programOneRoot.view(), "program-one"));
            SC_TRUST_RESULT(writeIndependentProgramFixture(fs, programTwoRoot.view(), "program-two"));
            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(buildRoot.view()));

            Build::Action action                        = makeNativeCompileAction(directories, {});
            action.parameters.execution.maxParallelJobs = 2;

            SC_TEST_EXPECT(Build::Action::execute(action, configureIndependentWorkspacePrograms, FixtureWorkspaceName));

            String programOneExecutable = StringEncoding::Utf8;
            String programTwoExecutable = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, IndependentProgramOneName, programOneExecutable));
            SC_TRUST_RESULT(computeExecutablePath(action, IndependentProgramTwoName, programTwoExecutable));

            SC_TEST_EXPECT(fs.existsAndIsFile(programOneExecutable.view()));
            SC_TEST_EXPECT(fs.existsAndIsFile(programTwoExecutable.view()));

            String stdoutOutput = StringEncoding::Utf8;
            SC_TEST_EXPECT(runBuiltProgram(programOneExecutable.view(), stdoutOutput));
            SC_TEST_EXPECT(stdoutOutput == "program-one\n");

            stdoutOutput = "";
            SC_TEST_EXPECT(runBuiltProgram(programTwoExecutable.view(), stdoutOutput));
            SC_TEST_EXPECT(stdoutOutput == "program-two\n");
        }

#if SC_PLATFORM_APPLE or SC_PLATFORM_LINUX
        if (test_section("generated make backend suppresses successful compile chatter in quiet mode"))
        {
            String             normalBuildRoot = StringEncoding::Utf8;
            Build::Directories normalDirectories;
            SC_TRUST_RESULT(createFixtureDirectories(report, normalBuildRoot, normalDirectories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String normalSourceRoot = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(normalSourceRoot, {normalBuildRoot.view(), "GeneratedQuietNormalFixture"}));
            SC_TRUST_RESULT(writeHeaderDependencyFixture(fs, normalSourceRoot.view(), "normal-generated"));
            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(normalSourceRoot.view()));

            Build::Action normalAction = makeGeneratedCompileAction(normalDirectories, HeaderFixtureProjectName);
            normalAction.parameters.execution.outputMode = Build::OutputMode::Normal;

            Result              normalBuildResult = Result(true);
            CapturedBuildOutput normalOutput;
            SC_TRUST_RESULT(captureBuildActionOutput(normalAction, configureHeaderDependencyProgram,
                                                     FixtureWorkspaceName, normalBuildResult, normalOutput));
            SC_TEST_EXPECT(normalBuildResult);

            String             quietBuildRoot = StringEncoding::Utf8;
            Build::Directories quietDirectories;
            SC_TRUST_RESULT(createFixtureDirectories(report, quietBuildRoot, quietDirectories));

            String quietSourceRoot = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(quietSourceRoot, {quietBuildRoot.view(), "GeneratedQuietModeFixture"}));
            SC_TRUST_RESULT(writeHeaderDependencyFixture(fs, quietSourceRoot.view(), "quiet-generated"));
            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(quietSourceRoot.view()));

            Build::Action quietAction = makeGeneratedCompileAction(quietDirectories, HeaderFixtureProjectName);
            quietAction.parameters.execution.outputMode = Build::OutputMode::Quiet;

            Result              quietBuildResult = Result(true);
            CapturedBuildOutput quietOutput;
            SC_TRUST_RESULT(captureBuildActionOutput(quietAction, configureHeaderDependencyProgram,
                                                     FixtureWorkspaceName, quietBuildResult, quietOutput));
            SC_TEST_EXPECT(quietBuildResult);
            SC_TEST_EXPECT(quietOutput.stdOut.view().sizeInBytes() < normalOutput.stdOut.view().sizeInBytes());
        }

        if (test_section("generated make run keeps program output in quiet mode"))
        {
            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String sourceRoot = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(sourceRoot, {buildRoot.view(), "GeneratedRunQuietFixture"}));
            SC_TRUST_RESULT(writeHeaderDependencyFixture(fs, sourceRoot.view(), "quiet-run"));
            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(sourceRoot.view()));

            Build::Action action                   = makeGeneratedCompileAction(directories, HeaderFixtureProjectName);
            action.action                          = Build::Action::Run;
            action.parameters.execution.outputMode = Build::OutputMode::Quiet;

            Result              buildResult = Result(true);
            CapturedBuildOutput capturedOutput;
            SC_TRUST_RESULT(captureBuildActionOutput(action, configureHeaderDependencyProgram, FixtureWorkspaceName,
                                                     buildResult, capturedOutput));
            SC_TEST_EXPECT(buildResult);
            SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view()).containsString("quiet-run\n"));
        }

        if (test_section("generated make backend keeps incremental rebuilds"))
        {
            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String sourceRoot = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(sourceRoot, {buildRoot.view(), "GeneratedIncrementalFixture"}));
            SC_TRUST_RESULT(writeHeaderDependencyFixture(fs, sourceRoot.view(), "incremental"));
            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(sourceRoot.view()));

            Build::Action action                   = makeGeneratedCompileAction(directories, HeaderFixtureProjectName);
            action.parameters.execution.outputMode = Build::OutputMode::Normal;

            SC_TEST_EXPECT(Build::Action::execute(action, configureHeaderDependencyProgram, FixtureWorkspaceName));

            Result              buildResult = Result(true);
            CapturedBuildOutput capturedOutput;
            SC_TRUST_RESULT(captureBuildActionOutput(action, configureHeaderDependencyProgram, FixtureWorkspaceName,
                                                     buildResult, capturedOutput));
            SC_TEST_EXPECT(buildResult);
            SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view()).containsString("Nothing to be done"));
            SC_TEST_EXPECT(not StringView(capturedOutput.stdOut.view()).containsString("Compiling "));
            SC_TEST_EXPECT(not StringView(capturedOutput.stdOut.view()).containsString("Linking "));
        }

        if (test_section("native backend suppresses successful compile noise in normal mode"))
        {
            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String sourceRoot    = StringEncoding::Utf8;
            String toolchainRoot = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(sourceRoot, {buildRoot.view(), "NormalOutputModeFixture"}));
            SC_TRUST_RESULT(Path::join(toolchainRoot, {sourceRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(writeSourceFixture(fs, sourceRoot.view(),
                                               "#include <stdio.h>\n"
                                               "int main()\n"
                                               "{\n"
                                               "    puts(\"normal-output-mode\");\n"
                                               "    return 0;\n"
                                               "}\n"));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolchainRoot.view()));
            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(sourceRoot.view()));

            String hostCompilerC   = StringEncoding::Utf8;
            String hostCompilerCpp = StringEncoding::Utf8;
            String hostArchiver    = StringEncoding::Utf8;
            SC_TRUST_RESULT(resolveHostToolPath("clang", hostCompilerC));
            SC_TRUST_RESULT(resolveHostToolPath("clang++", hostCompilerCpp));
            SC_TRUST_RESULT(resolveHostToolPath("ar", hostArchiver));

            String compilerLogPath = StringEncoding::Utf8;
            String compilerWrapper = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(compilerLogPath, {toolchainRoot.view(), "compiler.log"}));
            SC_TRUST_RESULT(Path::join(compilerWrapper, {toolchainRoot.view(), "compiler.sh"}));
            SC_TRUST_RESULT(fs.writeString(compilerLogPath.view(), ""));
            SC_TRUST_RESULT(writeNoisyToolWrapperScript(fs, compilerWrapper.view(), compilerLogPath.view(),
                                                        hostCompilerCpp.view(), "noisy compiler stdout",
                                                        "noisy compiler stderr"));

            Build::Action action                   = makeNativeCompileAction(directories, HeaderFixtureProjectName);
            action.parameters.toolchain.family     = Build::Toolchain::CustomDriver;
            action.parameters.execution.outputMode = Build::OutputMode::Normal;
            SC_TRUST_RESULT(action.parameters.toolchain.compilerC.assign(compilerWrapper.view()));
            SC_TRUST_RESULT(action.parameters.toolchain.compilerCpp.assign(compilerWrapper.view()));
            SC_TRUST_RESULT(action.parameters.toolchain.linker.assign(hostCompilerCpp.view()));
            SC_TRUST_RESULT(action.parameters.toolchain.archiver.assign(hostArchiver.view()));

            Result              buildResult = Result(true);
            CapturedBuildOutput capturedOutput;
            SC_TRUST_RESULT(captureBuildActionOutput(action, configureHeaderDependencyProgram, FixtureWorkspaceName,
                                                     buildResult, capturedOutput));
            SC_TEST_EXPECT(buildResult);
            SC_TEST_EXPECT(not StringView(capturedOutput.stdOut.view()).containsString("noisy compiler stdout"));
            SC_TEST_EXPECT(not StringView(capturedOutput.stdErr.view()).containsString("noisy compiler stderr"));
            SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view()).containsString("Build Summary:"));
        }

        if (test_section("native backend prints successful compile noise in verbose mode"))
        {
            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String sourceRoot    = StringEncoding::Utf8;
            String toolchainRoot = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(sourceRoot, {buildRoot.view(), "VerboseOutputModeFixture"}));
            SC_TRUST_RESULT(Path::join(toolchainRoot, {sourceRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(writeSourceFixture(fs, sourceRoot.view(),
                                               "#include <stdio.h>\n"
                                               "int main()\n"
                                               "{\n"
                                               "    puts(\"verbose-output-mode\");\n"
                                               "    return 0;\n"
                                               "}\n"));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolchainRoot.view()));
            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(sourceRoot.view()));

            String hostCompilerC   = StringEncoding::Utf8;
            String hostCompilerCpp = StringEncoding::Utf8;
            String hostArchiver    = StringEncoding::Utf8;
            SC_TRUST_RESULT(resolveHostToolPath("clang", hostCompilerC));
            SC_TRUST_RESULT(resolveHostToolPath("clang++", hostCompilerCpp));
            SC_TRUST_RESULT(resolveHostToolPath("ar", hostArchiver));

            String compilerLogPath = StringEncoding::Utf8;
            String compilerWrapper = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(compilerLogPath, {toolchainRoot.view(), "compiler.log"}));
            SC_TRUST_RESULT(Path::join(compilerWrapper, {toolchainRoot.view(), "compiler.sh"}));
            SC_TRUST_RESULT(fs.writeString(compilerLogPath.view(), ""));
            SC_TRUST_RESULT(writeNoisyToolWrapperScript(fs, compilerWrapper.view(), compilerLogPath.view(),
                                                        hostCompilerCpp.view(), "noisy compiler stdout",
                                                        "noisy compiler stderr"));

            Build::Action action                   = makeNativeCompileAction(directories, HeaderFixtureProjectName);
            action.parameters.toolchain.family     = Build::Toolchain::CustomDriver;
            action.parameters.execution.outputMode = Build::OutputMode::Verbose;
            SC_TRUST_RESULT(action.parameters.toolchain.compilerC.assign(compilerWrapper.view()));
            SC_TRUST_RESULT(action.parameters.toolchain.compilerCpp.assign(compilerWrapper.view()));
            SC_TRUST_RESULT(action.parameters.toolchain.linker.assign(hostCompilerCpp.view()));
            SC_TRUST_RESULT(action.parameters.toolchain.archiver.assign(hostArchiver.view()));

            Result              buildResult = Result(true);
            CapturedBuildOutput capturedOutput;
            SC_TRUST_RESULT(captureBuildActionOutput(action, configureHeaderDependencyProgram, FixtureWorkspaceName,
                                                     buildResult, capturedOutput));
            SC_TEST_EXPECT(buildResult);
            SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view()).containsString("OUTPUT:"));
            SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view()).containsString("noisy compiler stdout"));
            SC_TEST_EXPECT(StringView(capturedOutput.stdErr.view()).containsString("noisy compiler stderr"));
        }

        if (test_section("native backend fail-fast skips later workspace targets"))
        {
            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String programOneRoot = StringEncoding::Utf8;
            String programTwoRoot = StringEncoding::Utf8;
            String toolchainRoot  = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(programOneRoot, {buildRoot.view(), "ProgramOne"}));
            SC_TRUST_RESULT(Path::join(programTwoRoot, {buildRoot.view(), "ProgramTwo"}));
            SC_TRUST_RESULT(Path::join(toolchainRoot, {buildRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(writeSourceFixture(fs, programOneRoot.view(),
                                               "#include <stdio.h>\n"
                                               "int main()\n"
                                               "{\n"
                                               "    this_will_not_compile(\n"
                                               "}\n"));
            SC_TRUST_RESULT(writeIndependentProgramFixture(fs, programTwoRoot.view(), "program-two"));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolchainRoot.view()));
            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(buildRoot.view()));

            String hostCompilerC   = StringEncoding::Utf8;
            String hostCompilerCpp = StringEncoding::Utf8;
            String hostArchiver    = StringEncoding::Utf8;
            SC_TRUST_RESULT(resolveHostToolPath("clang", hostCompilerC));
            SC_TRUST_RESULT(resolveHostToolPath("clang++", hostCompilerCpp));
            SC_TRUST_RESULT(resolveHostToolPath("ar", hostArchiver));

            String compilerLogPath = StringEncoding::Utf8;
            String compilerWrapper = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(compilerLogPath, {toolchainRoot.view(), "compiler.log"}));
            SC_TRUST_RESULT(Path::join(compilerWrapper, {toolchainRoot.view(), "compiler.sh"}));
            SC_TRUST_RESULT(fs.writeString(compilerLogPath.view(), ""));
            SC_TRUST_RESULT(
                writeToolWrapperScript(fs, compilerWrapper.view(), compilerLogPath.view(), hostCompilerCpp.view()));

            Build::Action action                        = makeNativeCompileAction(directories, {});
            action.parameters.execution.maxParallelJobs = 1;
            action.parameters.execution.outputMode      = Build::OutputMode::Quiet;
            action.parameters.toolchain.family          = Build::Toolchain::CustomDriver;
            SC_TRUST_RESULT(action.parameters.toolchain.compilerC.assign(compilerWrapper.view()));
            SC_TRUST_RESULT(action.parameters.toolchain.compilerCpp.assign(compilerWrapper.view()));
            SC_TRUST_RESULT(action.parameters.toolchain.linker.assign(hostCompilerCpp.view()));
            SC_TRUST_RESULT(action.parameters.toolchain.archiver.assign(hostArchiver.view()));

            Result              buildResult = Result(true);
            CapturedBuildOutput capturedOutput;
            SC_TRUST_RESULT(captureBuildActionOutput(action, configureIndependentWorkspacePrograms,
                                                     FixtureWorkspaceName, buildResult, capturedOutput));
            SC_TEST_EXPECT(not buildResult);
            const StringView quietStdOut = capturedOutput.stdOut.view();
            SC_TEST_EXPECT(quietStdOut.isEmpty() or quietStdOut.bytesWithoutTerminator()[0] != '[');
            SC_TEST_EXPECT(not StringView(quietStdOut).containsString("\n[1/"));
            SC_TEST_EXPECT(StringView(quietStdOut).containsString("FAILED:"));

            String compilerLog = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(compilerLogPath.view(), compilerLog));
            SC_TEST_EXPECT(StringView(compilerLog.view()).containsString("ProgramOne/./main.cpp"));
            SC_TEST_EXPECT(not StringView(compilerLog.view()).containsString("ProgramTwo/./main.cpp"));
        }
#endif

#if SC_PLATFORM_WINDOWS
        if (test_section("native backend routes clang-cl toolchains"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String sourceRoot = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(sourceRoot, {buildRoot.view(), "ClangCLWorkspaceFixture"}));
            SC_TRUST_RESULT(writeWorkspaceDependencyFixture(fs, sourceRoot.view()));
            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(sourceRoot.view()));

            String clangClPath = StringEncoding::Utf8;
            String llvmLibPath = StringEncoding::Utf8;
            SC_TRUST_RESULT(resolveVisualStudioLLVMToolPath("clang-cl.exe", clangClPath));
            SC_TRUST_RESULT(resolveVisualStudioLLVMToolPath("llvm-lib.exe", llvmLibPath));

            Build::Action action               = makeNativeCompileAction(directories, WorkspaceExecutableProjectName);
            action.parameters.toolchain.family = Build::Toolchain::ClangCL;
            SC_TRUST_RESULT(action.parameters.toolchain.compilerC.assign(clangClPath.view()));
            SC_TRUST_RESULT(action.parameters.toolchain.compilerCpp.assign(clangClPath.view()));
            SC_TRUST_RESULT(action.parameters.toolchain.archiver.assign(llvmLibPath.view()));
            action.parameters.execution.maxParallelJobs = 2;

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
#endif

#if SC_PLATFORM_APPLE or SC_PLATFORM_LINUX
        if (test_section("native backend routes custom driver toolchains"))
        {
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
#endif
    }
};

void runSCBuildTest(SC::TestReport& report) { SCBuildFixtureTest test(report); }
} // namespace SC
