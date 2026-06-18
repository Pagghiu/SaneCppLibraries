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
#include "Tools/SC-package.h"
#include <stdlib.h>

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
static constexpr StringView ExternalFixtureProjectName     = "ExtFixture";
static constexpr StringView SelfHostingFixtureProjectName  = "SelfHostFx";
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
    case Build::Toolchain::FilC: compilerName = "filc"; return Result(true);
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

enum class FixturePackageLayout
{
    SharedRepository,
    IsolatedRun,
};

static Result createFixtureDirectories(TestReport& report, String& buildRoot, Build::Directories& directories,
                                       FixturePackageLayout packageLayout = FixturePackageLayout::SharedRepository)
{
    String targetDirectory = report.applicationRootDirectory.view();
    SC_TRY(Path::append(targetDirectory, {"../..", "_Tests"}, Path::AsNative));

    static size_t    fixtureRunCounter = 0;
    SmallString<128> runDirectory;
    fixtureRunCounter += 1;
    SC_TRY(StringBuilder::format(runDirectory, "sb-{}-{}", Time::Realtime::now().milliseconds, fixtureRunCounter));
    SC_TRY(Path::append(targetDirectory, {runDirectory.view()}, Path::AsNative));
    SC_TRY(Path::normalize(buildRoot, targetDirectory.view(), Path::AsNative));

    SC_TRY(Path::join(directories.projectsDirectory, {buildRoot.view(), "_Projects"}));
    SC_TRY(Path::join(directories.outputsDirectory, {buildRoot.view(), "_Outputs"}));
    SC_TRY(Path::join(directories.intermediatesDirectory, {buildRoot.view(), "_Intermediates"}));
    SC_TRY(Path::join(directories.buildCacheDirectory, {buildRoot.view(), "_BuildCache"}));
    if (packageLayout == FixturePackageLayout::SharedRepository)
    {
        SC_TRY(Path::join(directories.packagesCacheDirectory,
                          {report.libraryRootDirectory.view(), "_Build", "_PackagesCache"}));
        SC_TRY(Path::join(directories.packagesInstallDirectory,
                          {report.libraryRootDirectory.view(), "_Build", "_Packages"}));
    }
    else
    {
        SC_TRY(Path::join(directories.packagesCacheDirectory, {buildRoot.view(), "_PackagesCache"}));
        SC_TRY(Path::join(directories.packagesInstallDirectory, {buildRoot.view(), "_Packages"}));
    }
    directories.libraryDirectory = report.libraryRootDirectory.view();
    directories.projectDirectory = report.libraryRootDirectory.view();
    return Result(true);
}

#if SC_PLATFORM_WINDOWS
static Result appendDeepWindowsFixtureRoot(StringView buildRoot, StringView fixtureDirectoryName, String& projectRoot)
{
    SC_TRY(projectRoot.assign(buildRoot));
    for (size_t idx = 0; idx < 6; ++idx)
    {
        SmallString<64> segment;
        SC_TRY(StringBuilder::format(segment, "windows-long-path-segment-{:02}", idx));
        SC_TRY(Path::append(projectRoot, {segment.view()}, Path::AsNative));
    }
    SC_TRY(Path::append(projectRoot, {fixtureDirectoryName}, Path::AsNative));
    return Result(true);
}
#endif

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

#if SC_PLATFORM_APPLE
static bool tryResolveHostToolPath(StringView toolName, String& toolPath)
{
    Process process;
    String  output = StringEncoding::Utf8;
    if (not process.exec({"which", toolName}, output) or process.getExitStatus() != 0)
    {
        return false;
    }
    return toolPath.assign(StringView(output.view()).trimWhiteSpaces());
}

static bool tryResolveHostToolPath(Span<const StringView> toolNames, String& toolPath)
{
    for (const StringView toolName : toolNames)
    {
        if (tryResolveHostToolPath(toolName, toolPath))
        {
            return true;
        }
    }
    return false;
}
#endif

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

static Result writeOutputProducingWrapperScript(FileSystem& fs, StringView scriptPath, StringView logPath)
{
    String scriptContents = StringEncoding::Utf8;
    SC_TRY(StringBuilder::format(scriptContents,
                                 "#!/bin/sh\n"
                                 "printf '%s\\n' \"$*\" >> \"{}\"\n"
                                 "out=''\n"
                                 "prev=''\n"
                                 "for arg in \"$@\"; do\n"
                                 "  if [ \"$prev\" = '-o' ]; then\n"
                                 "    out=\"$arg\"\n"
                                 "    prev=''\n"
                                 "    continue\n"
                                 "  fi\n"
                                 "  case \"$arg\" in\n"
                                 "    -o) prev='-o' ;;\n"
                                 "  esac\n"
                                 "done\n"
                                 "if [ -n \"$out\" ]; then\n"
                                 "  /bin/mkdir -p \"$(/usr/bin/dirname \"$out\")\"\n"
                                 "  : > \"$out\"\n"
                                 "fi\n"
                                 "exit 0\n",
                                 logPath));
    SC_TRY(fs.writeString(scriptPath, scriptContents.view()));
    SC_TRY(fs.chmod(scriptPath, 0755u));
    return Result(true);
}

#if SC_PLATFORM_APPLE
static Result writeVersionedOutputProducingWrapperScript(FileSystem& fs, StringView scriptPath, StringView logPath,
                                                         StringView versionText)
{
    String scriptContents = StringEncoding::Utf8;
    auto   builder        = StringBuilder::create(scriptContents);
    SC_TRY(builder.append("#!/bin/sh\n"));
    SC_TRY(builder.append("printf '%s\\n' \"$*\" >> \"{}\"\n", logPath));
    SC_TRY(builder.append("if [ \"$1\" = \"--version\" ]; then\n"));
    SC_TRY(builder.append("  printf '%s\\n' '{}'\n", versionText));
    SC_TRY(builder.append("  exit 0\n"));
    SC_TRY(builder.append("fi\n"));
    SC_TRY(builder.append("out=''\n"));
    SC_TRY(builder.append("prev=''\n"));
    SC_TRY(builder.append("for arg in \"$@\"; do\n"));
    SC_TRY(builder.append("  if [ \"$prev\" = '-o' ]; then\n"));
    SC_TRY(builder.append("    out=\"$arg\"\n"));
    SC_TRY(builder.append("    prev=''\n"));
    SC_TRY(builder.append("    continue\n"));
    SC_TRY(builder.append("  fi\n"));
    SC_TRY(builder.append("  case \"$arg\" in\n"));
    SC_TRY(builder.append("    -o) prev='-o' ;;\n"));
    SC_TRY(builder.append("  esac\n"));
    SC_TRY(builder.append("done\n"));
    SC_TRY(builder.append("if [ -n \"$out\" ]; then\n"));
    SC_TRY(builder.append("  /bin/mkdir -p \"$(/usr/bin/dirname \"$out\")\"\n"));
    SC_TRY(builder.append("  : > \"$out\"\n"));
    SC_TRY(builder.append("fi\n"));
    SC_TRY(builder.append("exit 0\n"));
    builder.finalize();
    SC_TRY(fs.writeString(scriptPath, scriptContents.view()));
    SC_TRY(fs.chmod(scriptPath, 0755u));
    return Result(true);
}
#endif

#if SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
static Result writeVersionedLoggingOnlyWrapperScript(FileSystem& fs, StringView scriptPath, StringView logPath,
                                                     StringView stdOutText = {})
{
    String scriptContents = StringEncoding::Utf8;
    auto   builder        = StringBuilder::create(scriptContents);
    SC_TRY(builder.append("#!/bin/sh\n"));
    SC_TRY(builder.append("printf '%s\\n' \"$*\" >> \"{}\"\n", logPath));
    SC_TRY(builder.append("if [ \"$1\" = \"--version\" ]; then\n"));
    if (stdOutText.isEmpty())
    {
        SC_TRY(builder.append("  printf '%s\\n' 'fake version'\n"));
    }
    else
    {
        SC_TRY(builder.append("  printf '%s\\n' '{}'\n", stdOutText));
    }
    SC_TRY(builder.append("  exit 0\n"));
    SC_TRY(builder.append("fi\n"));
    if (not stdOutText.isEmpty())
    {
        SC_TRY(builder.append("printf '%s\\n' '{}'\n", stdOutText));
    }
    SC_TRY(builder.append("exit 0\n"));
    builder.finalize();
    SC_TRY(fs.writeString(scriptPath, scriptContents.view()));
    SC_TRY(fs.chmod(scriptPath, 0755u));
    return Result(true);
}

static Result writeFakeQEMUWrapperScript(FileSystem& fs, StringView scriptPath, StringView logPath,
                                         StringView versionText)
{
    String scriptContents = StringEncoding::Utf8;
    auto   builder        = StringBuilder::create(scriptContents);
    SC_TRY(builder.append("#!/bin/sh\n"));
    SC_TRY(builder.append("printf '%s\\n' \"$*\" >> \"{}\"\n", logPath));
    SC_TRY(builder.append("if [ \"$1\" = \"--version\" ]; then\n"));
    SC_TRY(builder.append("  printf '%s\\n' '{}'\n", versionText));
    SC_TRY(builder.append("  exit 0\n"));
    SC_TRY(builder.append("fi\n"));
    SC_TRY(builder.append("exit 0\n"));
    builder.finalize();
    SC_TRY(fs.writeString(scriptPath, scriptContents.view()));
    SC_TRY(fs.chmod(scriptPath, 0755u));
    return Result(true);
}

static Result createFakeImportedQEMURunner(FileSystem& fs, StringView runnerRoot, StringView qemuX86Log,
                                           StringView qemuArm64Log)
{
    String binDirectory = StringEncoding::Utf8;
    String qemuX86Path  = StringEncoding::Utf8;
    String qemuArmPath  = StringEncoding::Utf8;
    SC_TRY(Path::join(binDirectory, {runnerRoot, "bin"}));
    SC_TRY(Path::join(qemuX86Path, {binDirectory.view(), "qemu-x86_64"}));
    SC_TRY(Path::join(qemuArmPath, {binDirectory.view(), "qemu-aarch64"}));
    SC_TRY(fs.makeDirectoryRecursive(binDirectory.view()));
    SC_TRY(writeFakeQEMUWrapperScript(fs, qemuX86Path.view(), qemuX86Log, "qemu-x86_64 version 10.0.0"));
    SC_TRY(writeFakeQEMUWrapperScript(fs, qemuArmPath.view(), qemuArm64Log, "qemu-aarch64 version 10.0.0"));
    return Result(true);
}
#endif

#if SC_PLATFORM_LINUX
static Result writeBox64ForwardingWrapperScript(FileSystem& fs, StringView scriptPath, StringView logPath)
{
    String scriptContents = StringEncoding::Utf8;
    auto   builder        = StringBuilder::create(scriptContents);
    SC_TRY(builder.append("#!/bin/sh\n"));
    SC_TRY(builder.append("printf '%s\\n' \"$*\" >> \"{}\"\n", logPath));
    SC_TRY(builder.append("if [ \"$1\" = \"--version\" ]; then\n"));
    SC_TRY(builder.append("  printf '%s\\n' 'box64 version'\n"));
    SC_TRY(builder.append("  exit 0\n"));
    SC_TRY(builder.append("fi\n"));
    SC_TRY(builder.append("tool=\"$1\"\n"));
    SC_TRY(builder.append("shift\n"));
    SC_TRY(builder.append("exec \"$tool\" \"$@\"\n"));
    builder.finalize();
    SC_TRY(fs.writeString(scriptPath, scriptContents.view()));
    SC_TRY(fs.chmod(scriptPath, 0755u));
    return Result(true);
}

static Result createFakePackagedLinuxWineRunner(FileSystem& fs, StringView runnerRoot, StringView box64Log,
                                                StringView wineLog, StringView wineConsoleLog)
{
    String box64Path         = StringEncoding::Utf8;
    String amd64LibDirectory = StringEncoding::Utf8;
    String winePath          = StringEncoding::Utf8;
    String wineConsolePath   = StringEncoding::Utf8;
    SC_TRY(Path::join(box64Path, {runnerRoot, "box64", "usr", "bin", "box64"}));
    SC_TRY(Path::join(amd64LibDirectory, {runnerRoot, "amd64-libs", "usr", "lib", "x86_64-linux-gnu"}));
    SC_TRY(Path::join(winePath, {runnerRoot, "wine", "opt", "wine-stable", "bin", "wine"}));
    SC_TRY(Path::join(wineConsolePath, {runnerRoot, "wine", "opt", "wine-stable", "bin", "wineconsole"}));
    SC_TRY(fs.makeDirectoryRecursive(Path::dirname(box64Path.view(), Path::AsNative)));
    SC_TRY(fs.makeDirectoryRecursive(amd64LibDirectory.view()));
    SC_TRY(fs.makeDirectoryRecursive(Path::dirname(winePath.view(), Path::AsNative)));
    SC_TRY(writeBox64ForwardingWrapperScript(fs, box64Path.view(), box64Log));
    SC_TRY(writeVersionedLoggingOnlyWrapperScript(fs, winePath.view(), wineLog, "wine-11.0"));
    SC_TRY(writeVersionedLoggingOnlyWrapperScript(fs, wineConsolePath.view(), wineConsoleLog, "console"));

    String binDirectory = StringEncoding::Utf8;
    SC_TRY(Path::join(binDirectory, {runnerRoot, "bin"}));
    SC_TRY(fs.makeDirectoryRecursive(binDirectory.view()));

    auto writeWrapper = [&](StringView executableName, StringView targetPath) -> Result
    {
        String scriptPath = StringEncoding::Utf8;
        SC_TRY(Path::join(scriptPath, {binDirectory.view(), executableName}));

        String scriptContents = StringEncoding::Utf8;
        auto   builder        = StringBuilder::create(scriptContents);
        SC_TRY(builder.append("#!/bin/sh\n"));
        SC_TRY(builder.append("case \"$0\" in\n"));
        SC_TRY(builder.append("  */*) SCRIPT_DIR=${0%/*} ;;\n"));
        SC_TRY(builder.append("  *) SCRIPT_DIR=. ;;\n"));
        SC_TRY(builder.append("esac\n"));
        SC_TRY(builder.append("SCRIPT_DIR=$(CDPATH= cd -- \"$SCRIPT_DIR\" && pwd)\n"));
        SC_TRY(builder.append("RUNNER_ROOT=$(CDPATH= cd -- \"$SCRIPT_DIR/..\" && pwd)\n"));
        SC_TRY(builder.append("exec \"$RUNNER_ROOT/box64/usr/bin/box64\" \"$RUNNER_ROOT/{}\" \"$@\"\n", targetPath));
        builder.finalize();

        SC_TRY(fs.writeString(scriptPath.view(), scriptContents.view()));
        SC_TRY(fs.chmod(scriptPath.view(), 0755u));
        return Result(true);
    };

    SC_TRY(writeWrapper("wine", "wine/opt/wine-stable/bin/wine"));
    SC_TRY(writeWrapper("wineconsole", "wine/opt/wine-stable/bin/wineconsole"));
    return Result(true);
}
#endif

struct ScopedEnvironmentVariable
{
    String name         = StringEncoding::Utf8;
    String previous     = StringEncoding::Utf8;
    bool   hadPrevious  = false;
    bool   restoreValue = false;

    ~ScopedEnvironmentVariable()
    {
        if (not restoreValue)
        {
            return;
        }
#if SC_PLATFORM_WINDOWS
        (void)::SetEnvironmentVariableA(name.bytesIncludingTerminator(),
                                        hadPrevious ? previous.bytesIncludingTerminator() : nullptr);
#else
        if (hadPrevious)
        {
            (void)::setenv(name.bytesIncludingTerminator(), previous.bytesIncludingTerminator(), 1);
        }
        else
        {
            (void)::unsetenv(name.bytesIncludingTerminator());
        }
#endif
    }
};

static Result setScopedEnvironmentVariable(StringView name, StringView value, ScopedEnvironmentVariable& scoped)
{
    SC_TRY(scoped.name.assign(name));
    scoped.restoreValue = true;
#if SC_PLATFORM_WINDOWS
    const char* existing = ::getenv(scoped.name.bytesIncludingTerminator());
    if (existing)
    {
        scoped.hadPrevious = true;
        SC_TRY(scoped.previous.assign(StringView::fromNullTerminated(existing, StringEncoding::Native)));
    }
    SC_TRY_MSG(::SetEnvironmentVariableA(scoped.name.bytesIncludingTerminator(), value.bytesIncludingTerminator()) != 0,
               "Failed setting environment variable");
#else
    const char* existing = ::getenv(scoped.name.bytesIncludingTerminator());
    if (existing)
    {
        scoped.hadPrevious = true;
        SC_TRY(scoped.previous.assign(StringView::fromNullTerminated(existing, StringEncoding::Native)));
    }
    SC_TRY_MSG(::setenv(scoped.name.bytesIncludingTerminator(), value.bytesIncludingTerminator(), 1) == 0,
               "Failed setting environment variable");
#endif
    return Result(true);
}

#if SC_PLATFORM_LINUX
static Result unsetScopedEnvironmentVariable(StringView name, ScopedEnvironmentVariable& scoped)
{
    SC_TRY(scoped.name.assign(name));
    scoped.restoreValue = true;
#if SC_PLATFORM_WINDOWS
    const char* existing = ::getenv(scoped.name.bytesIncludingTerminator());
    if (existing)
    {
        scoped.hadPrevious = true;
        SC_TRY(scoped.previous.assign(StringView::fromNullTerminated(existing, StringEncoding::Native)));
    }
    SC_TRY_MSG(::SetEnvironmentVariableA(scoped.name.bytesIncludingTerminator(), nullptr) != 0,
               "Failed clearing environment variable");
#else
    const char* existing = ::getenv(scoped.name.bytesIncludingTerminator());
    if (existing)
    {
        scoped.hadPrevious = true;
        SC_TRY(scoped.previous.assign(StringView::fromNullTerminated(existing, StringEncoding::Native)));
    }
    SC_TRY_MSG(::unsetenv(scoped.name.bytesIncludingTerminator()) == 0, "Failed clearing environment variable");
#endif
    return Result(true);
}
#endif

#if SC_PLATFORM_APPLE
static bool isEnvironmentFlagEnabled(const char* name)
{
    const char* value = ::getenv(name);
    return value != nullptr and value[0] != '\0' and value[0] != '0';
}
#endif

static Result writeFakeMSVCWineScript(FileSystem& fs, StringView scriptPath, StringView logPath)
{
    String scriptContents = StringEncoding::Utf8;
    auto   builder        = StringBuilder::create(scriptContents);
    SC_TRY(builder.append("#!/bin/sh\n"));
    SC_TRY(builder.append("log_path='{}'\n", logPath));
    SC_TRY(builder.append("printf '%s\\n' \"$*\" >> \"$log_path\"\n"));
    SC_TRY(builder.append("tool=\"$1\"\n"));
    SC_TRY(builder.append("shift\n"));
    SC_TRY(builder.append("for arg in \"$@\"; do\n"));
    SC_TRY(builder.append("  if [ \"$arg\" = '/?' ]; then\n"));
    SC_TRY(builder.append("    printf '%s\\n' 'Microsoft (R) C/C++ Optimizing Compiler'\n"));
    SC_TRY(builder.append("    exit 0\n"));
    SC_TRY(builder.append("  fi\n"));
    SC_TRY(builder.append("done\n"));
    SC_TRY(builder.append("to_posix() {\n"));
    SC_TRY(builder.append("  printf '%s' \"$1\" | /usr/bin/tr '\\\\' '/' | /usr/bin/sed 's#^Z:##'\n"));
    SC_TRY(builder.append("}\n"));
    SC_TRY(builder.append("for arg in \"$@\"; do\n"));
    SC_TRY(builder.append("  case \"$arg\" in\n"));
    SC_TRY(builder.append("    /Fo*) out=$(printf '%s' \"$arg\" | /usr/bin/sed 's#^/Fo##'); out=$(to_posix \"$out\"); "
                          "/bin/mkdir -p \"$(/usr/bin/dirname \"$out\")\"; : > \"$out\" ;;\n"));
    SC_TRY(builder.append("    /OUT:*) out=$(printf '%s' \"$arg\" | /usr/bin/sed 's#^/OUT:##'); out=$(to_posix "
                          "\"$out\"); /bin/mkdir -p \"$(/usr/bin/dirname \"$out\")\"; : > \"$out\" ;;\n"));
    SC_TRY(builder.append("  esac\n"));
    SC_TRY(builder.append("done\n"));
    SC_TRY(builder.append("case \"$tool\" in\n"));
    SC_TRY(builder.append("  *cl.exe) printf '%s\\n' 'Note: including file: Z:\\\\fake\\\\include\\\\stdio.h' ;;\n"));
    SC_TRY(builder.append("esac\n"));
    SC_TRY(builder.append("exit 0\n"));
    builder.finalize();
    SC_TRY(fs.writeString(scriptPath, scriptContents.view()));
    SC_TRY(fs.chmod(scriptPath, 0755u));
    return Result(true);
}

static Result createFakeWineBundle(FileSystem& fs, StringView bundleRoot, StringView logPath, bool supportsX64,
                                   bool supportsArm64, StringView consoleLogPath = {},
                                   StringView stdOutText = "wrapped")
{
    String wineBinDirectory = StringEncoding::Utf8;
    String wineLibraryRoot  = StringEncoding::Utf8;
    String winePath         = StringEncoding::Utf8;
    SC_TRY(Path::join(wineBinDirectory, {bundleRoot, "wine", "bin"}));
    SC_TRY(Path::join(wineLibraryRoot, {bundleRoot, "wine", "lib", "wine"}));
    SC_TRY(Path::join(winePath, {wineBinDirectory.view(), "wine"}));
    SC_TRY(fs.makeDirectoryRecursive(wineBinDirectory.view()));
    SC_TRY(fs.makeDirectoryRecursive(wineLibraryRoot.view()));
    if (supportsX64)
    {
        String x64Loader = StringEncoding::Utf8;
        SC_TRY(Path::join(x64Loader, {wineLibraryRoot.view(), "x86_64-windows"}));
        SC_TRY(fs.makeDirectoryRecursive(x64Loader.view()));
    }
    if (supportsArm64)
    {
        String arm64Loader = StringEncoding::Utf8;
        SC_TRY(Path::join(arm64Loader, {wineLibraryRoot.view(), "arm64-windows"}));
        SC_TRY(fs.makeDirectoryRecursive(arm64Loader.view()));
    }
    SC_TRY(writeLoggingOnlyWrapperScript(fs, winePath.view(), logPath, stdOutText));

    if (not consoleLogPath.isEmpty())
    {
        String wineConsole = StringEncoding::Utf8;
        SC_TRY(Path::join(wineConsole, {wineBinDirectory.view(), "wineconsole"}));
        SC_TRY(writeLoggingOnlyWrapperScript(fs, wineConsole.view(), consoleLogPath, "console"));
    }
    return Result(true);
}

static Result createPortableMSVCImportFixture(FileSystem& fs, StringView rootDirectory)
{
    static constexpr StringView msvcVersion = "14.40.33807";
    static constexpr StringView sdkVersion  = "10.0.26100.0";

    String msvcInclude      = StringEncoding::Utf8;
    String sdkBinDirectory  = StringEncoding::Utf8;
    String sdkIncludeUM     = StringEncoding::Utf8;
    String sdkIncludeShared = StringEncoding::Utf8;
    String sdkIncludeUCRT   = StringEncoding::Utf8;
    String sdkIncludeWinRT  = StringEncoding::Utf8;
    String sdkIncludeCpp    = StringEncoding::Utf8;

    SC_TRY(Path::join(msvcInclude, {rootDirectory, "VC", "Tools", "MSVC", msvcVersion, "include"}));
    SC_TRY(Path::join(sdkBinDirectory, {rootDirectory, "Windows Kits", "10", "bin", sdkVersion, "x64"}));
    SC_TRY(Path::join(sdkIncludeUM, {rootDirectory, "Windows Kits", "10", "Include", sdkVersion, "um"}));
    SC_TRY(Path::join(sdkIncludeShared, {rootDirectory, "Windows Kits", "10", "Include", sdkVersion, "shared"}));
    SC_TRY(Path::join(sdkIncludeUCRT, {rootDirectory, "Windows Kits", "10", "Include", sdkVersion, "ucrt"}));
    SC_TRY(Path::join(sdkIncludeWinRT, {rootDirectory, "Windows Kits", "10", "Include", sdkVersion, "winrt"}));
    SC_TRY(Path::join(sdkIncludeCpp, {rootDirectory, "Windows Kits", "10", "Include", sdkVersion, "cppwinrt"}));

    SC_TRY(fs.makeDirectoryRecursive(msvcInclude.view()));
    SC_TRY(fs.makeDirectoryRecursive(sdkBinDirectory.view()));
    SC_TRY(fs.makeDirectoryRecursive(sdkIncludeUM.view()));
    SC_TRY(fs.makeDirectoryRecursive(sdkIncludeShared.view()));
    SC_TRY(fs.makeDirectoryRecursive(sdkIncludeUCRT.view()));
    SC_TRY(fs.makeDirectoryRecursive(sdkIncludeWinRT.view()));
    SC_TRY(fs.makeDirectoryRecursive(sdkIncludeCpp.view()));

    static constexpr StringView targetArchitectures[] = {"x64", "arm64"};
    static constexpr StringView toolNames[]           = {"cl.exe", "link.exe", "lib.exe"};
    for (const StringView targetArchitecture : targetArchitectures)
    {
        String msvcBinDirectory = StringEncoding::Utf8;
        String msvcLibrary      = StringEncoding::Utf8;
        String sdkLibUM         = StringEncoding::Utf8;
        String sdkLibUCRT       = StringEncoding::Utf8;
        SC_TRY(Path::join(msvcBinDirectory,
                          {rootDirectory, "VC", "Tools", "MSVC", msvcVersion, "bin", "Hostx64", targetArchitecture}));
        SC_TRY(Path::join(msvcLibrary, {rootDirectory, "VC", "Tools", "MSVC", msvcVersion, "lib", targetArchitecture}));
        SC_TRY(
            Path::join(sdkLibUM, {rootDirectory, "Windows Kits", "10", "Lib", sdkVersion, "um", targetArchitecture}));
        SC_TRY(Path::join(sdkLibUCRT,
                          {rootDirectory, "Windows Kits", "10", "Lib", sdkVersion, "ucrt", targetArchitecture}));

        SC_TRY(fs.makeDirectoryRecursive(msvcBinDirectory.view()));
        SC_TRY(fs.makeDirectoryRecursive(msvcLibrary.view()));
        SC_TRY(fs.makeDirectoryRecursive(sdkLibUM.view()));
        SC_TRY(fs.makeDirectoryRecursive(sdkLibUCRT.view()));

        for (const StringView toolName : toolNames)
        {
            String toolPath = StringEncoding::Utf8;
            SC_TRY(Path::join(toolPath, {msvcBinDirectory.view(), toolName}));
            SC_TRY(fs.writeString(toolPath.view(), ""));
        }
    }
    return Result(true);
}
#endif

#if SC_PLATFORM_LINUX
static Result writeFilCForwardingWrapperScript(FileSystem& fs, StringView scriptPath, StringView logPath,
                                               StringView toolPath, StringView installedDir,
                                               StringView targetTriple = "x86_64-unknown-linux-gnu")
{
    String scriptContents = StringEncoding::Utf8;
    auto   builder        = StringBuilder::create(scriptContents);
    SC_TRY(builder.append("#!/bin/sh\n"));
    SC_TRY(builder.append("printf '%s\\n' \"$*\" >> \"{}\"\n", logPath));
    SC_TRY(builder.append("if [ \"$1\" = \"--version\" ]; then\n"));
    SC_TRY(builder.append("  printf '%s\\n' 'Fil-C 0.678 clang version 20.1.8'\n"));
    SC_TRY(builder.append("  printf '%s\\n' 'Target: {}'\n", targetTriple));
    SC_TRY(builder.append("  printf '%s\\n' 'Thread model: posix'\n"));
    SC_TRY(builder.append("  printf '%s\\n' 'InstalledDir: {}'\n", installedDir));
    SC_TRY(builder.append("  printf '%s\\n' 'Build config: +assertions'\n"));
    SC_TRY(builder.append("  exit 0\n"));
    SC_TRY(builder.append("fi\n"));
    SC_TRY(builder.append("exec \"{}\" \"$@\"\n", toolPath));
    builder.finalize();
    SC_TRY(fs.writeString(scriptPath, scriptContents.view()));
    SC_TRY(fs.chmod(scriptPath, 0755u));
    return Result(true);
}

static Result createFakeFilCImportFixture(FileSystem& fs, StringView rootDirectory, StringView compilerCLogPath,
                                          StringView compilerCppLogPath,
                                          StringView targetTriple = "x86_64-unknown-linux-gnu")
{
    String binDirectory = StringEncoding::Utf8;
    String clangPath    = StringEncoding::Utf8;
    String clangCppPath = StringEncoding::Utf8;
    SC_TRY(Path::join(binDirectory, {rootDirectory, "build", "bin"}));
    SC_TRY(Path::join(clangPath, {binDirectory.view(), "clang"}));
    SC_TRY(Path::join(clangCppPath, {binDirectory.view(), "clang++"}));
    SC_TRY(fs.makeDirectoryRecursive(binDirectory.view()));

    String hostClang    = StringEncoding::Utf8;
    String hostClangCpp = StringEncoding::Utf8;
    SC_TRY(resolveHostToolPath("clang", hostClang));
    SC_TRY(resolveHostToolPath("clang++", hostClangCpp));

    SC_TRY(fs.writeString(compilerCLogPath, ""));
    SC_TRY(fs.writeString(compilerCppLogPath, ""));
    SC_TRY(writeFilCForwardingWrapperScript(fs, clangPath.view(), compilerCLogPath, hostClang.view(),
                                            binDirectory.view(), targetTriple));
    SC_TRY(writeFilCForwardingWrapperScript(fs, clangCppPath.view(), compilerCppLogPath, hostClangCpp.view(),
                                            binDirectory.view(), targetTriple));
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

    String findPattern = StringEncoding::Utf8;
    SC_TRY(StringBuilder::format(findPattern, "VC\\Tools\\Llvm\\x64\\bin\\{}", executableName));

    Process process;
    String  output = StringEncoding::Utf8;
    SC_TRY(process.exec({vswherePath.view(), "-latest", "-find", findPattern.view()}, output));
    SC_TRY_MSG(process.getExitStatus() == 0, "Cannot locate Visual Studio bundled LLVM tool");
    SC_TRY(toolPath.assign(StringView(output.view()).trimWhiteSpaces()));
    SC_TRY_MSG(fs.existsAndIsFile(toolPath.view()), "Bundled LLVM tool is missing");
    return Result(true);
}
#endif

static Result configureTinyConsoleProgram(Build::Definition& definition, const Build::Parameters& parameters)
{
    Build::Workspace workspace = {FixtureWorkspaceName};
    Build::Project   project   = {FixtureProjectName, Build::TargetType::ConsoleExecutable};

    SC_TRY(project.setRootDirectory(parameters.directories.projectDirectory.view()));
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
    Build::Project   project   = {SmallFixtureProjectName, Build::TargetType::ConsoleExecutable};

    SC_TRY(project.setRootDirectory(parameters.directories.projectDirectory.view()));
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
    Build::Project   project   = {StaticLibraryConsumerName, Build::TargetType::ConsoleExecutable};

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

    Build::Project libraryProject = {WorkspaceLibraryProjectName, Build::TargetType::StaticLibrary};
    SC_TRY(libraryProject.setRootDirectory(libraryRoot.view()));
    SC_TRY(libraryProject.addPresetConfiguration(Build::Configuration::Preset::Debug, parameters));
    SC_TRY(libraryProject.addPresetConfiguration(Build::Configuration::Preset::Release, parameters));
    SC_TRY(libraryProject.addFiles(".", "*.cpp"));

    Build::Project executableProject = {WorkspaceExecutableProjectName, Build::TargetType::ConsoleExecutable};
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

    Build::Project programOne = {IndependentProgramOneName, Build::TargetType::ConsoleExecutable};
    SC_TRY(programOne.setRootDirectory(programOneRoot.view()));
    SC_TRY(programOne.addPresetConfiguration(Build::Configuration::Preset::Debug, parameters));
    SC_TRY(programOne.addPresetConfiguration(Build::Configuration::Preset::Release, parameters));
    SC_TRY(programOne.addFiles(".", "*.cpp"));

    Build::Project programTwo = {IndependentProgramTwoName, Build::TargetType::ConsoleExecutable};
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

    Build::Project libraryProject = {WorkspaceLibraryProjectName, Build::TargetType::StaticLibrary};
    SC_TRY(libraryProject.setRootDirectory(libraryRoot.view()));
    SC_TRY(libraryProject.addPresetConfiguration(Build::Configuration::Preset::Debug, parameters));
    SC_TRY(libraryProject.addPresetConfiguration(Build::Configuration::Preset::Release, parameters));
    SC_TRY(libraryProject.addFiles(".", "*.c"));
    SC_TRY(libraryProject.addFiles(".", "*.cpp"));

    Build::Project executableProject = {WorkspaceExecutableProjectName, Build::TargetType::ConsoleExecutable};
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
    Build::Project   project   = {projectName, targetType};

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
    case Build::Platform::Linux:
        switch (action.parameters.targetMachine.environment)
        {
        case Build::TargetEnvironment::LinuxGlibc: targetOS = "linux-glibc"; break;
        case Build::TargetEnvironment::LinuxMusl: targetOS = "linux-musl"; break;
        case Build::TargetEnvironment::Native:
        case Build::TargetEnvironment::WindowsGNU:
        case Build::TargetEnvironment::WindowsMSVC: targetOS = "linux"; break;
        }
        break;
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
    (void)(output);
#endif
    return Result(true);
}

static Result captureBuildActionOutput(const Build::Action& action, Build::Action::ConfigureFunction configure,
                                       Result& buildResult, CapturedBuildOutput& capturedOutput)
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
    buildResult   = Build::Action::execute(action, configure);
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

    buildResult = Build::Action::execute(action, configure);
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

#if SC_PLATFORM_LINUX
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

static Result captureExternalBuildCommand(TestReport& report, StringView workingDirectory,
                                          Span<const StringSpan> arguments, CapturedProcessOutput& capturedOutput,
                                          StringView projectDirectoryOverride = {})
{
    String scriptPath = StringEncoding::Utf8;
#if SC_PLATFORM_WINDOWS
    SC_TRY(Path::join(scriptPath, {report.libraryRootDirectory.view(), "SC-build.bat"}));

    StringSpan processArguments[48];
    size_t     numArguments          = 0;
    processArguments[numArguments++] = "cmd";
    processArguments[numArguments++] = "/d";
    processArguments[numArguments++] = "/c";
    processArguments[numArguments++] = scriptPath.view();
    if (not projectDirectoryOverride.isEmpty())
    {
        processArguments[numArguments++] = "--project-dir";
        processArguments[numArguments++] = projectDirectoryOverride;
    }
#else
    (void)(projectDirectoryOverride);
    SC_TRY(Path::join(scriptPath, {report.libraryRootDirectory.view(), "SC-build.sh"}));

    StringSpan processArguments[48];
    size_t     numArguments          = 0;
    processArguments[numArguments++] = scriptPath.view();
#endif
    for (const StringSpan argument : arguments)
    {
        SC_TRY_MSG(numArguments < sizeof(processArguments) / sizeof(processArguments[0]), "Too many process arguments");
        processArguments[numArguments++] = argument;
    }

    Process process;
#if SC_PLATFORM_WINDOWS
    // cmd.exe cannot start from an extended-length current directory, so deep Windows fixtures pass the project
    // root explicitly and launch from the shorter library checkout.
    SC_TRY(process.setWorkingDirectory(projectDirectoryOverride.isEmpty() ? workingDirectory
                                                                          : report.libraryRootDirectory.view()));
#else
    SC_TRY(process.setWorkingDirectory(workingDirectory));
#endif
    SC_TRY(process.exec({processArguments, numArguments}, capturedOutput.stdOut, {}, capturedOutput.stdErr));
    capturedOutput.exitStatus = process.getExitStatus();
    SC_TRY(normalizeConsoleOutput(capturedOutput.stdOut));
    SC_TRY(normalizeConsoleOutput(capturedOutput.stdErr));
    return Result(true);
}

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

static Result writeExternalBuildFixture(FileSystem& fs, StringView projectRoot, Build::Libraries librariesMode)
{
    SC_TRY(fs.makeDirectoryRecursive(projectRoot));

    String sourceRoot = StringEncoding::Utf8;
    SC_TRY(Path::join(sourceRoot, {projectRoot, "Source"}));
    SC_TRY(fs.makeDirectoryRecursive(sourceRoot.view()));

    String buildDefinitionPath = StringEncoding::Utf8;
    String sourcePath          = StringEncoding::Utf8;
    SC_TRY(Path::join(buildDefinitionPath, {projectRoot, "SC-build.cpp"}));
    SC_TRY(Path::join(sourcePath, {sourceRoot.view(), "main.cpp"}));

    const StringView librariesArgument =
        librariesMode == Build::Libraries::Multiple ? ", Libraries::Multiple"_a8 : StringView();
    const StringView fixtureMessage =
        librariesMode == Build::Libraries::Multiple ? "external-build-multiple"_a8 : "external-build-single-file"_a8;

    String buildDefinitionContents = StringEncoding::Utf8;
    {
        auto builder = StringBuilder::create(buildDefinitionContents);
        SC_TRY(builder.append("#include \"SaneCppBuild.h\"\n"
                              "\n"
                              "namespace SC\n"
                              "{\n"
                              "namespace Build\n"
                              "{\n"
                              "Result configure(Definition& definition, const Parameters& parameters)\n"
                              "{\n"
                              "    Project project = {\""));
        SC_TRY(builder.append(ExternalFixtureProjectName));
        SC_TRY(builder.append("\", TargetType::ConsoleExecutable};\n"
                              "\n"
                              "    SC_TRY(project.setRootDirectory(parameters.directories.projectDirectory.view()));\n"
                              "    SC_TRY(addSaneCppLibraries(project, parameters"));
        SC_TRY(builder.append(librariesArgument));
        SC_TRY(
            builder.append("));\n"
                           "    SC_TRY(project.addPresetConfiguration(Configuration::Preset::Debug, parameters));\n"
                           "    SC_TRY(project.addPresetConfiguration(Configuration::Preset::Release, parameters));\n"
                           "    SC_TRY(project.addFiles(\"Source\", \"main.cpp\"));\n"
                           "    return definition.addProject(move(project));\n"
                           "}\n"
                           "} // namespace Build\n"
                           "} // namespace SC\n"));
        builder.finalize();
    }

    String sourceContents = StringEncoding::Utf8;
    {
        auto builder = StringBuilder::create(sourceContents);
        SC_TRY(builder.append("#include \"SaneCppMemory.h\"\n"
                              "#include \"SaneCppStrings.h\"\n"
                              "#include <stdio.h>\n"
                              "\n"
                              "int main()\n"
                              "{\n"
                              "    SC::SmallString<64> output(SC::StringEncoding::Ascii);\n"
                              "    if (not SC::StringBuilder::format(output, \"{}\", \""));
        SC_TRY(builder.append(fixtureMessage));
        SC_TRY(builder.append("\"))\n"
                              "    {\n"
                              "        return 1;\n"
                              "    }\n"
                              "    puts(output.view().bytesIncludingTerminator());\n"
                              "    return 0;\n"
                              "}\n"));
        builder.finalize();
    }

    SC_TRY(fs.writeString(buildDefinitionPath.view(), buildDefinitionContents.view()));
    SC_TRY(fs.writeString(sourcePath.view(), sourceContents.view()));
    return Result(true);
}

static Result writeSelfHostingBuildFixture(FileSystem& fs, StringView projectRoot)
{
    SC_TRY(fs.makeDirectoryRecursive(projectRoot));

    String buildDefinitionPath = StringEncoding::Utf8;
    SC_TRY(Path::join(buildDefinitionPath, {projectRoot, "SC-build.cpp"}));

    String buildDefinitionContents = StringEncoding::Utf8;
    SC_TRY(StringBuilder::format(
        buildDefinitionContents,
        "#if defined(SC_BUILD)\n"
        "#include \"SaneCppBuild.h\"\n"
        "\n"
        "namespace SC\n"
        "{{\n"
        "namespace Build\n"
        "{{\n"
        "Result configure(Definition& definition, const Parameters& parameters)\n"
        "{{\n"
        "    Project project = {{\"{}\", TargetType::ConsoleExecutable}};\n"
        "    SC_TRY(project.setRootDirectory(parameters.directories.projectDirectory.view()));\n"
        "    SC_TRY(project.addPresetConfiguration(Configuration::Preset::Debug, parameters));\n"
        "    SC_TRY(project.addPresetConfiguration(Configuration::Preset::Release, parameters));\n"
        "    SC_TRY(project.addFile(\"SC-build.cpp\"));\n"
        "    return definition.addProject(move(project));\n"
        "}}\n"
        "}} // namespace Build\n"
        "}} // namespace SC\n"
        "#else\n"
        "#include <stdio.h>\n"
        "\n"
        "int main()\n"
        "{{\n"
        "    puts(\"self-hosting-build-file\");\n"
        "    return 0;\n"
        "}}\n"
        "#endif\n",
        SelfHostingFixtureProjectName));

    SC_TRY(fs.writeString(buildDefinitionPath.view(), buildDefinitionContents.view()));
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
static constexpr StringView windowsGNUTargetTriple(Build::Architecture::Type architecture)
{
    switch (architecture)
    {
    case Build::Architecture::Intel64: return "x86_64-w64-windows-gnu";
    case Build::Architecture::Arm64: return "aarch64-w64-windows-gnu";
    case Build::Architecture::Intel32:
    case Build::Architecture::Wasm:
    case Build::Architecture::Any: return {};
    }
    Assert::unreachable();
}

static Result configureWindowsGNUAction(Build::Action& action, Build::Architecture::Type architecture)
{
    const StringView targetTriple = windowsGNUTargetTriple(architecture);
    SC_TRY_MSG(not targetTriple.isEmpty(), "Unsupported Windows GNU fixture architecture");

    action.parameters.platform                   = Build::Platform::Windows;
    action.parameters.architecture               = architecture;
    action.parameters.toolchain.family           = Build::Toolchain::LLVMMingw;
    action.parameters.targetMachine.platform     = Build::Platform::Windows;
    action.parameters.targetMachine.architecture = architecture;
    action.parameters.targetMachine.environment  = Build::TargetEnvironment::WindowsGNU;
    SC_TRY(action.parameters.toolchain.targetTriple.assign(targetTriple));
    return Result(true);
}

static Result configureWindowsMSVCAction(Build::Action& action, Build::Architecture::Type architecture)
{
    action.parameters.platform                   = Build::Platform::Windows;
    action.parameters.architecture               = architecture;
    action.parameters.toolchain.family           = Build::Toolchain::MSVC;
    action.parameters.targetMachine.platform     = Build::Platform::Windows;
    action.parameters.targetMachine.architecture = architecture;
    action.parameters.targetMachine.environment  = Build::TargetEnvironment::WindowsMSVC;
    SC_TRY(action.parameters.toolchain.targetTriple.assign({}));
    return Result(true);
}
#endif

#if SC_PLATFORM_APPLE or SC_PLATFORM_LINUX or SC_PLATFORM_WINDOWS
static Result configureLinuxTargetAction(Build::Action& action, Build::TargetEnvironment::Type environment,
                                         Build::Architecture::Type architecture, StringView sysroot = {})
{
    action.parameters.platform     = Build::Platform::Linux;
    action.parameters.architecture = architecture;
    if (action.parameters.toolchain.family != Build::Toolchain::CustomDriver)
    {
        action.parameters.toolchain.family = Build::Toolchain::Clang;
    }
    action.parameters.targetMachine.platform     = Build::Platform::Linux;
    action.parameters.targetMachine.architecture = architecture;
    action.parameters.targetMachine.environment  = environment;

    switch (environment)
    {
    case Build::TargetEnvironment::LinuxGlibc:
        switch (architecture)
        {
        case Build::Architecture::Intel64:
            SC_TRY(action.parameters.toolchain.targetTriple.assign("x86_64-unknown-linux-gnu"));
            break;
        case Build::Architecture::Arm64:
            SC_TRY(action.parameters.toolchain.targetTriple.assign("aarch64-unknown-linux-gnu"));
            break;
        case Build::Architecture::Intel32:
        case Build::Architecture::Wasm:
        case Build::Architecture::Any: return Result::Error("Unsupported Linux glibc fixture architecture");
        }
        break;
    case Build::TargetEnvironment::LinuxMusl:
        switch (architecture)
        {
        case Build::Architecture::Intel64:
            SC_TRY(action.parameters.toolchain.targetTriple.assign("x86_64-unknown-linux-musl"));
            break;
        case Build::Architecture::Arm64:
            SC_TRY(action.parameters.toolchain.targetTriple.assign("aarch64-unknown-linux-musl"));
            break;
        case Build::Architecture::Intel32:
        case Build::Architecture::Wasm:
        case Build::Architecture::Any: return Result::Error("Unsupported Linux musl fixture architecture");
        }
        break;
    case Build::TargetEnvironment::Native:
    case Build::TargetEnvironment::WindowsGNU:
    case Build::TargetEnvironment::WindowsMSVC: return Result::Error("Unsupported Linux fixture target environment");
    }

    SC_TRY(action.parameters.toolchain.sysroot.assign(sysroot));
    return Result(true);
}

#if SC_PLATFORM_APPLE
static constexpr StringView hostLLVMInstallDirectoryName() { return "llvm_macos_arm64"; }

static constexpr StringView packagedLinuxSysrootInstallDirectoryName(Build::TargetEnvironment::Type environment,
                                                                     Build::Architecture::Type      architecture)
{
    if (environment == Build::TargetEnvironment::LinuxGlibc)
    {
        switch (architecture)
        {
        case Build::Architecture::Intel64: return "linux-sysroot_glibc_x86_64";
        case Build::Architecture::Arm64: return "linux-sysroot_glibc_arm64";
        case Build::Architecture::Intel32:
        case Build::Architecture::Wasm:
        case Build::Architecture::Any: return {};
        }
    }
    if (environment == Build::TargetEnvironment::LinuxMusl)
    {
        switch (architecture)
        {
        case Build::Architecture::Intel64: return "linux-sysroot_musl_x86_64";
        case Build::Architecture::Arm64: return "linux-sysroot_musl_arm64";
        case Build::Architecture::Intel32:
        case Build::Architecture::Wasm:
        case Build::Architecture::Any: return {};
        }
    }
    return {};
}
#endif

#if SC_PLATFORM_WINDOWS
static constexpr StringView packagedLinuxSysrootCacheDirectoryName(Build::TargetEnvironment::Type environment,
                                                                   Build::Architecture::Type      architecture)
{
    if (environment == Build::TargetEnvironment::LinuxGlibc)
    {
        switch (architecture)
        {
        case Build::Architecture::Intel64: return "glibc-x86_64";
        case Build::Architecture::Arm64: return "glibc-arm64";
        case Build::Architecture::Intel32:
        case Build::Architecture::Wasm:
        case Build::Architecture::Any: return {};
        }
    }
    if (environment == Build::TargetEnvironment::LinuxMusl)
    {
        switch (architecture)
        {
        case Build::Architecture::Intel64: return "musl-x86_64";
        case Build::Architecture::Arm64: return "musl-arm64";
        case Build::Architecture::Intel32:
        case Build::Architecture::Wasm:
        case Build::Architecture::Any: return {};
        }
    }
    return {};
}

#endif
#endif

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
    Process process;
    String  rawStdout = StringEncoding::Utf8;
#if SC_PLATFORM_WINDOWS
    String normalizedExecutable = StringEncoding::Utf8;
    SC_TRY(Path::normalize(normalizedExecutable, executablePath, Path::AsNative));
    String workingDirectory = StringEncoding::Utf8;
    SC_TRY(workingDirectory.assign(Path::dirname(normalizedExecutable.view(), Path::AsNative)));
    String launchDirectory = StringEncoding::Utf8;
    SC_TRY(launchDirectory.assign(workingDirectory.view()));
    if (not workingDirectory.isEmpty())
    {
        wchar_t tempPathStorage[MAX_PATH + 1] = {};
        DWORD   tempPathLength                = ::GetTempPathW(MAX_PATH + 1, tempPathStorage);
        SC_TRY_MSG(tempPathLength != 0 and tempPathLength <= MAX_PATH, "Failed resolving Windows temp directory");

        String tempDirectory = StringEncoding::Utf8;
        SC_TRY(tempDirectory.assign(StringView::fromNullTerminated(tempPathStorage, StringEncoding::Utf16)));
        SC_TRY(Path::normalize(tempDirectory, tempDirectory.view(), Path::AsNative));

        SmallString<128> aliasName;
        SC_TRY(StringBuilder::format(aliasName, "SCBuildRun-{}", Time::Realtime::now().milliseconds));

        String aliasWorkingDirectory = StringEncoding::Utf8;
        SC_TRY(Path::join(aliasWorkingDirectory, {tempDirectory.view(), aliasName.view()}));

        FileSystem fs;
        SC_TRY(fs.init("."));
        if (fs.existsAndIsLink(aliasWorkingDirectory.view()))
        {
            SC_TRY(fs.removeEmptyDirectory(aliasWorkingDirectory.view()));
        }
        else if (fs.existsAndIsDirectory(aliasWorkingDirectory.view()))
        {
            SC_TRY(fs.removeDirectoryRecursive(aliasWorkingDirectory.view()));
        }
        SC_TRY(fs.createSymbolicLink(workingDirectory.view(), aliasWorkingDirectory.view()));
        SC_TRY(launchDirectory.assign(aliasWorkingDirectory.view()));
        SC_TRY(process.setWorkingDirectory(aliasWorkingDirectory.view()));
    }
    String launchExecutable = StringEncoding::Utf8;
    SC_TRY(Path::join(launchExecutable,
                      {launchDirectory.view(), Path::basename(normalizedExecutable.view(), Path::AsNative)}));
    StringSpan processArguments[] = {launchExecutable.view()};
    SC_TRY(process.exec({processArguments, 1}, rawStdout));
#else
    StringSpan processArguments[] = {executablePath};
    SC_TRY(process.exec({processArguments, 1}, rawStdout));
#endif
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
    (void)(executablePath);
    return Result(true);
#else
    (void)(importLibraryPath);
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

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            String expectedPackagesCacheDirectory = StringEncoding::Utf8;
            String expectedPackagesInstallDir     = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(expectedPackagesCacheDirectory,
                                       {report.libraryRootDirectory.view(), "_Build", "_PackagesCache"}));
            SC_TRUST_RESULT(
                Path::join(expectedPackagesInstallDir, {report.libraryRootDirectory.view(), "_Build", "_Packages"}));

            SC_TEST_EXPECT(StringView(buildRoot.view()).containsString("_Tests"));
            SC_TEST_EXPECT(directories.packagesCacheDirectory.view() == expectedPackagesCacheDirectory.view());
            SC_TEST_EXPECT(directories.packagesInstallDirectory.view() == expectedPackagesInstallDir.view());
        }

        if (test_section("native backend builds and runs fixture"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            Build::Action action = makeNativeCompileAction(directories, FixtureProjectName);

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram));

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

        const auto runExternalBuildFixture =
            [&](Build::Libraries librariesMode, StringView fixtureDirectoryName, StringView expectedOutput)
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String projectRoot = StringEncoding::Utf8;
            String nestedRoot  = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(projectRoot, {buildRoot.view(), fixtureDirectoryName}));
            SC_TRUST_RESULT(Path::join(nestedRoot, {projectRoot.view(), "Nested", "Child"}));
            SC_TRUST_RESULT(writeExternalBuildFixture(fs, projectRoot.view(), librariesMode));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(nestedRoot.view()));

            StringSpan commandArguments[] = {
                "--libraries-root", report.libraryRootDirectory.view(),
                "compile",          ExternalFixtureProjectName,
                "--config",         "Debug",
                "--generator",      "native",
            };

            CapturedProcessOutput capturedOutput;
            SC_TRUST_RESULT(captureExternalBuildCommand(report, nestedRoot.view(), commandArguments, capturedOutput));
            if (not recordExpectation("capturedOutput.exitStatus == 0", capturedOutput.exitStatus == 0))
            {
                if (not capturedOutput.stdOut.isEmpty())
                {
                    recordExpectation("capturedOutput.stdOut", false, capturedOutput.stdOut.view());
                }
                if (not capturedOutput.stdErr.isEmpty())
                {
                    recordExpectation("capturedOutput.stdErr", false, capturedOutput.stdErr.view());
                }
            }

            Build::Directories externalDirectories;
            SC_TRUST_RESULT(
                Path::join(externalDirectories.projectsDirectory, {projectRoot.view(), "_Build", "_Projects"}));
            SC_TRUST_RESULT(
                Path::join(externalDirectories.outputsDirectory, {projectRoot.view(), "_Build", "_Outputs"}));
            SC_TRUST_RESULT(Path::join(externalDirectories.intermediatesDirectory,
                                       {projectRoot.view(), "_Build", "_Intermediates"}));
            SC_TRUST_RESULT(
                Path::join(externalDirectories.buildCacheDirectory, {projectRoot.view(), "_Build", "_BuildCache"}));
            SC_TRUST_RESULT(Path::join(externalDirectories.packagesCacheDirectory,
                                       {projectRoot.view(), "_Build", "_PackagesCache"}));
            SC_TRUST_RESULT(
                Path::join(externalDirectories.packagesInstallDirectory, {projectRoot.view(), "_Build", "_Packages"}));
            externalDirectories.libraryDirectory = report.libraryRootDirectory.view();
            externalDirectories.projectDirectory = projectRoot.view();

            Build::Action action = makeNativeCompileAction(externalDirectories, ExternalFixtureProjectName);

            String executablePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, ExternalFixtureProjectName, executablePath));
            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));

            String stdoutOutput = StringEncoding::Utf8;
            SC_TEST_EXPECT(runBuiltProgram(executablePath.view(), stdoutOutput));
            SC_TEST_EXPECT(stdoutOutput == expectedOutput);
        };

        if (test_section("SC-build launcher builds external project with single-file libraries from nested working "
                         "directory"))
        {
            runExternalBuildFixture(Build::Libraries::SingleFile, "ebs"_a8, "external-build-single-file\n"_a8);
        }

        if (test_section("SC-build launcher builds external project with multiple library sources from nested working "
                         "directory"))
        {
            runExternalBuildFixture(Build::Libraries::Multiple, "ebm"_a8, "external-build-multiple\n"_a8);
        }

#if SC_PLATFORM_WINDOWS
        if (test_section("SC-build launcher builds external project from deep Windows path"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String projectRoot = StringEncoding::Utf8;
            String nestedRoot  = StringEncoding::Utf8;
            SC_TRUST_RESULT(appendDeepWindowsFixtureRoot(buildRoot.view(), "ebwlp"_a8, projectRoot));
            SC_TRUST_RESULT(Path::join(nestedRoot, {projectRoot.view(), "Nested", "Child"}));
            SC_TRUST_RESULT(writeExternalBuildFixture(fs, projectRoot.view(), Build::Libraries::SingleFile));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(nestedRoot.view()));

            StringSpan commandArguments[] = {
                "--libraries-root", report.libraryRootDirectory.view(),
                "compile",          ExternalFixtureProjectName,
                "--config",         "Debug",
                "--generator",      "native",
            };

            CapturedProcessOutput capturedOutput;
            SC_TRUST_RESULT(captureExternalBuildCommand(report, nestedRoot.view(), commandArguments, capturedOutput,
                                                        projectRoot.view()));
            if (not recordExpectation("capturedOutput.exitStatus == 0", capturedOutput.exitStatus == 0))
            {
                if (not capturedOutput.stdOut.isEmpty())
                {
                    recordExpectation("capturedOutput.stdOut", false, capturedOutput.stdOut.view());
                }
                if (not capturedOutput.stdErr.isEmpty())
                {
                    recordExpectation("capturedOutput.stdErr", false, capturedOutput.stdErr.view());
                }
            }

            Build::Directories externalDirectories;
            SC_TRUST_RESULT(
                Path::join(externalDirectories.projectsDirectory, {projectRoot.view(), "_Build", "_Projects"}));
            SC_TRUST_RESULT(
                Path::join(externalDirectories.outputsDirectory, {projectRoot.view(), "_Build", "_Outputs"}));
            SC_TRUST_RESULT(Path::join(externalDirectories.intermediatesDirectory,
                                       {projectRoot.view(), "_Build", "_Intermediates"}));
            SC_TRUST_RESULT(
                Path::join(externalDirectories.buildCacheDirectory, {projectRoot.view(), "_Build", "_BuildCache"}));
            SC_TRUST_RESULT(Path::join(externalDirectories.packagesCacheDirectory,
                                       {projectRoot.view(), "_Build", "_PackagesCache"}));
            SC_TRUST_RESULT(
                Path::join(externalDirectories.packagesInstallDirectory, {projectRoot.view(), "_Build", "_Packages"}));
            externalDirectories.libraryDirectory = report.libraryRootDirectory.view();
            externalDirectories.projectDirectory = projectRoot.view();

            Build::Action action = makeNativeCompileAction(externalDirectories, ExternalFixtureProjectName);

            String executablePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, ExternalFixtureProjectName, executablePath));
            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));

            String stdoutOutput = StringEncoding::Utf8;
            SC_TEST_EXPECT(runBuiltProgram(executablePath.view(), stdoutOutput));
            SC_TEST_EXPECT(stdoutOutput == "external-build-single-file\n"_a8);
        }
#endif

        if (test_section("SC-build launcher defaults to compile for external projects"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String projectRoot = StringEncoding::Utf8;
            String nestedRoot  = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(projectRoot, {buildRoot.view(), "ebd"}));
            SC_TRUST_RESULT(Path::join(nestedRoot, {projectRoot.view(), "Nested", "Child"}));
            SC_TRUST_RESULT(writeExternalBuildFixture(fs, projectRoot.view(), Build::Libraries::SingleFile));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(nestedRoot.view()));

            StringSpan commandArguments[] = {
                "--libraries-root",
                report.libraryRootDirectory.view(),
            };

            CapturedProcessOutput capturedOutput;
            SC_TRUST_RESULT(captureExternalBuildCommand(report, nestedRoot.view(), commandArguments, capturedOutput));
            if (not recordExpectation("capturedOutput.exitStatus == 0", capturedOutput.exitStatus == 0))
            {
                if (not capturedOutput.stdOut.isEmpty())
                {
                    recordExpectation("capturedOutput.stdOut", false, capturedOutput.stdOut.view());
                }
                if (not capturedOutput.stdErr.isEmpty())
                {
                    recordExpectation("capturedOutput.stdErr", false, capturedOutput.stdErr.view());
                }
            }

            Build::Directories externalDirectories;
            SC_TRUST_RESULT(
                Path::join(externalDirectories.projectsDirectory, {projectRoot.view(), "_Build", "_Projects"}));
            SC_TRUST_RESULT(
                Path::join(externalDirectories.outputsDirectory, {projectRoot.view(), "_Build", "_Outputs"}));
            SC_TRUST_RESULT(Path::join(externalDirectories.intermediatesDirectory,
                                       {projectRoot.view(), "_Build", "_Intermediates"}));
            SC_TRUST_RESULT(
                Path::join(externalDirectories.buildCacheDirectory, {projectRoot.view(), "_Build", "_BuildCache"}));
            SC_TRUST_RESULT(Path::join(externalDirectories.packagesCacheDirectory,
                                       {projectRoot.view(), "_Build", "_PackagesCache"}));
            SC_TRUST_RESULT(
                Path::join(externalDirectories.packagesInstallDirectory, {projectRoot.view(), "_Build", "_Packages"}));
            externalDirectories.libraryDirectory = report.libraryRootDirectory.view();
            externalDirectories.projectDirectory = projectRoot.view();

            Build::Action action = makeNativeCompileAction(externalDirectories, ExternalFixtureProjectName);

            String executablePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, ExternalFixtureProjectName, executablePath));
            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));
        }

        if (test_section("SC-build launcher defines SC_BUILD for self-hosting SC-build.cpp"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String projectRoot = StringEncoding::Utf8;
            String nestedRoot  = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(projectRoot, {buildRoot.view(), "shb"}));
            SC_TRUST_RESULT(Path::join(nestedRoot, {projectRoot.view(), "Nested", "Child"}));
            SC_TRUST_RESULT(writeSelfHostingBuildFixture(fs, projectRoot.view()));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(nestedRoot.view()));

            StringSpan commandArguments[] = {
                "--libraries-root", report.libraryRootDirectory.view(),
                "compile",          SelfHostingFixtureProjectName,
                "--config",         "Debug",
                "--generator",      "native",
            };

            CapturedProcessOutput capturedOutput;
            SC_TRUST_RESULT(captureExternalBuildCommand(report, nestedRoot.view(), commandArguments, capturedOutput));
            SC_TEST_EXPECT(capturedOutput.exitStatus == 0);

            Build::Directories externalDirectories;
            SC_TRUST_RESULT(
                Path::join(externalDirectories.projectsDirectory, {projectRoot.view(), "_Build", "_Projects"}));
            SC_TRUST_RESULT(
                Path::join(externalDirectories.outputsDirectory, {projectRoot.view(), "_Build", "_Outputs"}));
            SC_TRUST_RESULT(Path::join(externalDirectories.intermediatesDirectory,
                                       {projectRoot.view(), "_Build", "_Intermediates"}));
            SC_TRUST_RESULT(
                Path::join(externalDirectories.buildCacheDirectory, {projectRoot.view(), "_Build", "_BuildCache"}));
            SC_TRUST_RESULT(Path::join(externalDirectories.packagesCacheDirectory,
                                       {projectRoot.view(), "_Build", "_PackagesCache"}));
            SC_TRUST_RESULT(
                Path::join(externalDirectories.packagesInstallDirectory, {projectRoot.view(), "_Build", "_Packages"}));
            externalDirectories.libraryDirectory = report.libraryRootDirectory.view();
            externalDirectories.projectDirectory = projectRoot.view();

            Build::Action action = makeNativeCompileAction(externalDirectories, SelfHostingFixtureProjectName);

            String executablePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, SelfHostingFixtureProjectName, executablePath));
            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));

            String stdoutOutput = StringEncoding::Utf8;
            SC_TEST_EXPECT(runBuiltProgram(executablePath.view(), stdoutOutput));
            SC_TEST_EXPECT(stdoutOutput == "self-hosting-build-file\n"_a8);
        }

#if SC_PLATFORM_APPLE or SC_PLATFORM_LINUX or SC_PLATFORM_WINDOWS
        if (test_section("package install rejects unsupported Linux sysroot architecture"))
        {
            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(
                createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

            Tools::LinuxSysrootSpec spec;
            spec.environment  = Tools::LinuxSysrootSpec::Glibc;
            spec.architecture = InstructionSet::Intel32;

            Tools::Package package;
            SC_TEST_EXPECT(not Tools::installLinuxSysroot(directories.packagesCacheDirectory.view(),
                                                          directories.packagesInstallDirectory.view(), spec, package));
        }
#endif

#if SC_PLATFORM_APPLE or SC_PLATFORM_LINUX
        if (test_section("native backend cross compiles Windows x86_64 fixture with llvm-mingw"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            Build::Action action = makeNativeCompileAction(directories, FixtureProjectName);
            SC_TRUST_RESULT(configureWindowsGNUAction(action, Build::Architecture::Intel64));

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram));

            String executablePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, FixtureProjectName, executablePath));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));
            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));
        }

        if (test_section("native backend cross compiles Windows arm64 fixture with llvm-mingw"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            Build::Action action = makeNativeCompileAction(directories, FixtureProjectName);
            SC_TRUST_RESULT(configureWindowsGNUAction(action, Build::Architecture::Arm64));

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram));

            String executablePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, FixtureProjectName, executablePath));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));
            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));
        }

        if (test_section("native backend cross compiles Windows x86_64 fixture with portable MSVC import"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(
                createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String importRoot = StringEncoding::Utf8;
            String toolRoot   = StringEncoding::Utf8;
            String wineLog    = StringEncoding::Utf8;
            String winePath   = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(importRoot, {buildRoot.view(), "PortableMSVCImport"}));
            SC_TRUST_RESULT(Path::join(toolRoot, {buildRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(Path::join(wineLog, {toolRoot.view(), "portable-msvc-wine.log"}));
            SC_TRUST_RESULT(Path::join(winePath, {toolRoot.view(), "wine"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolRoot.view()));
            SC_TRUST_RESULT(createPortableMSVCImportFixture(fs, importRoot.view()));
            SC_TRUST_RESULT(writeFakeMSVCWineScript(fs, winePath.view(), wineLog.view()));

            ScopedEnvironmentVariable importVariable;
            ScopedEnvironmentVariable wineVariable;
            SC_TRUST_RESULT(
                setScopedEnvironmentVariable("SC_MSVC_IMPORT_DIRECTORY", importRoot.view(), importVariable));
            SC_TRUST_RESULT(setScopedEnvironmentVariable("SC_MSVC_WINE", winePath.view(), wineVariable));

            Build::Action action = makeNativeCompileAction(directories, FixtureProjectName);
            SC_TRUST_RESULT(configureWindowsMSVCAction(action, Build::Architecture::Intel64));

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram));

            String executablePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, FixtureProjectName, executablePath));
            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));

            String wineInvocation = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(wineLog.view(), wineInvocation));
            SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("cl.exe"));
            SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("link.exe"));
            SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("/Fo"));
            SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("/OUT:"));
        }

        if (test_section("native backend cross compiles Windows arm64 fixture with portable MSVC import"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(
                createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String importRoot = StringEncoding::Utf8;
            String toolRoot   = StringEncoding::Utf8;
            String wineLog    = StringEncoding::Utf8;
            String winePath   = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(importRoot, {buildRoot.view(), "PortableMSVCImport"}));
            SC_TRUST_RESULT(Path::join(toolRoot, {buildRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(Path::join(wineLog, {toolRoot.view(), "portable-msvc-arm64-wine.log"}));
            SC_TRUST_RESULT(Path::join(winePath, {toolRoot.view(), "wine"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolRoot.view()));
            SC_TRUST_RESULT(createPortableMSVCImportFixture(fs, importRoot.view()));
            SC_TRUST_RESULT(writeFakeMSVCWineScript(fs, winePath.view(), wineLog.view()));

            ScopedEnvironmentVariable importVariable;
            ScopedEnvironmentVariable wineVariable;
            SC_TRUST_RESULT(
                setScopedEnvironmentVariable("SC_MSVC_IMPORT_DIRECTORY", importRoot.view(), importVariable));
            SC_TRUST_RESULT(setScopedEnvironmentVariable("SC_MSVC_WINE", winePath.view(), wineVariable));

            Build::Action action = makeNativeCompileAction(directories, FixtureProjectName);
            SC_TRUST_RESULT(configureWindowsMSVCAction(action, Build::Architecture::Arm64));

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram));

            String executablePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, FixtureProjectName, executablePath));
            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));

            String wineInvocation = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(wineLog.view(), wineInvocation));
            SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("Hostx64\\arm64\\cl.exe"));
            SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("Hostx64\\arm64\\link.exe"));
        }

        if (test_section("package install imports portable MSVC for x64 and arm64"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(
                createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String importRoot = StringEncoding::Utf8;
            String toolRoot   = StringEncoding::Utf8;
            String wineLog    = StringEncoding::Utf8;
            String winePath   = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(importRoot, {buildRoot.view(), "PortableMSVCImport"}));
            SC_TRUST_RESULT(Path::join(toolRoot, {buildRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(Path::join(wineLog, {toolRoot.view(), "portable-msvc-package-wine.log"}));
            SC_TRUST_RESULT(Path::join(winePath, {toolRoot.view(), "wine"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolRoot.view()));
            SC_TRUST_RESULT(createPortableMSVCImportFixture(fs, importRoot.view()));
            SC_TRUST_RESULT(writeFakeMSVCWineScript(fs, winePath.view(), wineLog.view()));

            ScopedEnvironmentVariable importVariable;
            ScopedEnvironmentVariable wineVariable;
            SC_TRUST_RESULT(
                setScopedEnvironmentVariable("SC_MSVC_IMPORT_DIRECTORY", importRoot.view(), importVariable));
            SC_TRUST_RESULT(setScopedEnvironmentVariable("SC_MSVC_WINE", winePath.view(), wineVariable));

            Tools::Package package;
            SC_TEST_EXPECT(Tools::installMSVCToolchain(directories.packagesCacheDirectory.view(),
                                                       directories.packagesInstallDirectory.view(), package));

            String metadataPath = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(metadataPath, {package.installDirectoryLink.view(), "sc-msvc-package.json"}));
            SC_TEST_EXPECT(fs.existsAndIsFile(metadataPath.view()));

            String metadata = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(metadataPath.view(), metadata));
            SC_TEST_EXPECT(StringView(metadata.view()).containsString("\"targets\": [\"x64\", \"arm64\"]"));
            SC_TEST_EXPECT(StringView(metadata.view()).containsString(winePath.view()));

            static constexpr StringView targetArchitectures[] = {"x64", "arm64"};
            static constexpr StringView toolNames[]           = {"cl", "link", "lib"};
            for (const StringView targetArchitecture : targetArchitectures)
            {
                for (const StringView toolName : toolNames)
                {
                    String wrapperPath = StringEncoding::Utf8;
                    SC_TRUST_RESULT(Path::join(
                        wrapperPath, {package.installDirectoryLink.view(), "bin", targetArchitecture, toolName}));
                    SC_TEST_EXPECT(fs.existsAndIsFile(wrapperPath.view()));
                }
            }

            String wineInvocation = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(wineLog.view(), wineInvocation));
            SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("Hostx64\\x64\\cl.exe"));
            SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("Hostx64\\arm64\\cl.exe"));
            SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("Hostx64\\x64\\link.exe"));
            SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("Hostx64\\arm64\\link.exe"));
            SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("Ws2_32.lib"));
        }

        if (test_section("package install rejects incomplete portable MSVC import"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(
                createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String importRoot     = StringEncoding::Utf8;
            String toolRoot       = StringEncoding::Utf8;
            String wineLog        = StringEncoding::Utf8;
            String winePath       = StringEncoding::Utf8;
            String removedSDKUCRT = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(importRoot, {buildRoot.view(), "PortableMSVCImport"}));
            SC_TRUST_RESULT(Path::join(toolRoot, {buildRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(Path::join(wineLog, {toolRoot.view(), "portable-msvc-incomplete-wine.log"}));
            SC_TRUST_RESULT(Path::join(winePath, {toolRoot.view(), "wine"}));
            SC_TRUST_RESULT(Path::join(removedSDKUCRT,
                                       {importRoot.view(), "Windows Kits", "10", "Include", "10.0.26100.0", "ucrt"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolRoot.view()));
            SC_TRUST_RESULT(createPortableMSVCImportFixture(fs, importRoot.view()));
            SC_TRUST_RESULT(fs.removeDirectoriesRecursive(removedSDKUCRT.view()));
            SC_TRUST_RESULT(writeFakeMSVCWineScript(fs, winePath.view(), wineLog.view()));

            Tools::Package package;
            SC_TEST_EXPECT(not Tools::installMSVCToolchain(directories.packagesCacheDirectory.view(),
                                                           directories.packagesInstallDirectory.view(), package,
                                                           importRoot.view(), winePath.view()));
            SC_TEST_EXPECT(not fs.existsAndIsDirectory(package.installDirectoryLink.view()));
        }

        if (test_section("portable MSVC wrapper preserves slash defines"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(
                createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String importRoot = StringEncoding::Utf8;
            String toolRoot   = StringEncoding::Utf8;
            String wineLog    = StringEncoding::Utf8;
            String winePath   = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(importRoot, {buildRoot.view(), "PortableMSVCImport"}));
            SC_TRUST_RESULT(Path::join(toolRoot, {buildRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(Path::join(wineLog, {toolRoot.view(), "portable-msvc-define-wine.log"}));
            SC_TRUST_RESULT(Path::join(winePath, {toolRoot.view(), "wine"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolRoot.view()));
            SC_TRUST_RESULT(createPortableMSVCImportFixture(fs, importRoot.view()));
            SC_TRUST_RESULT(writeFakeMSVCWineScript(fs, winePath.view(), wineLog.view()));

            ScopedEnvironmentVariable importVariable;
            ScopedEnvironmentVariable wineVariable;
            SC_TRUST_RESULT(
                setScopedEnvironmentVariable("SC_MSVC_IMPORT_DIRECTORY", importRoot.view(), importVariable));
            SC_TRUST_RESULT(setScopedEnvironmentVariable("SC_MSVC_WINE", winePath.view(), wineVariable));

            Tools::Package package;
            SC_TEST_EXPECT(Tools::installMSVCToolchain(directories.packagesCacheDirectory.view(),
                                                       directories.packagesInstallDirectory.view(), package));

            String sourceFile     = StringEncoding::Utf8;
            String objectFile     = StringEncoding::Utf8;
            String objectArgument = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(sourceFile, {buildRoot.view(), "define-fixture.cpp"}));
            SC_TRUST_RESULT(Path::join(objectFile, {buildRoot.view(), "define-fixture.obj"}));
            SC_TRUST_RESULT(StringBuilder::format(objectArgument, "/Fo{}", objectFile.view()));
            SC_TRUST_RESULT(fs.writeString(sourceFile.view(), "int main() { return 0; }\n"));

            String compilerWrapper = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(compilerWrapper, {package.installDirectoryLink.view(), "bin", "x64", "cl"}));

            Process process;
            String  stdOut = StringEncoding::Utf8;
            String  stdErr = StringEncoding::Utf8;
            SC_TRUST_RESULT(process.exec({compilerWrapper.view(), "/nologo", "/DDEBUG=1", "/DSC_LIBRARY_ROOT=../../..",
                                          sourceFile.view(), objectArgument.view(), "/c"},
                                         stdOut, {}, stdErr));
            SC_TEST_EXPECT(process.getExitStatus() == 0);

            String wineInvocation = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(wineLog.view(), wineInvocation));
            SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("/DSC_LIBRARY_ROOT=../../.."));
            SC_TEST_EXPECT(not StringView(wineInvocation.view()).containsString(" Z:\\ /"));
        }

        if (test_section("package tool installs portable MSVC import from explicit arguments"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(
                createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String importRoot = StringEncoding::Utf8;
            String toolRoot   = StringEncoding::Utf8;
            String wineLog    = StringEncoding::Utf8;
            String winePath   = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(importRoot, {buildRoot.view(), "PortableMSVCImport"}));
            SC_TRUST_RESULT(Path::join(toolRoot, {buildRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(Path::join(wineLog, {toolRoot.view(), "portable-msvc-package-cli-wine.log"}));
            SC_TRUST_RESULT(Path::join(winePath, {toolRoot.view(), "wine"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolRoot.view()));
            SC_TRUST_RESULT(createPortableMSVCImportFixture(fs, importRoot.view()));
            SC_TRUST_RESULT(writeFakeMSVCWineScript(fs, winePath.view(), wineLog.view()));

            StringPath toolDestination;
            SC_TRUST_RESULT(toolDestination.assign(buildRoot.view()));

            Tools::Tool::Arguments toolArguments{*globalConsole,
                                                 report.libraryRootDirectory,
                                                 report.libraryRootDirectory,
                                                 toolDestination,
                                                 report.libraryRootDirectory,
                                                 "package",
                                                 "install",
                                                 {}};

            StringView packageArgumentsStorage[] = {"msvc", "--import-directory", importRoot.view(), "--wine",
                                                    winePath.view()};
            toolArguments.arguments              = {packageArgumentsStorage, 5};

            Tools::Package package;
            SC_TEST_EXPECT(Tools::runPackageTool(toolArguments, &package));

            String metadataPath = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(metadataPath, {package.installDirectoryLink.view(), "sc-msvc-package.json"}));
            SC_TEST_EXPECT(fs.existsAndIsFile(metadataPath.view()));

            String metadata = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(metadataPath.view(), metadata));
            SC_TEST_EXPECT(StringView(metadata.view()).containsString(winePath.view()));

            String wineInvocation = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(wineLog.view(), wineInvocation));
            SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("Hostx64\\x64\\cl.exe"));
            SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("Hostx64\\arm64\\cl.exe"));
        }

#if SC_PLATFORM_LINUX
        if (test_section("package tool installs filc import from explicit arguments"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(
                createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String importRoot     = StringEncoding::Utf8;
            String toolRoot       = StringEncoding::Utf8;
            String compilerCLog   = StringEncoding::Utf8;
            String compilerCppLog = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(importRoot, {buildRoot.view(), "FilCImport"}));
            SC_TRUST_RESULT(Path::join(toolRoot, {buildRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(Path::join(compilerCLog, {toolRoot.view(), "filc-clang.log"}));
            SC_TRUST_RESULT(Path::join(compilerCppLog, {toolRoot.view(), "filc-clangxx.log"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolRoot.view()));
            SC_TRUST_RESULT(
                createFakeFilCImportFixture(fs, importRoot.view(), compilerCLog.view(), compilerCppLog.view()));

            StringPath toolDestination;
            SC_TRUST_RESULT(toolDestination.assign(buildRoot.view()));

            Tools::Tool::Arguments toolArguments{*globalConsole,
                                                 report.libraryRootDirectory,
                                                 report.libraryRootDirectory,
                                                 toolDestination,
                                                 report.libraryRootDirectory,
                                                 "package",
                                                 "install",
                                                 {}};

            StringView packageArgumentsStorage[] = {"filc", "--import-directory", importRoot.view()};
            toolArguments.arguments              = {packageArgumentsStorage, 3};

            Tools::Package package;
            SC_TEST_EXPECT(Tools::runPackageTool(toolArguments, &package));

            String metadataPath = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(metadataPath, {package.installDirectoryLink.view(), "sc-filc-package.json"}));
            SC_TEST_EXPECT(fs.existsAndIsFile(metadataPath.view()));

            String metadata = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(metadataPath.view(), metadata));
            SC_TEST_EXPECT(StringView(metadata.view()).containsString("\"flavor\": \"pizfix\""));
            SC_TEST_EXPECT(
                StringView(metadata.view()).containsString("\"targetTriple\": \"x86_64-unknown-linux-gnu\""));

            String compilerCInvocation   = StringEncoding::Utf8;
            String compilerCppInvocation = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(compilerCLog.view(), compilerCInvocation));
            SC_TRUST_RESULT(fs.read(compilerCppLog.view(), compilerCppInvocation));
            SC_TEST_EXPECT(StringView(compilerCInvocation.view()).containsString("--version"));
            SC_TEST_EXPECT(StringView(compilerCppInvocation.view()).containsString("--version"));
        }

        if (test_section("package install rejects filc import with unsupported target"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(
                createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String importRoot     = StringEncoding::Utf8;
            String toolRoot       = StringEncoding::Utf8;
            String compilerCLog   = StringEncoding::Utf8;
            String compilerCppLog = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(importRoot, {buildRoot.view(), "FilCImport"}));
            SC_TRUST_RESULT(Path::join(toolRoot, {buildRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(Path::join(compilerCLog, {toolRoot.view(), "filc-bad-target-clang.log"}));
            SC_TRUST_RESULT(Path::join(compilerCppLog, {toolRoot.view(), "filc-bad-target-clangxx.log"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolRoot.view()));
            SC_TRUST_RESULT(createFakeFilCImportFixture(fs, importRoot.view(), compilerCLog.view(),
                                                        compilerCppLog.view(), "aarch64-unknown-linux-gnu"));

            Tools::Package package;
            SC_TEST_EXPECT(not Tools::installFilCToolchain(directories.packagesCacheDirectory.view(),
                                                           directories.packagesInstallDirectory.view(), package,
                                                           importRoot.view()));
            SC_TEST_EXPECT(not fs.existsAndIsDirectory(package.installDirectoryLink.view()));
        }

        if (test_section("package install reuses imported filc metadata without explicit import arguments"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(
                createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String importRoot     = StringEncoding::Utf8;
            String toolRoot       = StringEncoding::Utf8;
            String compilerCLog   = StringEncoding::Utf8;
            String compilerCppLog = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(importRoot, {buildRoot.view(), "FilCImport"}));
            SC_TRUST_RESULT(Path::join(toolRoot, {buildRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(Path::join(compilerCLog, {toolRoot.view(), "filc-reuse-clang.log"}));
            SC_TRUST_RESULT(Path::join(compilerCppLog, {toolRoot.view(), "filc-reuse-clangxx.log"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolRoot.view()));
            SC_TRUST_RESULT(
                createFakeFilCImportFixture(fs, importRoot.view(), compilerCLog.view(), compilerCppLog.view()));

            Tools::Package firstPackage;
            SC_TEST_EXPECT(Tools::installFilCToolchain(directories.packagesCacheDirectory.view(),
                                                       directories.packagesInstallDirectory.view(), firstPackage,
                                                       importRoot.view()));
            SC_TRUST_RESULT(fs.writeString(compilerCLog.view(), ""));
            SC_TRUST_RESULT(fs.writeString(compilerCppLog.view(), ""));

            Tools::Package reusedPackage;
            SC_TEST_EXPECT(Tools::installFilCToolchain(directories.packagesCacheDirectory.view(),
                                                       directories.packagesInstallDirectory.view(), reusedPackage));
            SC_TEST_EXPECT(firstPackage.packageLocalDirectory.view() == reusedPackage.packageLocalDirectory.view());

            String compilerCInvocation   = StringEncoding::Utf8;
            String compilerCppInvocation = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(compilerCLog.view(), compilerCInvocation));
            SC_TRUST_RESULT(fs.read(compilerCppLog.view(), compilerCppInvocation));
            SC_TEST_EXPECT(StringView(compilerCInvocation.view()).containsString("--version"));
            SC_TEST_EXPECT(StringView(compilerCppInvocation.view()).containsString("--version"));
        }

        if (test_section("native backend routes packaged filc toolchains without PATH overrides"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(
                createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String importRoot     = StringEncoding::Utf8;
            String toolRoot       = StringEncoding::Utf8;
            String compilerCLog   = StringEncoding::Utf8;
            String compilerCppLog = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(importRoot, {buildRoot.view(), "FilCImport"}));
            SC_TRUST_RESULT(Path::join(toolRoot, {buildRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(Path::join(compilerCLog, {toolRoot.view(), "filc-build-clang.log"}));
            SC_TRUST_RESULT(Path::join(compilerCppLog, {toolRoot.view(), "filc-build-clangxx.log"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolRoot.view()));
            SC_TRUST_RESULT(
                createFakeFilCImportFixture(fs, importRoot.view(), compilerCLog.view(), compilerCppLog.view()));

            Tools::Package package;
            SC_TEST_EXPECT(Tools::installFilCToolchain(directories.packagesCacheDirectory.view(),
                                                       directories.packagesInstallDirectory.view(), package,
                                                       importRoot.view()));

            Build::Action action                         = makeNativeCompileAction(directories, FixtureProjectName);
            action.parameters.platform                   = Build::Platform::Linux;
            action.parameters.architecture               = Build::Architecture::Intel64;
            action.parameters.toolchain.family           = Build::Toolchain::FilC;
            action.parameters.toolchain.architecture     = Build::Architecture::Intel64;
            action.parameters.targetMachine.platform     = Build::Platform::Linux;
            action.parameters.targetMachine.architecture = Build::Architecture::Intel64;
            action.parameters.targetMachine.environment  = Build::TargetEnvironment::Native;

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram));

            String executablePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, FixtureProjectName, executablePath));
            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));

            String stdoutOutput = StringEncoding::Utf8;
            SC_TEST_EXPECT(runBuiltProgram(executablePath.view(), stdoutOutput));

            String expectedOutput = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read("Tests/SCBuildTest/Fixture/TinyConsoleProgram/stdout.txt", expectedOutput));
            SC_TRUST_RESULT(normalizeConsoleOutput(expectedOutput));
            SC_TEST_EXPECT(stdoutOutput == expectedOutput.view());

            String compilerCppInvocation = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(compilerCppLog.view(), compilerCppInvocation));
            SC_TEST_EXPECT(StringView(compilerCppInvocation.view()).containsString("main.cpp"));
            SC_TEST_EXPECT(StringView(compilerCppInvocation.view()).containsString("--version"));
        }
#endif

        if (test_section("package install repairs legacy portable MSVC layout without SDK bin metadata"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(
                createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String packageRoot = StringEncoding::Utf8;
            String sdkBinRoot  = StringEncoding::Utf8;
            String toolRoot    = StringEncoding::Utf8;
            String wineLog     = StringEncoding::Utf8;
            String winePath    = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(packageRoot, {directories.packagesCacheDirectory.view(), "msvc",
#if SC_PLATFORM_APPLE
                                                     "portable-macos"
#elif SC_PLATFORM_LINUX
                                                     "portable-linux"
#else
                                                     "portable-host"
#endif
                                                    }));
            SC_TRUST_RESULT(Path::join(sdkBinRoot, {packageRoot.view(), "Windows Kits", "10", "bin"}));
            SC_TRUST_RESULT(Path::join(toolRoot, {buildRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(Path::join(wineLog, {toolRoot.view(), "portable-msvc-legacy-repair-wine.log"}));
            SC_TRUST_RESULT(Path::join(winePath, {toolRoot.view(), "wine"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolRoot.view()));
            SC_TRUST_RESULT(createPortableMSVCImportFixture(fs, packageRoot.view()));
            SC_TRUST_RESULT(fs.removeDirectoriesRecursive(sdkBinRoot.view()));
            SC_TRUST_RESULT(writeFakeMSVCWineScript(fs, winePath.view(), wineLog.view()));

            ScopedEnvironmentVariable wineVariable;
            SC_TRUST_RESULT(setScopedEnvironmentVariable("SC_MSVC_WINE", winePath.view(), wineVariable));

            Tools::Package package;
            SC_TEST_EXPECT(Tools::installMSVCToolchain(directories.packagesCacheDirectory.view(),
                                                       directories.packagesInstallDirectory.view(), package));
            SC_TEST_EXPECT(package.packageLocalDirectory.view() == packageRoot.view());

            String metadataPath = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(metadataPath, {package.installDirectoryLink.view(), "sc-msvc-package.json"}));
            SC_TEST_EXPECT(fs.existsAndIsFile(metadataPath.view()));

            String metadata = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(metadataPath.view(), metadata));
            SC_TEST_EXPECT(StringView(metadata.view()).containsString("\"sdkVersion\": \"10.0.26100.0\""));
            SC_TEST_EXPECT(StringView(metadata.view()).containsString(winePath.view()));

            String x64CompilerWrapper   = StringEncoding::Utf8;
            String arm64CompilerWrapper = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(x64CompilerWrapper, {package.installDirectoryLink.view(), "bin", "x64", "cl"}));
            SC_TRUST_RESULT(
                Path::join(arm64CompilerWrapper, {package.installDirectoryLink.view(), "bin", "arm64", "cl"}));
            SC_TEST_EXPECT(fs.existsAndIsFile(x64CompilerWrapper.view()));
            SC_TEST_EXPECT(fs.existsAndIsFile(arm64CompilerWrapper.view()));

            String wineInvocation = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(wineLog.view(), wineInvocation));
            SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("Hostx64\\x64\\cl.exe"));
            SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("Hostx64\\arm64\\link.exe"));
        }

#if SC_PLATFORM_LINUX
        if (test_section("native backend reuses installed portable MSVC metadata without Wine env"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(
                createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String importRoot = StringEncoding::Utf8;
            String toolRoot   = StringEncoding::Utf8;
            String wineLog    = StringEncoding::Utf8;
            String winePath   = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(importRoot, {buildRoot.view(), "PortableMSVCImport"}));
            SC_TRUST_RESULT(Path::join(toolRoot, {buildRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(Path::join(wineLog, {toolRoot.view(), "portable-msvc-installed-wine.log"}));
            SC_TRUST_RESULT(Path::join(winePath, {toolRoot.view(), "wine"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolRoot.view()));
            SC_TRUST_RESULT(createPortableMSVCImportFixture(fs, importRoot.view()));
            SC_TRUST_RESULT(writeFakeMSVCWineScript(fs, winePath.view(), wineLog.view()));

            StringPath toolDestination;
            SC_TRUST_RESULT(toolDestination.assign(buildRoot.view()));

            Tools::Tool::Arguments toolArguments{*globalConsole,
                                                 report.libraryRootDirectory,
                                                 report.libraryRootDirectory,
                                                 toolDestination,
                                                 report.libraryRootDirectory,
                                                 "package",
                                                 "install",
                                                 {}};

            StringView packageArgumentsStorage[] = {"msvc", "--import-directory", importRoot.view(), "--wine",
                                                    winePath.view()};
            toolArguments.arguments              = {packageArgumentsStorage, 5};

            Tools::Package package;
            SC_TEST_EXPECT(Tools::runPackageTool(toolArguments, &package));

            ScopedEnvironmentVariable clearedImportVariable;
            ScopedEnvironmentVariable clearedWineVariable;
            SC_TRUST_RESULT(unsetScopedEnvironmentVariable("SC_MSVC_IMPORT_DIRECTORY", clearedImportVariable));
            SC_TRUST_RESULT(unsetScopedEnvironmentVariable("SC_MSVC_WINE", clearedWineVariable));

            ScopedEnvironmentVariable pathVariable;
            SC_TRUST_RESULT(setScopedEnvironmentVariable("PATH", "/usr/bin:/bin", pathVariable));

            Build::Action action = makeNativeCompileAction(directories, FixtureProjectName);
            SC_TRUST_RESULT(configureWindowsMSVCAction(action, Build::Architecture::Intel64));

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram));

            String executablePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, FixtureProjectName, executablePath));
            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));

            String wineInvocation = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(wineLog.view(), wineInvocation));
            SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("Hostx64\\x64\\cl.exe"));
            SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("link.exe"));
        }
#endif

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
            SC_TRUST_RESULT(Path::join(toolRoot, {buildRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(Path::join(runnerLog, {toolRoot.view(), "wine.log"}));
            SC_TRUST_RESULT(Path::join(runnerConsoleLog, {toolRoot.view(), "wineconsole.log"}));
            SC_TRUST_RESULT(Path::join(runnerPath, {toolRoot.view(), "wine", "bin", "wine"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolRoot.view()));
            SC_TRUST_RESULT(
                createFakeWineBundle(fs, toolRoot.view(), runnerLog.view(), true, false, runnerConsoleLog.view()));

            Build::Action action          = makeNativeCompileAction(directories, FixtureProjectName);
            action.action                 = Build::Action::Run;
            action.parameters.runner.type = Build::RunnerSpec::Wine;
            SC_TRUST_RESULT(configureWindowsGNUAction(action, Build::Architecture::Intel64));
            SC_TRUST_RESULT(action.parameters.runner.executable.assign(runnerPath.view()));

            StringView forwardedArguments[] = {"--fixture", "runner"};
            action.additionalArguments      = forwardedArguments;

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram));

            String executablePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, FixtureProjectName, executablePath));
            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));

            String runnerInvocation = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(runnerLog.view(), runnerInvocation));
            SC_TEST_EXPECT(StringView(runnerInvocation.view()).containsString("reg add"));
            SC_TEST_EXPECT(StringView(runnerInvocation.view()).containsString("reg delete"));

            if (HostPlatform == SC::Platform::Linux and HostInstructionSet != InstructionSet::ARM64)
            {
                String runnerConsoleInvocation = StringEncoding::Utf8;
                SC_TRUST_RESULT(fs.read(runnerConsoleLog.view(), runnerConsoleInvocation));
                SC_TEST_EXPECT(StringView(runnerConsoleInvocation.view()).containsString("--backend=curses"));
                SC_TEST_EXPECT(StringView(runnerConsoleInvocation.view()).containsString("cmd /c Z:\\"));
                SC_TEST_EXPECT(StringView(runnerConsoleInvocation.view()).containsString("TinyConsoleProgram.exe"));
                SC_TEST_EXPECT(StringView(runnerConsoleInvocation.view()).containsString("--fixture runner"));
            }
            else
            {
                SC_TEST_EXPECT(StringView(runnerInvocation.view()).containsString("cmd /c Z:\\"));
                SC_TEST_EXPECT(StringView(runnerInvocation.view()).containsString("TinyConsoleProgram.exe"));
                SC_TEST_EXPECT(StringView(runnerInvocation.view()).containsString("--fixture runner"));
            }
        }

#if SC_PLATFORM_APPLE or SC_PLATFORM_LINUX
        if (test_section("package install imports qemu runner from PATH"))
        {
            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(
                createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String runnerRoot      = StringEncoding::Utf8;
            String hostToolsRoot   = StringEncoding::Utf8;
            String importedBin     = StringEncoding::Utf8;
            String qemuX86Log      = StringEncoding::Utf8;
            String qemuArm64Log    = StringEncoding::Utf8;
            String installedQEMU   = StringEncoding::Utf8;
            String packageMetadata = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(runnerRoot, {buildRoot.view(), "ImportedQEMU"}));
            SC_TRUST_RESULT(Path::join(hostToolsRoot, {buildRoot.view(), "HostTools"}));
            SC_TRUST_RESULT(Path::join(importedBin, {runnerRoot.view(), "bin"}));
            SC_TRUST_RESULT(Path::join(qemuX86Log, {hostToolsRoot.view(), "qemu-x86_64.log"}));
            SC_TRUST_RESULT(Path::join(qemuArm64Log, {hostToolsRoot.view(), "qemu-aarch64.log"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(hostToolsRoot.view()));
            SC_TRUST_RESULT(
                createFakeImportedQEMURunner(fs, runnerRoot.view(), qemuX86Log.view(), qemuArm64Log.view()));

            String newPath = StringEncoding::Utf8;
            if (const char* existingPath = ::getenv("PATH"))
            {
                SC_TRUST_RESULT(
                    StringBuilder::format(newPath, "{}:{}", importedBin.view(),
                                          StringView::fromNullTerminated(existingPath, StringEncoding::Native)));
            }
            else
            {
                SC_TRUST_RESULT(newPath.assign(importedBin.view()));
            }
            ScopedEnvironmentVariable scopedPath;
            SC_TRUST_RESULT(setScopedEnvironmentVariable("PATH", newPath.view(), scopedPath));

            Tools::Package package;
            SC_TEST_EXPECT(Tools::installQEMURunner(directories.packagesCacheDirectory.view(),
                                                    directories.packagesInstallDirectory.view(), package));
            SC_TEST_EXPECT(fs.existsAndIsDirectory(package.installDirectoryLink.view()));
            SC_TRUST_RESULT(Tools::resolveQEMURunnerExecutable(package.installDirectoryLink.view(),
                                                               InstructionSet::ARM64, installedQEMU));
            String installedQEMUFromCapability = StringEncoding::Utf8;
            SC_TEST_EXPECT(Tools::resolvePackageCapabilityPath(package.installDirectoryLink.view(),
                                                               Tools::PackageCapability::RunnerQEMUArm64,
                                                               installedQEMUFromCapability));
            SC_TEST_EXPECT(installedQEMUFromCapability.view() == installedQEMU.view());

            Process process;
            String  version = StringEncoding::Utf8;
            SC_TRUST_RESULT(process.exec({installedQEMU.view(), "--version"}, version));
            SC_TEST_EXPECT(process.getExitStatus() == 0);
            SC_TEST_EXPECT(StringView(version.view()).containsString("qemu-aarch64"));

            SC_TRUST_RESULT(fs.read(package.packageLocalTxt.view(), packageMetadata));
            SC_TEST_EXPECT(StringView(packageMetadata.view()).containsString("SC_PACKAGE_URL=import:"));
            SC_TEST_EXPECT(StringView(packageMetadata.view()).containsString("SC_PACKAGE_TARGETS=x86_64,arm64"));
        }

        if (test_section("package install rejects incomplete qemu imports"))
        {
            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(
                createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String runnerRoot   = StringEncoding::Utf8;
            String binDirectory = StringEncoding::Utf8;
            String qemuX86Path  = StringEncoding::Utf8;
            String qemuX86Log   = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(runnerRoot, {buildRoot.view(), "IncompleteQEMU"}));
            SC_TRUST_RESULT(Path::join(binDirectory, {runnerRoot.view(), "bin"}));
            SC_TRUST_RESULT(Path::join(qemuX86Path, {binDirectory.view(), "qemu-x86_64"}));
            SC_TRUST_RESULT(Path::join(qemuX86Log, {buildRoot.view(), "qemu-x86_64.log"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(binDirectory.view()));
            SC_TRUST_RESULT(
                writeFakeQEMUWrapperScript(fs, qemuX86Path.view(), qemuX86Log.view(), "qemu-x86_64 version 10.0.0"));

            Tools::Package package;
            SC_TEST_EXPECT(not Tools::installQEMURunner(directories.packagesCacheDirectory.view(),
                                                        directories.packagesInstallDirectory.view(), package,
                                                        runnerRoot.view()));
            SC_TEST_EXPECT(not fs.existsAndIsDirectory(package.installDirectoryLink.view()));
        }
#endif

#if SC_PLATFORM_LINUX
        if (test_section("native backend auto-resolves box64 Wine wrappers on Linux arm64"))
        {
            if (HostInstructionSet == InstructionSet::ARM64)
            {
                SC_TRUST_RESULT(verifyNativeBackendHostSupport());

                String             buildRoot = StringEncoding::Utf8;
                Build::Directories directories;
                SC_TRUST_RESULT(
                    createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

                FileSystem fs;
                SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

                String toolRoot        = StringEncoding::Utf8;
                String toolBin         = StringEncoding::Utf8;
                String box64Log        = StringEncoding::Utf8;
                String wine64Log       = StringEncoding::Utf8;
                String wineConsoleLog  = StringEncoding::Utf8;
                String box64Path       = StringEncoding::Utf8;
                String wine64Path      = StringEncoding::Utf8;
                String wineConsolePath = StringEncoding::Utf8;
                SC_TRUST_RESULT(Path::join(toolRoot, {buildRoot.view(), "HostTools"}));
                SC_TRUST_RESULT(Path::join(toolBin, {toolRoot.view(), "bin"}));
                SC_TRUST_RESULT(Path::join(box64Log, {toolRoot.view(), "box64.log"}));
                SC_TRUST_RESULT(Path::join(wine64Log, {toolRoot.view(), "wine64.log"}));
                SC_TRUST_RESULT(Path::join(wineConsoleLog, {toolRoot.view(), "wineconsole.log"}));
                SC_TRUST_RESULT(Path::join(box64Path, {toolBin.view(), "box64"}));
                SC_TRUST_RESULT(Path::join(wine64Path, {toolBin.view(), "wine64"}));
                SC_TRUST_RESULT(Path::join(wineConsolePath, {toolBin.view(), "wineconsole"}));
                SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolBin.view()));
                SC_TRUST_RESULT(writeBox64ForwardingWrapperScript(fs, box64Path.view(), box64Log.view()));
                SC_TRUST_RESULT(
                    writeVersionedLoggingOnlyWrapperScript(fs, wine64Path.view(), wine64Log.view(), "wine64"));
                SC_TRUST_RESULT(writeVersionedLoggingOnlyWrapperScript(fs, wineConsolePath.view(),
                                                                       wineConsoleLog.view(), "wineconsole"));

                String newPath = StringEncoding::Utf8;
                if (const char* existingPath = ::getenv("PATH"))
                {
                    SC_TRUST_RESULT(
                        StringBuilder::format(newPath, "{}:{}", toolBin.view(),
                                              StringView::fromNullTerminated(existingPath, StringEncoding::Native)));
                }
                else
                {
                    SC_TRUST_RESULT(newPath.assign(toolBin.view()));
                }
                ScopedEnvironmentVariable scopedPath;
                SC_TRUST_RESULT(setScopedEnvironmentVariable("PATH", newPath.view(), scopedPath));

                Build::Action action          = makeNativeCompileAction(directories, FixtureProjectName);
                action.action                 = Build::Action::Run;
                action.parameters.runner.type = Build::RunnerSpec::Auto;
                SC_TRUST_RESULT(configureWindowsGNUAction(action, Build::Architecture::Intel64));

                StringView forwardedArguments[] = {"--fixture", "runner"};
                action.additionalArguments      = forwardedArguments;

                SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram));

                String generatedWineWrapper = StringEncoding::Utf8;
                String generatedConsole     = StringEncoding::Utf8;
                SC_TRUST_RESULT(Path::join(generatedWineWrapper, {directories.packagesCacheDirectory.view(), "runners",
                                                                  "linux-box64-wine64", "wine"}));
                SC_TRUST_RESULT(Path::join(generatedConsole, {directories.packagesCacheDirectory.view(), "runners",
                                                              "linux-box64-wine64", "wineconsole"}));
                SC_TEST_EXPECT(fs.existsAndIsFile(generatedWineWrapper.view()));
                SC_TEST_EXPECT(fs.existsAndIsFile(generatedConsole.view()));

                String box64Invocation = StringEncoding::Utf8;
                SC_TRUST_RESULT(fs.read(box64Log.view(), box64Invocation));
                SC_TEST_EXPECT(StringView(box64Invocation.view()).containsString("wine64"));

                String wineInvocation = StringEncoding::Utf8;
                SC_TRUST_RESULT(fs.read(wine64Log.view(), wineInvocation));
                SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("cmd /c Z:\\"));
                SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("TinyConsoleProgram.exe"));
                SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("--fixture runner"));
            }
        }

        if (test_section("package install reuses packaged Linux Wine runner on arm64"))
        {
            if (HostInstructionSet == InstructionSet::ARM64)
            {
                String             buildRoot = StringEncoding::Utf8;
                Build::Directories directories;
                SC_TRUST_RESULT(
                    createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

                FileSystem fs;
                SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

                String runnerRoot      = StringEncoding::Utf8;
                String toolRoot        = StringEncoding::Utf8;
                String box64Log        = StringEncoding::Utf8;
                String wineLog         = StringEncoding::Utf8;
                String wineConsoleLog  = StringEncoding::Utf8;
                String installedRunner = StringEncoding::Utf8;
                SC_TRUST_RESULT(Path::join(
                    runnerRoot, {directories.packagesCacheDirectory.view(), "wine-stable", "linux-arm64-box64"}));
                SC_TRUST_RESULT(Path::join(toolRoot, {buildRoot.view(), "Toolchain"}));
                SC_TRUST_RESULT(Path::join(box64Log, {toolRoot.view(), "packaged-box64.log"}));
                SC_TRUST_RESULT(Path::join(wineLog, {toolRoot.view(), "packaged-wine.log"}));
                SC_TRUST_RESULT(Path::join(wineConsoleLog, {toolRoot.view(), "packaged-wineconsole.log"}));
                SC_TRUST_RESULT(Path::join(
                    installedRunner, {directories.packagesInstallDirectory.view(), "wine-stable_linux_arm64_box64"}));
                SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolRoot.view()));
                SC_TRUST_RESULT(createFakePackagedLinuxWineRunner(fs, runnerRoot.view(), box64Log.view(),
                                                                  wineLog.view(), wineConsoleLog.view()));

                Tools::Package package;
                SC_TEST_EXPECT(Tools::installLinuxWineRunner(directories.packagesCacheDirectory.view(),
                                                             directories.packagesInstallDirectory.view(), package));
                SC_TEST_EXPECT(package.installDirectoryLink == installedRunner.view());
                SC_TEST_EXPECT(fs.existsAndIsDirectory(installedRunner.view()));

                String box64Invocation = StringEncoding::Utf8;
                SC_TRUST_RESULT(fs.read(box64Log.view(), box64Invocation));
                SC_TEST_EXPECT(StringView(box64Invocation.view()).containsString("wine/opt/wine-stable/bin/wine"));
                SC_TEST_EXPECT(StringView(box64Invocation.view()).containsString("--version"));

                String wineInvocation = StringEncoding::Utf8;
                SC_TRUST_RESULT(fs.read(wineLog.view(), wineInvocation));
                SC_TEST_EXPECT(StringView(wineInvocation.view()).containsString("--version"));

                String installedWine = StringEncoding::Utf8;
                SC_TRUST_RESULT(Path::join(installedWine, {installedRunner.view(), "bin", "wine"}));
                Process process;
                String  version = StringEncoding::Utf8;
                SC_TRUST_RESULT(process.setEnvironment("PATH", "Z:\\portable-msvc\\bin;C:\\windows\\system32"));
                SC_TRUST_RESULT(process.exec({installedWine.view(), "--version"}, version));
                SC_TEST_EXPECT(process.getExitStatus() == 0);
                SC_TEST_EXPECT(StringView(version.view()).containsString("wine-11.0"));
            }
        }
#endif

        if (test_section("native backend reports missing Windows arm64 loader in bundled Wine"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String toolRoot   = StringEncoding::Utf8;
            String runnerLog  = StringEncoding::Utf8;
            String runnerPath = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(toolRoot, {buildRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(Path::join(runnerLog, {toolRoot.view(), "wine.log"}));
            SC_TRUST_RESULT(Path::join(runnerPath, {toolRoot.view(), "wine", "bin", "wine"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolRoot.view()));
            SC_TRUST_RESULT(createFakeWineBundle(fs, toolRoot.view(), runnerLog.view(), true, false));

            Build::Action action          = makeNativeCompileAction(directories, FixtureProjectName);
            action.action                 = Build::Action::Run;
            action.parameters.runner.type = Build::RunnerSpec::Wine;
            SC_TRUST_RESULT(configureWindowsGNUAction(action, Build::Architecture::Arm64));
            SC_TRUST_RESULT(action.parameters.runner.executable.assign(runnerPath.view()));

            Result runResult = Build::Action::execute(action, configureTinyConsoleProgram);
            SC_TEST_EXPECT(not runResult);

            String executablePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeExecutablePath(action, FixtureProjectName, executablePath));

            SC_TEST_EXPECT(fs.existsAndIsFile(executablePath.view()));
            SC_TEST_EXPECT(not fs.existsAndIsFile(runnerLog.view()));
        }

        if (test_section("native backend routes Windows arm64 runs through an arm64-capable Wine runner"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String toolRoot   = StringEncoding::Utf8;
            String runnerLog  = StringEncoding::Utf8;
            String runnerPath = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(toolRoot, {buildRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(Path::join(runnerLog, {toolRoot.view(), "wine.log"}));
            SC_TRUST_RESULT(Path::join(runnerPath, {toolRoot.view(), "wine", "bin", "wine"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolRoot.view()));
            SC_TRUST_RESULT(createFakeWineBundle(fs, toolRoot.view(), runnerLog.view(), true, true));

            Build::Action action          = makeNativeCompileAction(directories, FixtureProjectName);
            action.action                 = Build::Action::Run;
            action.parameters.runner.type = Build::RunnerSpec::Wine;
            SC_TRUST_RESULT(configureWindowsGNUAction(action, Build::Architecture::Arm64));
            SC_TRUST_RESULT(action.parameters.runner.executable.assign(runnerPath.view()));

            StringView forwardedArguments[] = {"--fixture", "runner"};
            action.additionalArguments      = forwardedArguments;

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram));

            String runnerInvocation = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(runnerLog.view(), runnerInvocation));
            SC_TEST_EXPECT(StringView(runnerInvocation.view()).containsString("cmd /c Z:\\"));
            SC_TEST_EXPECT(StringView(runnerInvocation.view()).containsString("TinyConsoleProgram.exe"));
            SC_TEST_EXPECT(StringView(runnerInvocation.view()).containsString("--fixture runner"));
        }
#endif

#if SC_PLATFORM_APPLE
        if (test_section("native backend smoke-starts SCTest through Wine on macOS")) {}
#endif

#if SC_PLATFORM_LINUX
        if (test_section(
                "native backend smoke-starts Windows GNU arm64 SCTest through native ARM64 Wine on Linux arm64"))
        {
            if (HostInstructionSet == InstructionSet::ARM64)
            {
                FileSystem fs;
                SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

                String repositoryIntermediates = StringEncoding::Utf8;
                String repositoryOutputs       = StringEncoding::Utf8;
                String repositoryPrefix        = StringEncoding::Utf8;
                SC_TRUST_RESULT(
                    Path::join(repositoryIntermediates, {report.libraryRootDirectory.view(), "_Build", "_Intermediates",
                                                         "SCTest", "windows-arm64-Native-llvm-mingw-Debug"}));
                SC_TRUST_RESULT(Path::join(repositoryOutputs, {report.libraryRootDirectory.view(), "_Build", "_Outputs",
                                                               "windows-arm64-Native-llvm-mingw-Debug"}));
                SC_TRUST_RESULT(Path::join(repositoryPrefix, {report.libraryRootDirectory.view(), "_Build",
                                                              "_BuildCache", "wine-prefix-arm64"}));
                if (fs.existsAndIsDirectory(repositoryIntermediates.view()))
                {
                    SC_TRUST_RESULT(fs.removeDirectoriesRecursive(repositoryIntermediates.view()));
                }
                if (fs.existsAndIsDirectory(repositoryOutputs.view()))
                {
                    SC_TRUST_RESULT(fs.removeDirectoriesRecursive(repositoryOutputs.view()));
                }
                if (fs.existsAndIsDirectory(repositoryPrefix.view()))
                {
                    SC_TRUST_RESULT(fs.removeDirectoriesRecursive(repositoryPrefix.view()));
                }

                CapturedProcessOutput capturedOutput;
                const StringSpan      arguments[] = {
                    "build",    "run",   "SCTest", "--target", "windows-gnu-arm64", "--runner",       "auto",
                    "--output", "quiet", "--",     "--test",   "BaseTest",          "--test-section", "new/delete"};
                SC_TRUST_RESULT(captureRepositoryBuildCommand(report, arguments, capturedOutput));

                SC_TEST_EXPECT(capturedOutput.exitStatus == 0);
                SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view()).containsString("RUNNER = "));
                SC_TEST_EXPECT(
                    StringView(capturedOutput.stdOut.view()).containsString("wine-stable_linux_arm64_native/bin/wine"));
                SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view()).containsString("SCTest.exe"));
                SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view())
                                   .containsString("TestReport::Running single test \"BaseTest\""));
                SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view())
                                   .containsString("TestReport::Running single section \"new/delete\""));
                SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view()).containsString("TOTAL Succeeded = 1"));
            }
        }

        if (test_section("native backend smoke-starts SCTest through portable MSVC Wine on Linux arm64"))
        {
            if (HostInstructionSet == InstructionSet::ARM64)
            {
                FileSystem fs;
                SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

                String repositoryIntermediates = StringEncoding::Utf8;
                String repositoryOutputs       = StringEncoding::Utf8;
                SC_TRUST_RESULT(
                    Path::join(repositoryIntermediates, {report.libraryRootDirectory.view(), "_Build", "_Intermediates",
                                                         "SCTest", "windows-x86_64-Native-msvc-Debug"}));
                SC_TRUST_RESULT(Path::join(repositoryOutputs, {report.libraryRootDirectory.view(), "_Build", "_Outputs",
                                                               "windows-x86_64-Native-msvc-Debug"}));
                if (fs.existsAndIsDirectory(repositoryIntermediates.view()))
                {
                    SC_TRUST_RESULT(fs.removeDirectoriesRecursive(repositoryIntermediates.view()));
                }
                if (fs.existsAndIsDirectory(repositoryOutputs.view()))
                {
                    SC_TRUST_RESULT(fs.removeDirectoriesRecursive(repositoryOutputs.view()));
                }

                CapturedProcessOutput capturedOutput;
                const StringSpan      arguments[] = {
                    "build", "run", "SCTest", "--target", "windows-msvc-x86_64", "--runner",  "auto", "--output",
                    "quiet", "--",  "--test", "BaseTest", "--test-section",      "new/delete"};
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
        }

        if (test_section("native backend smoke-starts SCTest through native ARM64 Wine on Linux arm64"))
        {
            if (HostInstructionSet == InstructionSet::ARM64)
            {
                FileSystem fs;
                SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

                String repositoryIntermediates = StringEncoding::Utf8;
                String repositoryOutputs       = StringEncoding::Utf8;
                String repositoryPrefix        = StringEncoding::Utf8;
                SC_TRUST_RESULT(
                    Path::join(repositoryIntermediates, {report.libraryRootDirectory.view(), "_Build", "_Intermediates",
                                                         "SCTest", "windows-arm64-Native-msvc-Debug"}));
                SC_TRUST_RESULT(Path::join(repositoryOutputs, {report.libraryRootDirectory.view(), "_Build", "_Outputs",
                                                               "windows-arm64-Native-msvc-Debug"}));
                SC_TRUST_RESULT(Path::join(repositoryPrefix, {report.libraryRootDirectory.view(), "_Build",
                                                              "_BuildCache", "wine-prefix-arm64"}));
                if (fs.existsAndIsDirectory(repositoryIntermediates.view()))
                {
                    SC_TRUST_RESULT(fs.removeDirectoriesRecursive(repositoryIntermediates.view()));
                }
                if (fs.existsAndIsDirectory(repositoryOutputs.view()))
                {
                    SC_TRUST_RESULT(fs.removeDirectoriesRecursive(repositoryOutputs.view()));
                }
                if (fs.existsAndIsDirectory(repositoryPrefix.view()))
                {
                    SC_TRUST_RESULT(fs.removeDirectoriesRecursive(repositoryPrefix.view()));
                }

                CapturedProcessOutput capturedOutput;
                const StringSpan      arguments[] = {
                    "build",    "run",   "SCTest", "--target", "windows-msvc-arm64", "--runner",       "auto",
                    "--output", "quiet", "--",     "--test",   "BaseTest",           "--test-section", "new/delete"};
                SC_TRUST_RESULT(captureRepositoryBuildCommand(report, arguments, capturedOutput));

                SC_TEST_EXPECT(capturedOutput.exitStatus == 0);
                SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view()).containsString("RUNNER = "));
                SC_TEST_EXPECT(
                    StringView(capturedOutput.stdOut.view()).containsString("wine-stable_linux_arm64_native/bin/wine"));
                SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view()).containsString("SCTest.exe"));
                SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view())
                                   .containsString("TestReport::Running single test \"BaseTest\""));
                SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view())
                                   .containsString("TestReport::Running single section \"new/delete\""));
                SC_TEST_EXPECT(StringView(capturedOutput.stdOut.view()).containsString("TOTAL Succeeded = 1"));
            }
        }
#endif

        if (test_section("native backend keeps small fixture small and unexported"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            Build::Action action = makeNativeCompileAction(directories, SmallFixtureProjectName, "Release");

            SC_TEST_EXPECT(Build::Action::execute(action, configureSmallSCProgram));

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

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram));

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
            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram));

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

            SC_TEST_EXPECT(Build::Action::execute(action, configureHeaderDependencyProgram));

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

            SC_TEST_EXPECT(Build::Action::execute(action, configureHeaderDependencyProgram));

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

            Result compileResult = Build::Action::execute(action, configureCompileFailureProgram);
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
            SC_TRUST_RESULT(
                captureBuildActionOutput(action, configureCompileFailureProgram, buildResult, capturedOutput));
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

            Result compileResult = Build::Action::execute(action, configureLinkFailureProgram);
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
            SC_TEST_EXPECT(Build::Action::execute(libraryAction, configureStaticLibraryProgram));

            String libraryPath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeArtifactPath(libraryAction, StaticLibraryProjectName,
                                                Build::TargetType::StaticLibrary, libraryPath));
            SC_TEST_EXPECT(fs.existsAndIsFile(libraryPath.view()));

            SC_TRUST_RESULT(setDynamicLinkedLibraryPath(libraryPath.view()));
            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(consumerRoot.view()));

            Build::Action consumerAction = makeNativeCompileAction(directories, StaticLibraryConsumerName);
            SC_TEST_EXPECT(Build::Action::execute(consumerAction, configureStaticLibraryConsumerProgram));

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
            SC_TEST_EXPECT(Build::Action::execute(action, configureWorkspaceDependencyProgram));

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

            SC_TEST_EXPECT(Build::Action::execute(action, configureIndependentWorkspacePrograms));

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
            SC_TRUST_RESULT(captureBuildActionOutput(normalAction, configureHeaderDependencyProgram, normalBuildResult,
                                                     normalOutput));
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
            SC_TRUST_RESULT(
                captureBuildActionOutput(quietAction, configureHeaderDependencyProgram, quietBuildResult, quietOutput));
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
            SC_TRUST_RESULT(
                captureBuildActionOutput(action, configureHeaderDependencyProgram, buildResult, capturedOutput));
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

            SC_TEST_EXPECT(Build::Action::execute(action, configureHeaderDependencyProgram));

            Result              buildResult = Result(true);
            CapturedBuildOutput capturedOutput;
            SC_TRUST_RESULT(
                captureBuildActionOutput(action, configureHeaderDependencyProgram, buildResult, capturedOutput));
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
            SC_TRUST_RESULT(
                captureBuildActionOutput(action, configureHeaderDependencyProgram, buildResult, capturedOutput));
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
            SC_TRUST_RESULT(
                captureBuildActionOutput(action, configureHeaderDependencyProgram, buildResult, capturedOutput));
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
            SC_TRUST_RESULT(
                captureBuildActionOutput(action, configureIndependentWorkspacePrograms, buildResult, capturedOutput));
            SC_TEST_EXPECT(not buildResult);
            const StringView quietStdOut = capturedOutput.stdOut.view();
            SC_TEST_EXPECT(quietStdOut.isEmpty() or quietStdOut.bytesWithoutTerminator()[0] != '[');
            SC_TEST_EXPECT(not StringView(quietStdOut).containsString("\n[1/"));
            SC_TEST_EXPECT(StringView(quietStdOut).containsString("FAILED:"));

            String compilerLog = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(compilerLogPath.view(), compilerLog));
            const StringView compilerLogView = compilerLog.view();
            SC_TEST_EXPECT(compilerLogView.containsString("ProgramOne/./main.cpp") or
                           compilerLogView.containsString("ProgramOne/main.cpp"));
            SC_TEST_EXPECT(not compilerLogView.containsString("ProgramTwo/./main.cpp"));
            SC_TEST_EXPECT(not compilerLogView.containsString("ProgramTwo/main.cpp"));
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

            SC_TEST_EXPECT(Build::Action::execute(action, configureWorkspaceDependencyProgram));

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

            SC_TEST_EXPECT(Build::Action::execute(action, configureCustomDriverDependencyProgram));

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

        if (test_section("native backend shapes Linux musl target profiles for custom driver toolchains"))
        {
            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String toolchainRoot   = StringEncoding::Utf8;
            String compilerLogPath = StringEncoding::Utf8;
            String compilerWrapper = StringEncoding::Utf8;
            String linkerLogPath   = StringEncoding::Utf8;
            String linkerWrapper   = StringEncoding::Utf8;
            String sysroot         = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(toolchainRoot, {buildRoot.view(), "LinuxMuslToolchain"}));
            SC_TRUST_RESULT(Path::join(compilerLogPath, {toolchainRoot.view(), "compiler.log"}));
            SC_TRUST_RESULT(Path::join(linkerLogPath, {toolchainRoot.view(), "linker.log"}));
            SC_TRUST_RESULT(Path::join(compilerWrapper, {toolchainRoot.view(), "compiler.sh"}));
            SC_TRUST_RESULT(Path::join(linkerWrapper, {toolchainRoot.view(), "linker.sh"}));
            SC_TRUST_RESULT(Path::join(sysroot, {buildRoot.view(), "sysroots", "linux-musl"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolchainRoot.view()));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(sysroot.view()));
            SC_TRUST_RESULT(fs.writeString(compilerLogPath.view(), ""));
            SC_TRUST_RESULT(fs.writeString(linkerLogPath.view(), ""));
            SC_TRUST_RESULT(writeOutputProducingWrapperScript(fs, compilerWrapper.view(), compilerLogPath.view()));
            SC_TRUST_RESULT(writeOutputProducingWrapperScript(fs, linkerWrapper.view(), linkerLogPath.view()));

            Build::Action action               = makeNativeCompileAction(directories, FixtureProjectName);
            action.parameters.toolchain.family = Build::Toolchain::CustomDriver;
            SC_TRUST_RESULT(action.parameters.toolchain.compilerC.assign(compilerWrapper.view()));
            SC_TRUST_RESULT(action.parameters.toolchain.compilerCpp.assign(compilerWrapper.view()));
            SC_TRUST_RESULT(action.parameters.toolchain.linker.assign(linkerWrapper.view()));
            SC_TRUST_RESULT(configureLinuxTargetAction(action, Build::TargetEnvironment::LinuxMusl,
                                                       Build::Architecture::Arm64, sysroot.view()));

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram));

            String compilerLog = StringEncoding::Utf8;
            String linkerLog   = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(compilerLogPath.view(), compilerLog));
            SC_TRUST_RESULT(fs.read(linkerLogPath.view(), linkerLog));
            SC_TEST_EXPECT(StringView(compilerLog.view()).containsString("-target aarch64-unknown-linux-musl"));
            SC_TEST_EXPECT(StringView(compilerLog.view()).containsString("--sysroot"));
            SC_TEST_EXPECT(StringView(compilerLog.view()).containsString(sysroot.view()));
            SC_TEST_EXPECT(StringView(linkerLog.view()).containsString("-target aarch64-unknown-linux-musl"));
            SC_TEST_EXPECT(StringView(linkerLog.view()).containsString("--sysroot"));
            SC_TEST_EXPECT(StringView(linkerLog.view()).containsString(sysroot.view()));
        }

        if (test_section("native backend shapes Linux glibc target profiles for custom driver toolchains"))
        {
            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String toolchainRoot   = StringEncoding::Utf8;
            String compilerLogPath = StringEncoding::Utf8;
            String compilerWrapper = StringEncoding::Utf8;
            String linkerLogPath   = StringEncoding::Utf8;
            String linkerWrapper   = StringEncoding::Utf8;
            String sysroot         = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(toolchainRoot, {buildRoot.view(), "LinuxGlibcToolchain"}));
            SC_TRUST_RESULT(Path::join(compilerLogPath, {toolchainRoot.view(), "compiler.log"}));
            SC_TRUST_RESULT(Path::join(linkerLogPath, {toolchainRoot.view(), "linker.log"}));
            SC_TRUST_RESULT(Path::join(compilerWrapper, {toolchainRoot.view(), "compiler.sh"}));
            SC_TRUST_RESULT(Path::join(linkerWrapper, {toolchainRoot.view(), "linker.sh"}));
            SC_TRUST_RESULT(Path::join(sysroot, {buildRoot.view(), "sysroots", "linux-glibc"}));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(toolchainRoot.view()));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(sysroot.view()));
            SC_TRUST_RESULT(fs.writeString(compilerLogPath.view(), ""));
            SC_TRUST_RESULT(fs.writeString(linkerLogPath.view(), ""));
            SC_TRUST_RESULT(writeOutputProducingWrapperScript(fs, compilerWrapper.view(), compilerLogPath.view()));
            SC_TRUST_RESULT(writeOutputProducingWrapperScript(fs, linkerWrapper.view(), linkerLogPath.view()));

            Build::Action action               = makeNativeCompileAction(directories, FixtureProjectName);
            action.parameters.toolchain.family = Build::Toolchain::CustomDriver;
            SC_TRUST_RESULT(action.parameters.toolchain.compilerC.assign(compilerWrapper.view()));
            SC_TRUST_RESULT(action.parameters.toolchain.compilerCpp.assign(compilerWrapper.view()));
            SC_TRUST_RESULT(action.parameters.toolchain.linker.assign(linkerWrapper.view()));
            SC_TRUST_RESULT(configureLinuxTargetAction(action, Build::TargetEnvironment::LinuxGlibc,
                                                       Build::Architecture::Intel64, sysroot.view()));

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram));

            String compilerLog = StringEncoding::Utf8;
            String linkerLog   = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(compilerLogPath.view(), compilerLog));
            SC_TRUST_RESULT(fs.read(linkerLogPath.view(), linkerLog));
            SC_TEST_EXPECT(StringView(compilerLog.view()).containsString("-target x86_64-unknown-linux-gnu"));
            SC_TEST_EXPECT(StringView(compilerLog.view()).containsString("--sysroot"));
            SC_TEST_EXPECT(StringView(compilerLog.view()).containsString(sysroot.view()));
            SC_TEST_EXPECT(StringView(linkerLog.view()).containsString("-target x86_64-unknown-linux-gnu"));
            SC_TEST_EXPECT(StringView(linkerLog.view()).containsString("--sysroot"));
            SC_TEST_EXPECT(StringView(linkerLog.view()).containsString(sysroot.view()));
        }
#endif

#if SC_PLATFORM_WINDOWS
        if (test_section("native backend auto-selects packaged LLVM toolchain and glibc sysroot for Linux target "
                         "profiles on Windows"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String sourceRoot = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(sourceRoot, {buildRoot.view(), "WindowsLinuxGlibcStaticLibraryFixture"}));
            SC_TRUST_RESULT(writeStaticLibraryFixture(fs, sourceRoot.view()));
            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(sourceRoot.view()));

            Build::Action action = makeNativeCompileAction(directories, StaticLibraryProjectName);
            SC_TRUST_RESULT(
                configureLinuxTargetAction(action, Build::TargetEnvironment::LinuxGlibc, Build::Architecture::Arm64));

            SC_TEST_EXPECT(Build::Action::execute(action, configureStaticLibraryProgram));

            String libraryPath = StringEncoding::Utf8;
            SC_TRUST_RESULT(
                computeArtifactPath(action, StaticLibraryProjectName, Build::TargetType::StaticLibrary, libraryPath));
            SC_TEST_EXPECT(fs.existsAndIsFile(libraryPath.view()));
        }

        if (test_section("native backend auto-selects packaged LLVM toolchain and musl sysroot for Linux target "
                         "profiles on Windows"))
        {
            SC_TRUST_RESULT(verifyNativeBackendHostSupport());

            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(createFixtureDirectories(report, buildRoot, directories));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String sourceRoot = StringEncoding::Utf8;
            SC_TRUST_RESULT(Path::join(sourceRoot, {buildRoot.view(), "WindowsLinuxMuslStaticLibraryFixture"}));
            SC_TRUST_RESULT(writeStaticLibraryFixture(fs, sourceRoot.view()));
            SC_TRUST_RESULT(setDynamicFixtureProjectRoot(sourceRoot.view()));

            Build::Action action = makeNativeCompileAction(directories, StaticLibraryProjectName);
            SC_TRUST_RESULT(
                configureLinuxTargetAction(action, Build::TargetEnvironment::LinuxMusl, Build::Architecture::Intel64));

            SC_TEST_EXPECT(Build::Action::execute(action, configureStaticLibraryProgram));

            String libraryPath = StringEncoding::Utf8;
            SC_TRUST_RESULT(
                computeArtifactPath(action, StaticLibraryProjectName, Build::TargetType::StaticLibrary, libraryPath));
            SC_TEST_EXPECT(fs.existsAndIsFile(libraryPath.view()));
        }
#endif

#if SC_PLATFORM_APPLE
        {
            static constexpr StringView qemuX86_64Names[] = {"qemu-x86_64", "qemu-x86_64-static"};
            static constexpr StringView qemuArm64Names[]  = {"qemu-aarch64", "qemu-aarch64-static"};
            String                      qemuX86_64        = StringEncoding::Utf8;
            String                      qemuArm64         = StringEncoding::Utf8;
            const bool                  hasQEMUX86_64     = tryResolveHostToolPath(
                {qemuX86_64Names, sizeof(qemuX86_64Names) / sizeof(qemuX86_64Names[0])}, qemuX86_64);
            const bool hasQEMUArm64 =
                tryResolveHostToolPath({qemuArm64Names, sizeof(qemuArm64Names) / sizeof(qemuArm64Names[0])}, qemuArm64);
            const bool requireRealQEMU = isEnvironmentFlagEnabled("SC_BUILD_REQUIRE_REAL_QEMU");
            if (test_section("native backend smoke-runs Linux target profiles through real qemu on macOS"))
            {
                if (requireRealQEMU)
                {
                    SC_TEST_EXPECT(hasQEMUX86_64);
                    SC_TEST_EXPECT(hasQEMUArm64);
                }
                if (not(hasQEMUX86_64 or hasQEMUArm64))
                {
                    SC_TEST_EXPECT(true);
                }
                else
                {
                    SC_TRUST_RESULT(verifyNativeBackendHostSupport());

                    auto smokeRunLinuxTarget = [&](Build::TargetEnvironment::Type environment,
                                                   Build::Architecture::Type      architecture,
                                                   StringView                     qemuExecutable) -> Result
                    {
                        String             buildRoot = StringEncoding::Utf8;
                        Build::Directories directories;
                        SC_TRY(createFixtureDirectories(report, buildRoot, directories));

                        Build::Action action          = makeNativeCompileAction(directories, FixtureProjectName);
                        action.action                 = Build::Action::Run;
                        action.parameters.runner.type = Build::RunnerSpec::QEMU;
                        SC_TRY(action.parameters.runner.executable.assign(qemuExecutable));
                        SC_TRY(configureLinuxTargetAction(action, environment, architecture));

                        StringView forwardedArguments[] = {"--fixture", "runner"};
                        action.additionalArguments      = {forwardedArguments,
                                                           sizeof(forwardedArguments) / sizeof(forwardedArguments[0])};
                        SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram));
                        return Result(true);
                    };

                    if (hasQEMUX86_64)
                    {
                        SC_TRUST_RESULT(smokeRunLinuxTarget(Build::TargetEnvironment::LinuxGlibc,
                                                            Build::Architecture::Intel64, qemuX86_64.view()));
                        SC_TRUST_RESULT(smokeRunLinuxTarget(Build::TargetEnvironment::LinuxMusl,
                                                            Build::Architecture::Intel64, qemuX86_64.view()));
                    }
                    if (hasQEMUArm64)
                    {
                        SC_TRUST_RESULT(smokeRunLinuxTarget(Build::TargetEnvironment::LinuxGlibc,
                                                            Build::Architecture::Arm64, qemuArm64.view()));
                        SC_TRUST_RESULT(smokeRunLinuxTarget(Build::TargetEnvironment::LinuxMusl,
                                                            Build::Architecture::Arm64, qemuArm64.view()));
                    }
                }
            }
        }

        if (test_section("native backend auto-selects packaged LLVM toolchain and glibc sysroot for Linux target "
                         "profiles on macOS"))
        {
            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(
                createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String llvmRoot     = StringEncoding::Utf8;
            String llvmBin      = StringEncoding::Utf8;
            String clangLogPath = StringEncoding::Utf8;
            String clangPath    = StringEncoding::Utf8;
            String clangCppPath = StringEncoding::Utf8;
            String llvmArPath   = StringEncoding::Utf8;
            String sysrootRoot  = StringEncoding::Utf8;
            SC_TRUST_RESULT(
                Path::join(llvmRoot, {directories.packagesInstallDirectory.view(), hostLLVMInstallDirectoryName()}));
            SC_TRUST_RESULT(Path::join(llvmBin, {llvmRoot.view(), "bin"}));
            SC_TRUST_RESULT(Path::join(clangLogPath, {llvmRoot.view(), "clang.log"}));
            SC_TRUST_RESULT(Path::join(clangPath, {llvmBin.view(), "clang"}));
            SC_TRUST_RESULT(Path::join(clangCppPath, {llvmBin.view(), "clang++"}));
            SC_TRUST_RESULT(Path::join(llvmArPath, {llvmBin.view(), "llvm-ar"}));
            SC_TRUST_RESULT(
                Path::join(sysrootRoot, {directories.packagesInstallDirectory.view(),
                                         packagedLinuxSysrootInstallDirectoryName(Build::TargetEnvironment::LinuxGlibc,
                                                                                  Build::Architecture::Arm64)}));
            auto makeSysrootPath = [&](StringView pattern, String& path) -> Result
            { return Result(StringBuilder::format(path, pattern, sysrootRoot.view())); };
            auto makeSysrootDirectory = [&](StringView pattern) -> Result
            {
                String path = StringEncoding::Utf8;
                SC_TRY(makeSysrootPath(pattern, path));
                return fs.makeDirectoryRecursive(path.view());
            };
            auto writeSysrootFile = [&](StringView pattern) -> Result
            {
                String path = StringEncoding::Utf8;
                SC_TRY(makeSysrootPath(pattern, path));
                return fs.writeString(path.view(), "");
            };
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(llvmBin.view()));
            SC_TRUST_RESULT(makeSysrootDirectory("{}/usr/aarch64-linux-gnu/include"));
            SC_TRUST_RESULT(makeSysrootDirectory("{}/usr/aarch64-linux-gnu/lib"));
            SC_TRUST_RESULT(makeSysrootDirectory("{}/usr/lib/gcc-cross/aarch64-linux-gnu/11"));
            SC_TRUST_RESULT(fs.writeString(clangLogPath.view(), ""));
            SC_TRUST_RESULT(writeVersionedOutputProducingWrapperScript(fs, clangPath.view(), clangLogPath.view(),
                                                                       "clang version 20.1.8"));
            SC_TRUST_RESULT(writeVersionedOutputProducingWrapperScript(fs, clangCppPath.view(), clangLogPath.view(),
                                                                       "clang version 20.1.8"));
            SC_TRUST_RESULT(writeVersionedOutputProducingWrapperScript(fs, llvmArPath.view(), clangLogPath.view(),
                                                                       "LLVM archive tool"));
            SC_TRUST_RESULT(writeSysrootFile("{}/usr/aarch64-linux-gnu/include/stdio.h"));
            SC_TRUST_RESULT(writeSysrootFile("{}/usr/aarch64-linux-gnu/lib/ld-linux-aarch64.so.1"));
            SC_TRUST_RESULT(writeSysrootFile("{}/usr/lib/gcc-cross/aarch64-linux-gnu/11/crtbeginS.o"));
            SC_TRUST_RESULT(writeSysrootFile("{}/usr/lib/gcc-cross/aarch64-linux-gnu/11/libgcc.a"));

            Build::Action action = makeNativeCompileAction(directories, FixtureProjectName);
            SC_TRUST_RESULT(
                configureLinuxTargetAction(action, Build::TargetEnvironment::LinuxGlibc, Build::Architecture::Arm64));

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram));

            String clangLog = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(clangLogPath.view(), clangLog));
            SC_TEST_EXPECT(StringView(clangLog.view()).containsString("-target aarch64-unknown-linux-gnu"));
            SC_TEST_EXPECT(StringView(clangLog.view()).containsString("-fuse-ld=lld"));
        }

        if (test_section("native backend auto-selects packaged musl sysroot for Linux target profiles on macOS"))
        {
            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(
                createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String llvmRoot     = StringEncoding::Utf8;
            String llvmBin      = StringEncoding::Utf8;
            String clangLogPath = StringEncoding::Utf8;
            String clangPath    = StringEncoding::Utf8;
            String clangCppPath = StringEncoding::Utf8;
            String llvmArPath   = StringEncoding::Utf8;
            String sysrootRoot  = StringEncoding::Utf8;
            SC_TRUST_RESULT(
                Path::join(llvmRoot, {directories.packagesInstallDirectory.view(), hostLLVMInstallDirectoryName()}));
            SC_TRUST_RESULT(Path::join(llvmBin, {llvmRoot.view(), "bin"}));
            SC_TRUST_RESULT(Path::join(clangLogPath, {llvmRoot.view(), "clang.log"}));
            SC_TRUST_RESULT(Path::join(clangPath, {llvmBin.view(), "clang"}));
            SC_TRUST_RESULT(Path::join(clangCppPath, {llvmBin.view(), "clang++"}));
            SC_TRUST_RESULT(Path::join(llvmArPath, {llvmBin.view(), "llvm-ar"}));
            SC_TRUST_RESULT(
                Path::join(sysrootRoot, {directories.packagesInstallDirectory.view(),
                                         packagedLinuxSysrootInstallDirectoryName(Build::TargetEnvironment::LinuxMusl,
                                                                                  Build::Architecture::Intel64)}));
            auto makeSysrootPath = [&](StringView pattern, String& path) -> Result
            { return Result(StringBuilder::format(path, pattern, sysrootRoot.view())); };
            auto makeSysrootDirectory = [&](StringView pattern) -> Result
            {
                String path = StringEncoding::Utf8;
                SC_TRY(makeSysrootPath(pattern, path));
                return fs.makeDirectoryRecursive(path.view());
            };
            auto writeSysrootFile = [&](StringView pattern) -> Result
            {
                String path = StringEncoding::Utf8;
                SC_TRY(makeSysrootPath(pattern, path));
                return fs.writeString(path.view(), "");
            };
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(llvmBin.view()));
            SC_TRUST_RESULT(makeSysrootDirectory("{}/lib"));
            SC_TRUST_RESULT(makeSysrootDirectory("{}/usr/include"));
            SC_TRUST_RESULT(makeSysrootDirectory("{}/usr/include/linux"));
            SC_TRUST_RESULT(makeSysrootDirectory("{}/usr/lib/gcc/x86_64-alpine-linux-musl/15.2.0"));
            SC_TRUST_RESULT(makeSysrootDirectory("{}/usr/lib"));
            SC_TRUST_RESULT(fs.writeString(clangLogPath.view(), ""));
            SC_TRUST_RESULT(writeVersionedOutputProducingWrapperScript(fs, clangPath.view(), clangLogPath.view(),
                                                                       "clang version 20.1.8"));
            SC_TRUST_RESULT(writeVersionedOutputProducingWrapperScript(fs, clangCppPath.view(), clangLogPath.view(),
                                                                       "clang version 20.1.8"));
            SC_TRUST_RESULT(writeVersionedOutputProducingWrapperScript(fs, llvmArPath.view(), clangLogPath.view(),
                                                                       "LLVM archive tool"));
            SC_TRUST_RESULT(writeSysrootFile("{}/lib/ld-musl-x86_64.so.1"));
            SC_TRUST_RESULT(writeSysrootFile("{}/usr/include/stdio.h"));
            SC_TRUST_RESULT(writeSysrootFile("{}/usr/include/linux/io_uring.h"));
            SC_TRUST_RESULT(writeSysrootFile("{}/usr/lib/crt1.o"));
            SC_TRUST_RESULT(writeSysrootFile("{}/usr/lib/gcc/x86_64-alpine-linux-musl/15.2.0/crtbeginS.o"));
            SC_TRUST_RESULT(writeSysrootFile("{}/usr/lib/gcc/x86_64-alpine-linux-musl/15.2.0/libgcc.a"));

            Build::Action action = makeNativeCompileAction(directories, FixtureProjectName);
            SC_TRUST_RESULT(
                configureLinuxTargetAction(action, Build::TargetEnvironment::LinuxMusl, Build::Architecture::Intel64));

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram));

            String clangLog = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(clangLogPath.view(), clangLog));
            SC_TEST_EXPECT(StringView(clangLog.view()).containsString("-target x86_64-unknown-linux-musl"));
            SC_TEST_EXPECT(StringView(clangLog.view()).containsString("-fuse-ld=lld"));
        }

        if (test_section("native backend auto-routes Linux arm64 runs through packaged qemu on macOS"))
        {
            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(
                createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String llvmRoot     = StringEncoding::Utf8;
            String llvmBin      = StringEncoding::Utf8;
            String clangLogPath = StringEncoding::Utf8;
            String clangPath    = StringEncoding::Utf8;
            String clangCppPath = StringEncoding::Utf8;
            String llvmArPath   = StringEncoding::Utf8;
            String sysrootRoot  = StringEncoding::Utf8;
            String qemuRoot     = StringEncoding::Utf8;
            String qemuX86Log   = StringEncoding::Utf8;
            String qemuArmLog   = StringEncoding::Utf8;
            SC_TRUST_RESULT(
                Path::join(llvmRoot, {directories.packagesInstallDirectory.view(), hostLLVMInstallDirectoryName()}));
            SC_TRUST_RESULT(Path::join(llvmBin, {llvmRoot.view(), "bin"}));
            SC_TRUST_RESULT(Path::join(clangLogPath, {llvmRoot.view(), "clang.log"}));
            SC_TRUST_RESULT(Path::join(clangPath, {llvmBin.view(), "clang"}));
            SC_TRUST_RESULT(Path::join(clangCppPath, {llvmBin.view(), "clang++"}));
            SC_TRUST_RESULT(Path::join(llvmArPath, {llvmBin.view(), "llvm-ar"}));
            SC_TRUST_RESULT(
                Path::join(sysrootRoot, {directories.packagesInstallDirectory.view(),
                                         packagedLinuxSysrootInstallDirectoryName(Build::TargetEnvironment::LinuxGlibc,
                                                                                  Build::Architecture::Arm64)}));
            SC_TRUST_RESULT(Path::join(qemuRoot, {buildRoot.view(), "ImportedQEMU"}));
            SC_TRUST_RESULT(Path::join(qemuX86Log, {buildRoot.view(), "qemu-x86_64.log"}));
            SC_TRUST_RESULT(Path::join(qemuArmLog, {buildRoot.view(), "qemu-aarch64.log"}));
            auto makeSysrootPath = [&](StringView pattern, String& path) -> Result
            { return Result(StringBuilder::format(path, pattern, sysrootRoot.view())); };
            auto makeSysrootDirectory = [&](StringView pattern) -> Result
            {
                String path = StringEncoding::Utf8;
                SC_TRY(makeSysrootPath(pattern, path));
                return fs.makeDirectoryRecursive(path.view());
            };
            auto writeSysrootFile = [&](StringView pattern) -> Result
            {
                String path = StringEncoding::Utf8;
                SC_TRY(makeSysrootPath(pattern, path));
                return fs.writeString(path.view(), "");
            };
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(llvmBin.view()));
            SC_TRUST_RESULT(createFakeImportedQEMURunner(fs, qemuRoot.view(), qemuX86Log.view(), qemuArmLog.view()));
            SC_TRUST_RESULT(makeSysrootDirectory("{}/usr/aarch64-linux-gnu/include"));
            SC_TRUST_RESULT(makeSysrootDirectory("{}/usr/aarch64-linux-gnu/lib"));
            SC_TRUST_RESULT(makeSysrootDirectory("{}/usr/lib/gcc-cross/aarch64-linux-gnu/11"));
            SC_TRUST_RESULT(fs.writeString(clangLogPath.view(), ""));
            SC_TRUST_RESULT(writeVersionedOutputProducingWrapperScript(fs, clangPath.view(), clangLogPath.view(),
                                                                       "clang version 20.1.8"));
            SC_TRUST_RESULT(writeVersionedOutputProducingWrapperScript(fs, clangCppPath.view(), clangLogPath.view(),
                                                                       "clang version 20.1.8"));
            SC_TRUST_RESULT(writeVersionedOutputProducingWrapperScript(fs, llvmArPath.view(), clangLogPath.view(),
                                                                       "LLVM archive tool"));
            SC_TRUST_RESULT(writeSysrootFile("{}/usr/aarch64-linux-gnu/include/stdio.h"));
            SC_TRUST_RESULT(writeSysrootFile("{}/usr/aarch64-linux-gnu/lib/ld-linux-aarch64.so.1"));
            SC_TRUST_RESULT(writeSysrootFile("{}/usr/lib/gcc-cross/aarch64-linux-gnu/11/crtbeginS.o"));
            SC_TRUST_RESULT(writeSysrootFile("{}/usr/lib/gcc-cross/aarch64-linux-gnu/11/libgcc.a"));

            Tools::Package qemuPackage;
            SC_TEST_EXPECT(Tools::installQEMURunner(directories.packagesCacheDirectory.view(),
                                                    directories.packagesInstallDirectory.view(), qemuPackage,
                                                    qemuRoot.view()));

            Build::Action action = makeNativeCompileAction(directories, FixtureProjectName);
            action.action        = Build::Action::Run;
            SC_TRUST_RESULT(configureLinuxTargetAction(action, Build::TargetEnvironment::LinuxGlibc,
                                                       Build::Architecture::Arm64, sysrootRoot.view()));
            StringView forwardedArguments[] = {"--fixture", "runner"};
            action.additionalArguments      = {forwardedArguments,
                                               sizeof(forwardedArguments) / sizeof(forwardedArguments[0])};

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram));

            String qemuInvocation = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(qemuArmLog.view(), qemuInvocation));
            SC_TEST_EXPECT(StringView(qemuInvocation.view()).containsString("-L"));
            SC_TEST_EXPECT(StringView(qemuInvocation.view()).containsString(sysrootRoot.view()));
            SC_TEST_EXPECT(StringView(qemuInvocation.view()).containsString("TinyConsoleProgram"));
            SC_TEST_EXPECT(StringView(qemuInvocation.view()).containsString("--fixture runner"));
        }

        if (test_section("native backend auto-routes Linux arm64 runs through qemu on macOS"))
        {
            String             buildRoot = StringEncoding::Utf8;
            Build::Directories directories;
            SC_TRUST_RESULT(
                createFixtureDirectories(report, buildRoot, directories, FixturePackageLayout::IsolatedRun));

            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String llvmRoot      = StringEncoding::Utf8;
            String llvmBin       = StringEncoding::Utf8;
            String clangLogPath  = StringEncoding::Utf8;
            String clangPath     = StringEncoding::Utf8;
            String clangCppPath  = StringEncoding::Utf8;
            String llvmArPath    = StringEncoding::Utf8;
            String sysrootRoot   = StringEncoding::Utf8;
            String qemuDirectory = StringEncoding::Utf8;
            String qemuLogPath   = StringEncoding::Utf8;
            String qemuPath      = StringEncoding::Utf8;
            SC_TRUST_RESULT(
                Path::join(llvmRoot, {directories.packagesInstallDirectory.view(), hostLLVMInstallDirectoryName()}));
            SC_TRUST_RESULT(Path::join(llvmBin, {llvmRoot.view(), "bin"}));
            SC_TRUST_RESULT(Path::join(clangLogPath, {llvmRoot.view(), "clang.log"}));
            SC_TRUST_RESULT(Path::join(clangPath, {llvmBin.view(), "clang"}));
            SC_TRUST_RESULT(Path::join(clangCppPath, {llvmBin.view(), "clang++"}));
            SC_TRUST_RESULT(Path::join(llvmArPath, {llvmBin.view(), "llvm-ar"}));
            SC_TRUST_RESULT(
                Path::join(sysrootRoot, {directories.packagesInstallDirectory.view(),
                                         packagedLinuxSysrootInstallDirectoryName(Build::TargetEnvironment::LinuxGlibc,
                                                                                  Build::Architecture::Arm64)}));
            SC_TRUST_RESULT(Path::join(qemuDirectory, {buildRoot.view(), "Toolchain"}));
            SC_TRUST_RESULT(Path::join(qemuLogPath, {qemuDirectory.view(), "qemu.log"}));
            SC_TRUST_RESULT(Path::join(qemuPath, {qemuDirectory.view(), "qemu-aarch64"}));
            auto makeSysrootPath = [&](StringView pattern, String& path) -> Result
            { return Result(StringBuilder::format(path, pattern, sysrootRoot.view())); };
            auto makeSysrootDirectory = [&](StringView pattern) -> Result
            {
                String path = StringEncoding::Utf8;
                SC_TRY(makeSysrootPath(pattern, path));
                return fs.makeDirectoryRecursive(path.view());
            };
            auto writeSysrootFile = [&](StringView pattern) -> Result
            {
                String path = StringEncoding::Utf8;
                SC_TRY(makeSysrootPath(pattern, path));
                return fs.writeString(path.view(), "");
            };
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(llvmBin.view()));
            SC_TRUST_RESULT(fs.makeDirectoryRecursive(qemuDirectory.view()));
            SC_TRUST_RESULT(makeSysrootDirectory("{}/usr/aarch64-linux-gnu/include"));
            SC_TRUST_RESULT(makeSysrootDirectory("{}/usr/aarch64-linux-gnu/lib"));
            SC_TRUST_RESULT(makeSysrootDirectory("{}/usr/lib/gcc-cross/aarch64-linux-gnu/11"));
            SC_TRUST_RESULT(fs.writeString(clangLogPath.view(), ""));
            SC_TRUST_RESULT(fs.writeString(qemuLogPath.view(), ""));
            SC_TRUST_RESULT(writeVersionedOutputProducingWrapperScript(fs, clangPath.view(), clangLogPath.view(),
                                                                       "clang version 20.1.8"));
            SC_TRUST_RESULT(writeVersionedOutputProducingWrapperScript(fs, clangCppPath.view(), clangLogPath.view(),
                                                                       "clang version 20.1.8"));
            SC_TRUST_RESULT(writeVersionedOutputProducingWrapperScript(fs, llvmArPath.view(), clangLogPath.view(),
                                                                       "LLVM archive tool"));
            SC_TRUST_RESULT(writeVersionedLoggingOnlyWrapperScript(fs, qemuPath.view(), qemuLogPath.view()));
            SC_TRUST_RESULT(writeSysrootFile("{}/usr/aarch64-linux-gnu/include/stdio.h"));
            SC_TRUST_RESULT(writeSysrootFile("{}/usr/aarch64-linux-gnu/lib/ld-linux-aarch64.so.1"));
            SC_TRUST_RESULT(writeSysrootFile("{}/usr/lib/gcc-cross/aarch64-linux-gnu/11/crtbeginS.o"));
            SC_TRUST_RESULT(writeSysrootFile("{}/usr/lib/gcc-cross/aarch64-linux-gnu/11/libgcc.a"));

            String      pathValue    = StringEncoding::Utf8;
            const char* existingPath = ::getenv("PATH");
            if (existingPath != nullptr)
            {
                SC_TRUST_RESULT(
                    StringBuilder::format(pathValue, "{}:{}", qemuDirectory.view(),
                                          StringView::fromNullTerminated(existingPath, StringEncoding::Native)));
            }
            else
            {
                SC_TRUST_RESULT(pathValue.assign(qemuDirectory.view()));
            }
            ScopedEnvironmentVariable scopedPath;
            SC_TRUST_RESULT(setScopedEnvironmentVariable("PATH", pathValue.view(), scopedPath));

            Build::Action action = makeNativeCompileAction(directories, FixtureProjectName);
            action.action        = Build::Action::Run;
            SC_TRUST_RESULT(configureLinuxTargetAction(action, Build::TargetEnvironment::LinuxGlibc,
                                                       Build::Architecture::Arm64, sysrootRoot.view()));
            StringView forwardedArguments[] = {"--fixture", "runner"};
            action.additionalArguments      = {forwardedArguments,
                                               sizeof(forwardedArguments) / sizeof(forwardedArguments[0])};

            SC_TEST_EXPECT(Build::Action::execute(action, configureTinyConsoleProgram));

            String qemuInvocation = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read(qemuLogPath.view(), qemuInvocation));
            SC_TEST_EXPECT(StringView(qemuInvocation.view()).containsString("-L"));
            SC_TEST_EXPECT(StringView(qemuInvocation.view()).containsString(sysrootRoot.view()));
            SC_TEST_EXPECT(StringView(qemuInvocation.view()).containsString("TinyConsoleProgram"));
            SC_TEST_EXPECT(StringView(qemuInvocation.view()).containsString("--fixture runner"));
        }
#endif
    }
};

void runSCBuildTest(SC::TestReport& report) { SCBuildFixtureTest test(report); }
} // namespace SC
