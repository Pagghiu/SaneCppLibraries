// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Tools/Tools.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Strings/StringFormat.h"
#include "Libraries/Testing/Testing.h"
#include "Tools/SC-build.h"
#include "Tools/SC-build/BuildCLI.h"
#include "Tools/SC-package.h"

extern SC::Console* globalConsole;
namespace SC
{
static bool writeBuildHelpAddendumToString(Build::Action::Type actionType, String& text)
{
    GrowableBuffer<String> growable(text);
    StringFormatOutput     output(StringEncoding::Utf8, growable);
    const bool             result = Tools::detail::appendBuildActionHelpAddendum(output, actionType);
    growable.finalize();
    return result;
}

static bool shouldRunHeavySupportToolsTests()
{
    ProcessEnvironment environment;
    StringSpan         value;
    return environment.get("SC_RUN_HEAVY_SUPPORT_TOOLS_TESTS", value) and not value.isEmpty() and value != "0";
}

static Result installFakeRegistryPackage(StringView, StringView packagesInstallDirectory, Tools::Package& package,
                                         Span<const StringView>)
{
    FileSystem fs;
    SC_TRY(fs.init("."));

    String packageRoot = StringEncoding::Utf8;
    String binRoot     = StringEncoding::Utf8;
    String toolPath    = StringEncoding::Utf8;
    SC_TRY(Path::join(packageRoot, {packagesInstallDirectory, "external-fake"}));
    SC_TRY(Path::join(binRoot, {packageRoot.view(), "bin"}));
    SC_TRY(Path::join(toolPath, {binRoot.view(), "fake-tool"}));
    if (fs.existsAndIsDirectory(packageRoot.view()))
    {
        SC_TRY(fs.removeDirectoriesRecursive(packageRoot.view()));
    }
    SC_TRY(fs.makeDirectoryRecursive(binRoot.view()));
    SC_TRY(fs.writeString(toolPath.view(), "fake"));

    package.installDirectoryLink = packageRoot.view();
    Tools::PackageReceiptInfo info;
    info.packageName                            = "external-fake";
    info.packageVersion                         = "1";
    info.recipeVersion                          = "1";
    info.hostPlatform                           = "test";
    info.packageVariant                         = "host";
    info.source                                 = "test";
    info.sourceHash                             = "";
    info.validation                             = "passed";
    const StringView phases[]                   = {"installFakeRegistryPackage", "writeReceipt"};
    info.phases                                 = phases;
    const Tools::PackageReceiptExport exports[] = {
        {Tools::PackageExportKind::Tool, "fake-tool", "bin/fake-tool"},
    };
    return Tools::writePackageReceipt(package, info, exports);
}

#if !SC_PLATFORM_WINDOWS
static Result writeFakeQEMURunner(FileSystem& fs, StringView path, StringView name)
{
    String script = StringEncoding::Utf8;
    auto   builder = StringBuilder::create(script);
    SC_TRY(builder.append("#!/bin/sh\n"));
    SC_TRY(builder.append("echo \"{} version 10.0.0\"\n", name));
    builder.finalize();
    SC_TRY(fs.writeString(path, script.view()));
    return fs.chmod(path, 0755u);
}

static Result qemuRepairPackageRoot(StringView packagesRoot, String& packageRoot)
{
    StringView leaf;
    switch (HostPlatform)
    {
    case Platform::Apple:
        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64: leaf = "macos_arm64"; break;
        case InstructionSet::Intel64: leaf = "macos_intel64"; break;
        case InstructionSet::Intel32: return Result::Error("Unsupported QEMU test host");
        }
        break;
    case Platform::Linux:
        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64: leaf = "linux_arm64"; break;
        case InstructionSet::Intel64: leaf = "linux_intel64"; break;
        case InstructionSet::Intel32: return Result::Error("Unsupported QEMU test host");
        }
        break;
    case Platform::Windows:
    case Platform::Emscripten: return Result::Error("Unsupported QEMU test host");
    }
    SC_TRY(StringBuilder::format(packageRoot, "{}/qemu_{}", packagesRoot, leaf));
    return Result(true);
}
#endif

struct SupportToolsTest : public TestCase
{
    SupportToolsTest(SC::TestReport& report) : TestCase(report, "SupportToolsTest")
    {
        using namespace SC::Tools;
        using namespace SC::Tools::detail;
        StringPath outputDirectory;
        (void)StringBuilder::format(outputDirectory, "{0}/_Build/_TestScratch/SupportToolsTest",
                                    report.libraryRootDirectory);
        {
            FileSystem fs;
            SC_TEST_EXPECT(fs.init("."));
            if (fs.existsAndIsDirectory(outputDirectory.view()))
            {
                SC_TEST_EXPECT(fs.removeDirectoriesRecursive(outputDirectory.view()));
            }
            SC_TEST_EXPECT(fs.makeDirectoryRecursive(outputDirectory.view()));
        }
        Tool::Arguments arguments{*globalConsole,
                                  report.libraryRootDirectory,
                                  report.libraryRootDirectory,
                                  outputDirectory,
                                  report.libraryRootDirectory,
                                  StringView(),
                                  StringView(),
                                  {}};

        const bool runHeavySections = shouldRunHeavySupportToolsTests();
        StringView args[10];
        if (test_section("build cli parses legacy positional arguments"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "Release";
            args[2]             = "native";
            args[3]             = "arm64";
            args[4]             = "quiet";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Error;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.projectName == "SCTest");
            SC_TEST_EXPECT(action.configurationName == "Release");
            SC_TEST_EXPECT(action.parameters.generator == Build::Generator::Native);
            SC_TEST_EXPECT(action.parameters.architecture == Build::Architecture::Arm64);
            SC_TEST_EXPECT(action.parameters.execution.outputMode.hasBeenSet());
            SC_TEST_EXPECT(action.parameters.execution.outputMode == Build::OutputMode::Quiet);
        }
        if (test_section("build cli parses named options and config aliases"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "-c";
            args[2]             = "d";
            args[3]             = "-g";
            args[4]             = "n";
            args[5]             = "--arch";
            args[6]             = "arm";
            args[7]             = "--quiet";
            arguments.arguments = {args, 8};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Error;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.projectName == "SCTest");
            SC_TEST_EXPECT(action.configurationName == "Debug");
            SC_TEST_EXPECT(action.parameters.generator == Build::Generator::Native);
            SC_TEST_EXPECT(action.parameters.architecture == Build::Architecture::Arm64);
            SC_TEST_EXPECT(action.parameters.execution.outputMode == Build::OutputMode::Quiet);
        }
        if (test_section("build cli defaults omitted generator to native"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            arguments.arguments = {args, 1};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Error;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.parameters.generator == Build::Generator::Native);
        }
        if (test_section("build cli maps generator default to native"))
        {
            arguments.tool      = "build";
            arguments.action    = "run";
            args[0]             = "SCTest";
            args[1]             = "--generator";
            args[2]             = "default";
            arguments.arguments = {args, 3};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Error;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Run, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.parameters.generator == Build::Generator::Native);
        }
        if (test_section("build cli parses filc toolchain selection"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "--toolchain";
            args[2]             = "filc";
            args[3]             = "--arch";
            args[4]             = "intel64";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Error;
#if SC_PLATFORM_LINUX
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.parameters.toolchain.family == Build::Toolchain::FilC);
            SC_TEST_EXPECT(action.parameters.architecture == Build::Architecture::Intel64);
            SC_TEST_EXPECT(action.parameters.toolchain.architecture == Build::Architecture::Intel64);
#else
            SC_TEST_EXPECT(not prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
#endif
        }
        if (test_section("build cli parses Windows GNU target profiles"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "--generator";
            args[2]             = "native";
            args[3]             = "--target";
            args[4]             = "windows-gnu-x86_64";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Error;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.parameters.generator == Build::Generator::Native);
            SC_TEST_EXPECT(action.parameters.platform == Build::Platform::Windows);
            SC_TEST_EXPECT(action.parameters.architecture == Build::Architecture::Intel64);
            SC_TEST_EXPECT(action.parameters.toolchain.family == Build::Toolchain::LLVMMingw);
            SC_TEST_EXPECT(action.parameters.targetMachine.platform == Build::Platform::Windows);
            SC_TEST_EXPECT(action.parameters.targetMachine.architecture == Build::Architecture::Intel64);
            SC_TEST_EXPECT(action.parameters.targetMachine.environment == Build::TargetEnvironment::WindowsGNU);
            SC_TEST_EXPECT(action.parameters.toolchain.targetTriple == "x86_64-w64-windows-gnu");
        }
        if (test_section("build cli parses Windows GNU arm64 target profiles"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "--generator";
            args[2]             = "native";
            args[3]             = "--target";
            args[4]             = "windows-gnu-arm64";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Error;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.parameters.generator == Build::Generator::Native);
            SC_TEST_EXPECT(action.parameters.platform == Build::Platform::Windows);
            SC_TEST_EXPECT(action.parameters.architecture == Build::Architecture::Arm64);
            SC_TEST_EXPECT(action.parameters.toolchain.family == Build::Toolchain::LLVMMingw);
            SC_TEST_EXPECT(action.parameters.targetMachine.platform == Build::Platform::Windows);
            SC_TEST_EXPECT(action.parameters.targetMachine.architecture == Build::Architecture::Arm64);
            SC_TEST_EXPECT(action.parameters.targetMachine.environment == Build::TargetEnvironment::WindowsGNU);
            SC_TEST_EXPECT(action.parameters.toolchain.targetTriple == "aarch64-w64-windows-gnu");
        }
        if (test_section("build cli parses Linux glibc target profiles"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "--generator";
            args[2]             = "native";
            args[3]             = "--target";
            args[4]             = "linux-glibc-x86_64";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Error;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.parameters.generator == Build::Generator::Native);
            SC_TEST_EXPECT(action.parameters.platform == Build::Platform::Linux);
            SC_TEST_EXPECT(action.parameters.architecture == Build::Architecture::Intel64);
            SC_TEST_EXPECT(action.parameters.toolchain.family == Build::Toolchain::Clang);
            SC_TEST_EXPECT(action.parameters.targetMachine.platform == Build::Platform::Linux);
            SC_TEST_EXPECT(action.parameters.targetMachine.architecture == Build::Architecture::Intel64);
            SC_TEST_EXPECT(action.parameters.targetMachine.environment == Build::TargetEnvironment::LinuxGlibc);
            SC_TEST_EXPECT(action.parameters.toolchain.targetTriple == "x86_64-unknown-linux-gnu");
        }
        if (test_section("build cli parses Linux musl arm64 target profiles"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "--generator";
            args[2]             = "native";
            args[3]             = "--target";
            args[4]             = "linux-musl-arm64";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Error;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.parameters.generator == Build::Generator::Native);
            SC_TEST_EXPECT(action.parameters.platform == Build::Platform::Linux);
            SC_TEST_EXPECT(action.parameters.architecture == Build::Architecture::Arm64);
            SC_TEST_EXPECT(action.parameters.toolchain.family == Build::Toolchain::Clang);
            SC_TEST_EXPECT(action.parameters.targetMachine.platform == Build::Platform::Linux);
            SC_TEST_EXPECT(action.parameters.targetMachine.architecture == Build::Architecture::Arm64);
            SC_TEST_EXPECT(action.parameters.targetMachine.environment == Build::TargetEnvironment::LinuxMusl);
            SC_TEST_EXPECT(action.parameters.toolchain.targetTriple == "aarch64-unknown-linux-musl");
        }
        if (test_section("build cli parses Windows MSVC target profiles"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "--generator";
            args[2]             = "native";
            args[3]             = "--target";
            args[4]             = "windows-msvc-x86_64";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Error;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.parameters.generator == Build::Generator::Native);
            SC_TEST_EXPECT(action.parameters.platform == Build::Platform::Windows);
            SC_TEST_EXPECT(action.parameters.architecture == Build::Architecture::Intel64);
            SC_TEST_EXPECT(action.parameters.toolchain.family == Build::Toolchain::MSVC);
            SC_TEST_EXPECT(action.parameters.targetMachine.platform == Build::Platform::Windows);
            SC_TEST_EXPECT(action.parameters.targetMachine.architecture == Build::Architecture::Intel64);
            SC_TEST_EXPECT(action.parameters.targetMachine.environment == Build::TargetEnvironment::WindowsMSVC);
        }
        if (test_section("build cli parses Windows MSVC arm64 target profiles"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "--generator";
            args[2]             = "native";
            args[3]             = "--target";
            args[4]             = "windows-msvc-arm64";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Error;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.parameters.generator == Build::Generator::Native);
            SC_TEST_EXPECT(action.parameters.platform == Build::Platform::Windows);
            SC_TEST_EXPECT(action.parameters.architecture == Build::Architecture::Arm64);
            SC_TEST_EXPECT(action.parameters.toolchain.family == Build::Toolchain::MSVC);
            SC_TEST_EXPECT(action.parameters.targetMachine.platform == Build::Platform::Windows);
            SC_TEST_EXPECT(action.parameters.targetMachine.architecture == Build::Architecture::Arm64);
            SC_TEST_EXPECT(action.parameters.targetMachine.environment == Build::TargetEnvironment::WindowsMSVC);
        }
        if (test_section("build run parses runner selection"))
        {
            arguments.tool      = "build";
            arguments.action    = "run";
            args[0]             = "SCTest";
            args[1]             = "--target";
            args[2]             = "windows-gnu-x86_64";
            args[3]             = "--runner";
            args[4]             = "wine";
            args[5]             = "--runner-path";
            args[6]             = "/tmp/fake-wine";
            arguments.arguments = {args, 7};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Error;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Run, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.parameters.runner.type == Build::RunnerSpec::Wine);
            SC_TEST_EXPECT(action.parameters.runner.executable == "/tmp/fake-wine");
            SC_TEST_EXPECT(action.parameters.targetMachine.platform == Build::Platform::Windows);
        }
        if (test_section("build run parses qemu runner selection"))
        {
            arguments.tool      = "build";
            arguments.action    = "run";
            args[0]             = "SCTest";
            args[1]             = "--target";
            args[2]             = "linux-glibc-arm64";
            args[3]             = "--runner";
            args[4]             = "qemu";
            args[5]             = "--runner-path";
            args[6]             = "/tmp/fake-qemu-aarch64";
            arguments.arguments = {args, 7};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Error;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Run, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.parameters.runner.type == Build::RunnerSpec::QEMU);
            SC_TEST_EXPECT(action.parameters.runner.executable == "/tmp/fake-qemu-aarch64");
            SC_TEST_EXPECT(action.parameters.targetMachine.platform == Build::Platform::Linux);
            SC_TEST_EXPECT(action.parameters.targetMachine.environment == Build::TargetEnvironment::LinuxGlibc);
        }
        if (test_section("build cli parses raw target triple and sysroot overrides"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "--triple";
            args[2]             = "aarch64-linux-musl";
            args[3]             = "--sysroot";
            args[4]             = "/opt/sysroots/linux-musl";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Error;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.parameters.toolchain.targetTriple == "aarch64-linux-musl");
            SC_TEST_EXPECT(action.parameters.toolchain.sysroot == "/opt/sysroots/linux-musl");
        }
        if (test_section("build cli raw triple override wins over target profile"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "--target";
            args[2]             = "windows-gnu-x86_64";
            args[3]             = "--triple";
            args[4]             = "x86_64-custom-windows-gnu";
            args[5]             = "--sysroot";
            args[6]             = "/tmp/custom-windows-sysroot";
            arguments.arguments = {args, 7};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Error;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.parameters.toolchain.family == Build::Toolchain::LLVMMingw);
            SC_TEST_EXPECT(action.parameters.targetMachine.platform == Build::Platform::Windows);
            SC_TEST_EXPECT(action.parameters.targetMachine.environment == Build::TargetEnvironment::WindowsGNU);
            SC_TEST_EXPECT(action.parameters.toolchain.targetTriple == "x86_64-custom-windows-gnu");
            SC_TEST_EXPECT(action.parameters.toolchain.sysroot == "/tmp/custom-windows-sysroot");
        }
        if (test_section("build cli rejects incompatible generator for Windows GNU target profiles"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "--generator";
            args[2]             = "make";
            args[3]             = "--target";
            args[4]             = "windows-gnu-x86_64";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Ready;
            SC_TEST_EXPECT(not prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
        }
        if (test_section("build cli rejects incompatible arch for Windows GNU target profiles"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "--target";
            args[2]             = "windows-gnu-arm64";
            args[3]             = "--arch";
            args[4]             = "intel64";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Ready;
            SC_TEST_EXPECT(not prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
        }
        if (test_section("build cli rejects sysroot overrides for Windows MSVC target profiles"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "--target";
            args[2]             = "windows-msvc-x86_64";
            args[3]             = "--sysroot";
            args[4]             = "/tmp/msvc";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Ready;
            SC_TEST_EXPECT(not prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
        }
        if (test_section("build cli rejects contradictory triples for Windows GNU target profiles"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "--target";
            args[2]             = "windows-gnu-x86_64";
            args[3]             = "--triple";
            args[4]             = "aarch64-linux-musl";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Ready;
            SC_TEST_EXPECT(not prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
        }
        if (test_section("build cli rejects contradictory triples for Linux musl target profiles"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "--target";
            args[2]             = "linux-musl-arm64";
            args[3]             = "--triple";
            args[4]             = "aarch64-unknown-linux-gnu";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Ready;
            SC_TEST_EXPECT(not prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
        }
        if (test_section("build cli rejects reserved abi selector"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "--target";
            args[2]             = "linux-glibc-x86_64";
            args[3]             = "--abi";
            args[4]             = "gnu";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Ready;
            SC_TEST_EXPECT(not prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
        }
        if (test_section("build target profile selects native backend automatically"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "--target";
            args[2]             = "windows-gnu-x86_64";
            arguments.arguments = {args, 3};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Error;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.parameters.generator == Build::Generator::Native);
            SC_TEST_EXPECT(action.parameters.toolchain.family == Build::Toolchain::LLVMMingw);
        }
        if (test_section("build cli parses utf16 legacy generator values"))
        {
            arguments.tool               = "build";
            arguments.action             = "compile";
            args[0]                      = "SCTest";
            args[1]                      = "Debug";
            const uint16_t vs2022Utf16[] = {'v', 's', '2', '0', '2', '2'};
            args[2]             = StringView({reinterpret_cast<const char*>(vs2022Utf16), sizeof(vs2022Utf16)}, false,
                                             StringEncoding::Utf16);
            arguments.arguments = {args, 3};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Error;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.parameters.generator == Build::Generator::VisualStudio2022);
        }
        if (test_section("build cli named options override legacy positional values"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "Release";
            args[2]             = "make";
            args[3]             = "arm64";
            args[4]             = "quiet";
            args[5]             = "--config";
            args[6]             = "Debug";
            args[7]             = "--generator";
            args[8]             = "native";
            args[9]             = "-v";
            arguments.arguments = {args, 10};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Error;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.configurationName == "Debug");
            SC_TEST_EXPECT(action.parameters.generator == Build::Generator::Native);
            SC_TEST_EXPECT(action.parameters.architecture == Build::Architecture::Arm64);
            SC_TEST_EXPECT(action.parameters.execution.outputMode == Build::OutputMode::Verbose);
        }
        if (test_section("build cli keeps run forwarding after terminator"))
        {
            arguments.tool      = "build";
            arguments.action    = "run";
            args[0]             = "SCTest";
            args[1]             = "--config";
            args[2]             = "Release";
            args[3]             = "--";
            args[4]             = "--test";
            args[5]             = "BuildTest";
            arguments.arguments = {args, 6};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Error;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Run, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.configurationName == "Release");
            SC_TEST_EXPECT(action.additionalArguments.sizeInElements() == 2);
            SC_TEST_EXPECT(action.additionalArguments[0] == "--test");
            SC_TEST_EXPECT(action.additionalArguments[1] == "BuildTest");
        }
        if (test_section("build run rejects Wine for native targets"))
        {
            arguments.tool      = "build";
            arguments.action    = "run";
            args[0]             = "SCTest";
            args[1]             = "--runner";
            args[2]             = "wine";
            arguments.arguments = {args, 3};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Ready;
            SC_TEST_EXPECT(not prepareBuildAction(Build::Action::Run, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
        }
#if SC_PLATFORM_LINUX
        if (test_section("build run accepts direct Linux native architecture translation"))
        {
            arguments.tool      = "build";
            arguments.action    = "run";
            args[0]             = "SCTest";
            args[1]             = "--arch";
            args[2]             = "intel64";
            args[3]             = "--runner";
            args[4]             = "none";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Ready;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Run, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.parameters.targetMachine.platform == Build::Platform::Linux);
            SC_TEST_EXPECT(action.parameters.targetMachine.environment == Build::TargetEnvironment::Native);
            SC_TEST_EXPECT(action.parameters.targetMachine.architecture == Build::Architecture::Intel64);
        }
#endif
        if (test_section("build run accepts auto-run for Windows GNU arm64"))
        {
            arguments.tool      = "build";
            arguments.action    = "run";
            args[0]             = "SCTest";
            args[1]             = "--target";
            args[2]             = "windows-gnu-arm64";
            arguments.arguments = {args, 3};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Ready;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Run, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
        }
        if (test_section("build run accepts qemu for Linux arm64 targets"))
        {
            arguments.tool      = "build";
            arguments.action    = "run";
            args[0]             = "SCTest";
            args[1]             = "--target";
            args[2]             = "linux-musl-arm64";
            args[3]             = "--runner";
            args[4]             = "qemu";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Ready;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Run, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
        }
        if (test_section("build run rejects qemu for Windows targets"))
        {
            arguments.tool      = "build";
            arguments.action    = "run";
            args[0]             = "SCTest";
            args[1]             = "--target";
            args[2]             = "windows-gnu-x86_64";
            args[3]             = "--runner";
            args[4]             = "qemu";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Ready;
            SC_TEST_EXPECT(not prepareBuildAction(Build::Action::Run, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
        }
        if (test_section("build run rejects runner none for foreign targets"))
        {
            arguments.tool      = "build";
            arguments.action    = "run";
            args[0]             = "SCTest";
            args[1]             = "--target";
            args[2]             = "windows-gnu-x86_64";
            args[3]             = "--runner";
            args[4]             = "none";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Ready;
            SC_TEST_EXPECT(not prepareBuildAction(Build::Action::Run, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
        }
        if (test_section("build run rejects Wine for Linux targets"))
        {
            arguments.tool      = "build";
            arguments.action    = "run";
            args[0]             = "SCTest";
            args[1]             = "--target";
            args[2]             = "linux-glibc-x86_64";
            args[3]             = "--runner";
            args[4]             = "wine";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Ready;
            SC_TEST_EXPECT(not prepareBuildAction(Build::Action::Run, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
        }
        if (test_section("build run rejects custom runner without runner-path"))
        {
            arguments.tool      = "build";
            arguments.action    = "run";
            args[0]             = "SCTest";
            args[1]             = "--target";
            args[2]             = "windows-gnu-x86_64";
            args[3]             = "--runner";
            args[4]             = "custom";
            arguments.arguments = {args, 5};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Ready;
            SC_TEST_EXPECT(not prepareBuildAction(Build::Action::Run, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
        }
        if (test_section("build cli help is handled without executing builds"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "--help";
            arguments.arguments = {args, 1};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Ready;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::HelpRequested);
        }
        if (test_section("build cli help addendum documents cross targets"))
        {
            String text = StringEncoding::Utf8;
            SC_TEST_EXPECT(writeBuildHelpAddendumToString(Build::Action::Run, text));
            SC_TEST_EXPECT(StringView(text.view()).containsString("windows-gnu-x86_64"));
            SC_TEST_EXPECT(StringView(text.view()).containsString("windows-gnu-arm64"));
            SC_TEST_EXPECT(StringView(text.view()).containsString("windows-msvc-x86_64"));
            SC_TEST_EXPECT(StringView(text.view()).containsString("windows-msvc-arm64"));
            SC_TEST_EXPECT(StringView(text.view()).containsString("linux-glibc-x86_64"));
            SC_TEST_EXPECT(StringView(text.view()).containsString("linux-musl-arm64"));
            SC_TEST_EXPECT(StringView(text.view()).containsString("Current native-backend support matrix"));
            SC_TEST_EXPECT(StringView(text.view())
                               .containsString("macOS -> linux-glibc-arm64: build=supported, run=smoke-supported"));
            SC_TEST_EXPECT(
                StringView(text.view()).containsString("--abi is reserved for a future public ABI selector"));
            SC_TEST_EXPECT(
                StringView(text.view()).containsString("--triple overrides the resolved compiler target triple"));
            SC_TEST_EXPECT(StringView(text.view()).containsString("Fil-C is toolchain-only for now"));
            SC_TEST_EXPECT(StringView(text.view()).containsString("Arguments after -- are forwarded"));
        }
        if (test_section("build cli rejects unknown options"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "--wat";
            arguments.arguments = {args, 1};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Ready;
            SC_TEST_EXPECT(not prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Error);
        }
        if (test_section("build cli passes configuration names through unchanged"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "--config";
            args[2]             = "DebugCoverage";
            arguments.arguments = {args, 3};

            Build::Action           action;
            BuildCLIResolvedStorage storage;
            BuildCLIStatus          status = BuildCLIStatus::Ready;
            SC_TEST_EXPECT(prepareBuildAction(Build::Action::Compile, arguments, action, storage, status));
            SC_TEST_EXPECT(status == BuildCLIStatus::Ready);
            SC_TEST_EXPECT(action.configurationName == "DebugCoverage");
        }
        if (runHeavySections and test_section("coverage"))
        {
            arguments.tool      = "build";
            arguments.action    = "coverage";
            args[0]             = "SCTest";
            args[1]             = "DebugCoverage";
            arguments.arguments = {args, 2};
            SC_TEST_EXPECT(runBuildTool(arguments));
        }
        if (runHeavySections and test_section("compile"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "Debug";
            arguments.arguments = {args, 2};
            SC_TEST_EXPECT(runBuildTool(arguments));
        }
        if (runHeavySections and test_section("run"))
        {
            arguments.tool      = "build";
            arguments.action    = "run";
            args[0]             = "SCTest";
            args[1]             = "Debug";
            arguments.arguments = {args, 2};
            SC_TEST_EXPECT(runBuildTool(arguments));
        }
        if (runHeavySections and test_section("build documentation"))
        {
            arguments.tool      = "build";
            arguments.action    = "documentation";
            arguments.arguments = {};
            SC_TEST_EXPECT(runBuildTool(arguments));
        }
        if (runHeavySections and test_section("install doxygen-awesome-css"))
        {
            arguments.tool      = "package";
            arguments.action    = "install";
            args[0]             = "doxygen-awesome-css";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments));
        }
        if (runHeavySections and test_section("install doxygen"))
        {
            arguments.tool      = "package";
            arguments.action    = "install";
            args[0]             = "doxygen";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments));
        }
        if (runHeavySections and test_section("install clang"))
        {
            arguments.tool      = "package";
            arguments.action    = "install";
            args[0]             = "clang";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments));
        }
        if (test_section("install filc rejects missing import-directory value"))
        {
            arguments.tool      = "package";
            arguments.action    = "install";
            args[0]             = "filc";
            args[1]             = "--import-directory";
            arguments.arguments = {args, 2};
            SC_TEST_EXPECT(not runPackageTool(arguments));
        }
        if (test_section("install filc rejects unknown option"))
        {
            arguments.tool      = "package";
            arguments.action    = "install";
            args[0]             = "filc";
            args[1]             = "--unknown";
            args[2]             = "value";
            arguments.arguments = {args, 3};
            SC_TEST_EXPECT(not runPackageTool(arguments));
        }
        if (test_section("install msvc rejects missing wine option value"))
        {
            arguments.tool      = "package";
            arguments.action    = "install";
            args[0]             = "msvc";
            args[1]             = "--wine";
            arguments.arguments = {args, 2};
            SC_TEST_EXPECT(not runPackageTool(arguments));
        }
        if (test_section("install msvc rejects unknown option"))
        {
            arguments.tool      = "package";
            arguments.action    = "install";
            args[0]             = "msvc";
            args[1]             = "--unknown";
            args[2]             = "value";
            arguments.arguments = {args, 3};
            SC_TEST_EXPECT(not runPackageTool(arguments));
        }
        if (test_section("package list is available"))
        {
            arguments.tool      = "package";
            arguments.action    = "list";
            arguments.arguments = {};
            SC_TEST_EXPECT(runPackageTool(arguments));
        }
        if (test_section("package help is available"))
        {
            arguments.tool      = "package";
            arguments.action    = "help";
            arguments.arguments = {};
            SC_TEST_EXPECT(runPackageTool(arguments));
        }
        if (test_section("package install help is available"))
        {
            arguments.tool      = "package";
            arguments.action    = "install";
            args[0]             = "--help";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments));
        }
        if (test_section("package info describes llvm"))
        {
            arguments.tool      = "package";
            arguments.action    = "info";
            args[0]             = "llvm";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments));
        }
        if (test_section("package info exposes custom adapter phases"))
        {
            arguments.tool      = "package";
            arguments.action    = "info";
            args[0]             = "msvc";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments));
        }
        if (test_section("package info rejects unknown package"))
        {
            arguments.tool      = "package";
            arguments.action    = "info";
            args[0]             = "no-such-package";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(not runPackageTool(arguments));
        }
        if (test_section("package commands accept external registry"))
        {
            static constexpr PackageRegistryExport fakeExports[] = {
                {PackageExportKind::Tool, "fake-tool"},
            };
            static constexpr StringView fakePhases[] = {
                "installFakeRegistryPackage",
                "writeReceipt",
            };
            const PackageRegistryEntry fakeEntry = {
                "fake", "external-fake", "tool",     "External package registry fixture", "host", "test fixture",
                false,  fakeExports,     fakePhases, installFakeRegistryPackage};
            PackageRegistryEntry   registryStorage[16];
            PackageRegistryBuilder registryBuilder = {{registryStorage, 16}};
            SC_TEST_EXPECT(addBuiltinPackages(registryBuilder));
            SC_TEST_EXPECT(registryBuilder.add(fakeEntry));
            const PackageRegistry registry = registryBuilder.registry();
            Package               package;

            SC_TEST_EXPECT(registry.find("clang") != nullptr);
            SC_TEST_EXPECT(registry.find("fake") != nullptr);

            arguments.tool      = "package";
            arguments.action    = "list";
            arguments.arguments = {};
            SC_TEST_EXPECT(runPackageTool(arguments, registry));

            arguments.action    = "info";
            args[0]             = "fake";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments, registry));

            arguments.action    = "install";
            args[0]             = "fake";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments, registry, &package));

            arguments.action    = "status";
            args[0]             = "fake";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments, registry));

            arguments.action    = "exports";
            args[0]             = "fake";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments, registry));

            arguments.action    = "receipt";
            args[0]             = "fake";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments, registry));

            arguments.action    = "verify";
            args[0]             = "fake";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments, registry));

            static constexpr PackageRegistryExport missingExports[] = {
                {PackageExportKind::Tool, "missing-tool"},
            };
            const PackageRegistryEntry badEntry = {
                "fake-missing", "external-fake",
                "tool",         "External package registry fixture with a mismatched contract",
                "host",         "test fixture",
                false,          missingExports,
                fakePhases,     installFakeRegistryPackage};
            const PackageRegistry badRegistry = {{&badEntry, 1}};
            args[0]                           = "fake-missing";
            SC_TEST_EXPECT(not runPackageTool(arguments, badRegistry));

            arguments.action = "doctor";
            args[0]          = "fake";
            SC_TEST_EXPECT(runPackageTool(arguments, registry));
            args[0] = "fake-missing";
            SC_TEST_EXPECT(runPackageTool(arguments, badRegistry));
        }
#if !SC_PLATFORM_WINDOWS
        if (test_section("package repair writes qemu receipt for existing layout"))
        {
            FileSystem fs;
            SC_TEST_EXPECT(fs.init("."));

            String packagesRoot = StringEncoding::Utf8;
            String packageRoot  = StringEncoding::Utf8;
            String binRoot      = StringEncoding::Utf8;
            String qemuX86_64   = StringEncoding::Utf8;
            String qemuArm64    = StringEncoding::Utf8;
            SC_TEST_EXPECT(Path::join(packagesRoot, {outputDirectory.view(), PackagesInstallDirectory}));
            SC_TEST_EXPECT(qemuRepairPackageRoot(packagesRoot.view(), packageRoot));
            SC_TEST_EXPECT(Path::join(binRoot, {packageRoot.view(), "bin"}));
            SC_TEST_EXPECT(Path::join(qemuX86_64, {binRoot.view(), "qemu-x86_64"}));
            SC_TEST_EXPECT(Path::join(qemuArm64, {binRoot.view(), "qemu-aarch64"}));
            if (fs.existsAndIsDirectory(packageRoot.view()))
            {
                SC_TEST_EXPECT(fs.removeDirectoriesRecursive(packageRoot.view()));
            }
            SC_TEST_EXPECT(fs.makeDirectoryRecursive(binRoot.view()));
            SC_TEST_EXPECT(writeFakeQEMURunner(fs, qemuX86_64.view(), "qemu-x86_64"));
            SC_TEST_EXPECT(writeFakeQEMURunner(fs, qemuArm64.view(), "qemu-aarch64"));

            arguments.tool      = "package";
            arguments.action    = "repair";
            args[0]             = "qemu";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments));

            String resolved = StringEncoding::Utf8;
            SC_TEST_EXPECT(resolvePackageCapabilityPath(packageRoot.view(), PackageCapability::RunnerQEMUX86_64,
                                                        resolved));
            SC_TEST_EXPECT(StringView(resolved.view()).endsWith("qemu-x86_64"));
            SC_TEST_EXPECT(resolvePackageCapabilityPath(packageRoot.view(), PackageCapability::RunnerQEMUArm64,
                                                        resolved));
            SC_TEST_EXPECT(StringView(resolved.view()).endsWith("qemu-aarch64"));
        }
#endif
        if (test_section("package repair migrates llvm-mingw compiler exports"))
        {
            FileSystem fs;
            SC_TEST_EXPECT(fs.init("."));

            String packagesRoot = StringEncoding::Utf8;
            String packageRoot  = StringEncoding::Utf8;
            String binRoot      = StringEncoding::Utf8;
            String x64C         = StringEncoding::Utf8;
            String x64CXX       = StringEncoding::Utf8;
            String arm64C       = StringEncoding::Utf8;
            String arm64CXX     = StringEncoding::Utf8;
            String ar           = StringEncoding::Utf8;
            SC_TEST_EXPECT(Path::join(packagesRoot, {outputDirectory.view(), PackagesInstallDirectory}));
            SC_TEST_EXPECT(Path::join(packageRoot, {packagesRoot.view(), "llvm-mingw"}));
            SC_TEST_EXPECT(Path::join(binRoot, {packageRoot.view(), "bin"}));
            SC_TEST_EXPECT(Path::join(x64C, {binRoot.view(), "x86_64-w64-mingw32-clang"}));
            SC_TEST_EXPECT(Path::join(x64CXX, {binRoot.view(), "x86_64-w64-mingw32-clang++"}));
            SC_TEST_EXPECT(Path::join(arm64C, {binRoot.view(), "aarch64-w64-mingw32-clang"}));
            SC_TEST_EXPECT(Path::join(arm64CXX, {binRoot.view(), "aarch64-w64-mingw32-clang++"}));
            SC_TEST_EXPECT(Path::join(ar, {binRoot.view(), "llvm-ar"}));
            if (fs.existsAndIsDirectory(packageRoot.view()))
            {
                SC_TEST_EXPECT(fs.removeDirectoriesRecursive(packageRoot.view()));
            }
            SC_TEST_EXPECT(fs.makeDirectoryRecursive(binRoot.view()));
            SC_TEST_EXPECT(fs.writeString(x64C.view(), "fake"));
            SC_TEST_EXPECT(fs.writeString(x64CXX.view(), "fake"));
            SC_TEST_EXPECT(fs.writeString(arm64C.view(), "fake"));
            SC_TEST_EXPECT(fs.writeString(arm64CXX.view(), "fake"));
            SC_TEST_EXPECT(fs.writeString(ar.view(), "fake"));

            Package package;
            package.installDirectoryLink = packageRoot.view();
            PackageReceiptInfo info;
            info.packageName                     = "llvm-mingw";
            info.packageVersion                  = "old";
            info.recipeVersion                   = "1";
            info.hostPlatform                    = "test";
            info.packageVariant                  = "host";
            info.source                          = "test";
            info.sourceHash                      = "";
            info.validation                      = "passed";
            const StringView phases[]            = {"writeReceipt"};
            info.phases                          = phases;
            const PackageReceiptExport exports[] = {
                {PackageExportKind::Tool, PackageExport::LLVMMinGWClang_X86_64, "bin/x86_64-w64-mingw32-clang"},
                {PackageExportKind::Tool, PackageExport::LLVMMinGWClangArm64, "bin/aarch64-w64-mingw32-clang"},
            };
            SC_TEST_EXPECT(writePackageReceipt(package, info, exports));

            arguments.tool      = "package";
            arguments.action    = "repair";
            args[0]             = "llvm-mingw";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments));

            String resolved = StringEncoding::Utf8;
            SC_TEST_EXPECT(resolvePackageExportPath(packageRoot.view(), PackageExport::LLVMMinGWClangXXArm64,
                                                    resolved));
            SC_TEST_EXPECT(StringView(resolved.view()).endsWith("aarch64-w64-mingw32-clang++"));
            SC_TEST_EXPECT(resolvePackageCapabilityPath(packageRoot.view(),
                                                        PackageCapability::ToolchainWindowsGNUArm64, resolved));
            SC_TEST_EXPECT(StringView(resolved.view()).endsWith("aarch64-w64-mingw32-clang"));
        }
        if (test_section("package commands accept external copy recipe"))
        {
            FileSystem fs;
            SC_TEST_EXPECT(fs.init("."));

            String sourceRoot = StringEncoding::Utf8;
            String binRoot    = StringEncoding::Utf8;
            String toolPath   = StringEncoding::Utf8;
            SC_TEST_EXPECT(Path::join(sourceRoot, {outputDirectory.view(), "ExternalCopyRecipeSource"}));
            SC_TEST_EXPECT(Path::join(binRoot, {sourceRoot.view(), "bin"}));
            SC_TEST_EXPECT(Path::join(toolPath, {binRoot.view(), "fake-tool"}));
            if (fs.existsAndIsDirectory(sourceRoot.view()))
            {
                SC_TEST_EXPECT(fs.removeDirectoriesRecursive(sourceRoot.view()));
            }
            SC_TEST_EXPECT(fs.makeDirectoryRecursive(binRoot.view()));
            SC_TEST_EXPECT(fs.writeString(toolPath.view(), "fake"));

            static constexpr PackageRegistryExport registryExports[] = {
                {PackageExportKind::Tool, "fake-tool"},
            };
            static constexpr StringView phases[] = {
                "copyDirectory",
                "writeReceipt",
            };
            static constexpr PackageReceiptExport receiptExports[] = {
                {PackageExportKind::Tool, "fake-tool", "bin/fake-tool"},
            };
            PackageRecipe recipe;
            recipe.kind                    = PackageRecipeKind::CopyDirectory;
            recipe.copySourceDirectory     = sourceRoot.view();
            recipe.download.packageName    = "external-copy";
            recipe.download.packageVersion = "1";
            recipe.exports                 = receiptExports;
            recipe.phases                  = phases;
            recipe.phaseRegistry           = builtinPackagePhaseRegistry();

            const PackageRegistryEntry recipeEntry = {
                "copy-fake", "external-copy", "tool", "External copy recipe fixture",
                "host",      "test fixture",  false,  registryExports,
                phases,      nullptr,         &recipe};
            const PackageRegistry recipeRegistry = {{&recipeEntry, 1}};

            Package package;
            arguments.tool      = "package";
            arguments.action    = "install";
            args[0]             = "copy-fake";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments, recipeRegistry, &package));
            SC_TEST_EXPECT(fs.existsAndIsDirectory(package.installDirectoryLink.view()));

            arguments.action = "verify";
            SC_TEST_EXPECT(runPackageTool(arguments, recipeRegistry));
            arguments.action = "exports";
            SC_TEST_EXPECT(runPackageTool(arguments, recipeRegistry));

            static constexpr StringView badPhases[] = {
                "missingPhase",
            };
            PackageRecipe badRecipe                   = recipe;
            badRecipe.phases                          = badPhases;
            const PackageRegistryEntry badRecipeEntry = {
                "copy-fake-bad", "external-copy-bad",
                "tool",          "External copy recipe fixture with an unknown phase",
                "host",          "test fixture",
                false,           registryExports,
                badPhases,       nullptr,
                &badRecipe};
            const PackageRegistry badRecipeRegistry = {{&badRecipeEntry, 1}};
            args[0]                                 = "copy-fake-bad";
            arguments.action                        = "install";
            SC_TEST_EXPECT(not runPackageTool(arguments, badRecipeRegistry, &package));
        }
        if (test_section("package receipt resolves exports and capabilities"))
        {
            FileSystem fs;
            SC_TEST_EXPECT(fs.init("."));

            String packageRoot    = StringEncoding::Utf8;
            String toolRoot       = StringEncoding::Utf8;
            String toolPath       = StringEncoding::Utf8;
            String capabilityPath = StringEncoding::Utf8;
            SC_TEST_EXPECT(Path::join(packageRoot, {outputDirectory.view(), "_PackageReceiptTest"}));
            SC_TEST_EXPECT(Path::join(toolRoot, {packageRoot.view(), "bin"}));
            SC_TEST_EXPECT(Path::join(toolPath, {toolRoot.view(), "fake-tool"}));
            SC_TEST_EXPECT(Path::join(capabilityPath, {toolRoot.view(), "fake-capability"}));
            if (fs.existsAndIsDirectory(packageRoot.view()))
            {
                SC_TEST_EXPECT(fs.removeDirectoriesRecursive(packageRoot.view()));
            }
            SC_TEST_EXPECT(fs.makeDirectoryRecursive(toolRoot.view()));
            SC_TEST_EXPECT(fs.writeString(toolPath.view(), "fake"));
            SC_TEST_EXPECT(fs.writeString(capabilityPath.view(), "fake"));

            Package package;
            package.installDirectoryLink = packageRoot.view();
            PackageReceiptInfo info;
            info.packageName          = "fake";
            info.packageVersion       = "1";
            info.recipeVersion        = "1";
            info.hostPlatform         = "test";
            info.packageVariant       = "host";
            info.source               = "test";
            info.sourceHash           = "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
            info.validation           = "passed";
            const StringView phases[] = {
                "resolveFake",
                "validateFake",
                "writeReceipt",
            };
            info.phases                          = phases;
            const PackageReceiptExport exports[] = {
                {PackageExportKind::Tool, "fake-tool", "bin/fake-tool"},
                {PackageExportKind::Tool, "tool.fake", "bin/fake-tool"},
                {PackageExportKind::Capability, "tool.fake", "bin/fake-capability"},
            };
            SC_TEST_EXPECT(writePackageReceipt(package, info, exports));

            String resolved = StringEncoding::Utf8;
            SC_TEST_EXPECT(resolvePackageExportPath(packageRoot.view(), "fake-tool", resolved));
            SC_TEST_EXPECT(resolved.view() == toolPath.view());
            resolved = "";
            SC_TEST_EXPECT(resolvePackageCapabilityPath(packageRoot.view(), "tool.fake", resolved));
            SC_TEST_EXPECT(resolved.view() == capabilityPath.view());
        }
        if (test_section("package receipt rejects invalid source hash"))
        {
            Package package;
            package.installDirectoryLink = outputDirectory.view();
            PackageReceiptInfo info;
            info.packageName    = "fake";
            info.packageVersion = "1";
            info.recipeVersion  = "1";
            info.hostPlatform   = "test";
            info.packageVariant = "host";
            info.source         = "test";
            info.sourceHash     = "sha512:abc";
            info.validation     = "passed";
            SC_TEST_EXPECT(not writePackageReceipt(package, info));

            info.sourceHash = "sha256";
            SC_TEST_EXPECT(not writePackageReceipt(package, info));
        }
        if (test_section("package receipt parses multiline exports"))
        {
            FileSystem fs;
            SC_TEST_EXPECT(fs.init("."));

            String packageRoot = StringEncoding::Utf8;
            String binRoot     = StringEncoding::Utf8;
            String toolPath    = StringEncoding::Utf8;
            String receiptPath = StringEncoding::Utf8;
            SC_TEST_EXPECT(Path::join(packageRoot, {outputDirectory.view(), "_PackageReceiptMultilineTest"}));
            SC_TEST_EXPECT(Path::join(binRoot, {packageRoot.view(), "bin"}));
            SC_TEST_EXPECT(Path::join(toolPath, {binRoot.view(), "fake-tool"}));
            SC_TEST_EXPECT(Path::join(receiptPath, {packageRoot.view(), PackageReceiptFileName}));
            if (fs.existsAndIsDirectory(packageRoot.view()))
            {
                SC_TEST_EXPECT(fs.removeDirectoriesRecursive(packageRoot.view()));
            }
            SC_TEST_EXPECT(fs.makeDirectoryRecursive(binRoot.view()));
            SC_TEST_EXPECT(fs.writeString(toolPath.view(), "fake"));
            SC_TEST_EXPECT(fs.writeString(receiptPath.view(), "{\n"
                                                              "  \"schema\": 1,\n"
                                                              "  \"name\": \"fake\",\n"
                                                              "  \"version\": \"1\",\n"
                                                              "  \"source\": \"test\",\n"
                                                              "  \"sourceHash\": \"\",\n"
                                                              "  \"installRoot\": \"test\",\n"
                                                              "  \"validation\": \"passed\",\n"
                                                              "  \"exports\": [\n"
                                                              "    {\n"
                                                              "      \"kind\": \"capability\",\n"
                                                              "      \"name\": \"tool.fake\",\n"
                                                              "      \"path\": \"bin/fake-tool\"\n"
                                                              "    }\n"
                                                              "  ]\n"
                                                              "}\n"));

            String resolved = StringEncoding::Utf8;
            SC_TEST_EXPECT(resolvePackageCapabilityPath(packageRoot.view(), "tool.fake", resolved));
            SC_TEST_EXPECT(resolved.view() == toolPath.view());
        }
        if (test_section("package receipt rejects escaping export paths"))
        {
            FileSystem fs;
            SC_TEST_EXPECT(fs.init("."));

            String packageRoot = StringEncoding::Utf8;
            String receiptPath = StringEncoding::Utf8;
            SC_TEST_EXPECT(Path::join(packageRoot, {outputDirectory.view(), "_PackageReceiptEscapeTest"}));
            SC_TEST_EXPECT(Path::join(receiptPath, {packageRoot.view(), PackageReceiptFileName}));
            if (fs.existsAndIsDirectory(packageRoot.view()))
            {
                SC_TEST_EXPECT(fs.removeDirectoriesRecursive(packageRoot.view()));
            }
            SC_TEST_EXPECT(fs.makeDirectoryRecursive(packageRoot.view()));
            SC_TEST_EXPECT(fs.writeString(
                receiptPath.view(),
                R"({"schema":1,"name":"fake","version":"1","source":"test","installRoot":"test","validation":"passed","exports":[{"kind":"capability","name":"tool.fake","path":"../escaped"}]})"_a8));

            String resolved = StringEncoding::Utf8;
            SC_TEST_EXPECT(not resolvePackageCapabilityPath(packageRoot.view(), "tool.fake", resolved));
        }
        if (test_section("package receipt rejects unsupported schema during export lookup"))
        {
            FileSystem fs;
            SC_TEST_EXPECT(fs.init("."));

            String packageRoot = StringEncoding::Utf8;
            String binRoot     = StringEncoding::Utf8;
            String toolPath    = StringEncoding::Utf8;
            String receiptPath = StringEncoding::Utf8;
            SC_TEST_EXPECT(Path::join(packageRoot, {outputDirectory.view(), "_PackageReceiptSchemaTest"}));
            SC_TEST_EXPECT(Path::join(binRoot, {packageRoot.view(), "bin"}));
            SC_TEST_EXPECT(Path::join(toolPath, {binRoot.view(), "fake-tool"}));
            SC_TEST_EXPECT(Path::join(receiptPath, {packageRoot.view(), PackageReceiptFileName}));
            if (fs.existsAndIsDirectory(packageRoot.view()))
            {
                SC_TEST_EXPECT(fs.removeDirectoriesRecursive(packageRoot.view()));
            }
            SC_TEST_EXPECT(fs.makeDirectoryRecursive(binRoot.view()));
            SC_TEST_EXPECT(fs.writeString(toolPath.view(), "fake"));
            SC_TEST_EXPECT(fs.writeString(
                receiptPath.view(),
                R"({"schema":99,"name":"fake","exports":[{"kind":"capability","name":"tool.fake","path":"bin/fake-tool"}]})"_a8));

            String resolved = StringEncoding::Utf8;
            SC_TEST_EXPECT(not resolvePackageCapabilityPath(packageRoot.view(), "tool.fake", resolved));
        }
        if (test_section("package receipt rejects duplicate exports during lookup"))
        {
            FileSystem fs;
            SC_TEST_EXPECT(fs.init("."));

            String packageRoot = StringEncoding::Utf8;
            String binRoot     = StringEncoding::Utf8;
            String firstPath   = StringEncoding::Utf8;
            String secondPath  = StringEncoding::Utf8;
            String receiptPath = StringEncoding::Utf8;
            SC_TEST_EXPECT(Path::join(packageRoot, {outputDirectory.view(), "_PackageReceiptDuplicateTest"}));
            SC_TEST_EXPECT(Path::join(binRoot, {packageRoot.view(), "bin"}));
            SC_TEST_EXPECT(Path::join(firstPath, {binRoot.view(), "first-tool"}));
            SC_TEST_EXPECT(Path::join(secondPath, {binRoot.view(), "second-tool"}));
            SC_TEST_EXPECT(Path::join(receiptPath, {packageRoot.view(), PackageReceiptFileName}));
            if (fs.existsAndIsDirectory(packageRoot.view()))
            {
                SC_TEST_EXPECT(fs.removeDirectoriesRecursive(packageRoot.view()));
            }
            SC_TEST_EXPECT(fs.makeDirectoryRecursive(binRoot.view()));
            SC_TEST_EXPECT(fs.writeString(firstPath.view(), "fake"));
            SC_TEST_EXPECT(fs.writeString(secondPath.view(), "fake"));
            SC_TEST_EXPECT(fs.writeString(
                receiptPath.view(),
                R"({"schema":1,"name":"fake","exports":[{"kind":"capability","name":"tool.fake","path":"bin/first-tool"},{"kind":"capability","name":"tool.fake","path":"bin/second-tool"}]})"_a8));

            String resolved = StringEncoding::Utf8;
            SC_TEST_EXPECT(not resolvePackageCapabilityPath(packageRoot.view(), "tool.fake", resolved));
        }
        if (test_section("package status can scan registry"))
        {
            arguments.tool      = "package";
            arguments.action    = "status";
            arguments.arguments = {};
            SC_TEST_EXPECT(runPackageTool(arguments));
        }
        if (test_section("package verify can scan installed receipts"))
        {
            arguments.tool      = "package";
            arguments.action    = "verify";
            arguments.arguments = {};
            SC_TEST_EXPECT(runPackageTool(arguments));
        }
        if (test_section("package receipt prints installed receipt"))
        {
            FileSystem fs;
            SC_TEST_EXPECT(fs.init("."));

            String packagesRoot = StringEncoding::Utf8;
            String packageRoot  = StringEncoding::Utf8;
            String binRoot      = StringEncoding::Utf8;
            String toolPath     = StringEncoding::Utf8;
            SC_TEST_EXPECT(Path::join(packagesRoot, {outputDirectory.view(), PackagesInstallDirectory}));
            SC_TEST_EXPECT(Path::join(packageRoot, {packagesRoot.view(), "_ReceiptCommandClang"}));
            SC_TEST_EXPECT(Path::join(binRoot, {packageRoot.view(), "bin"}));
            SC_TEST_EXPECT(Path::join(toolPath, {binRoot.view(), "clang-format"}));
            if (fs.existsAndIsDirectory(packageRoot.view()))
            {
                SC_TEST_EXPECT(fs.removeDirectoriesRecursive(packageRoot.view()));
            }
            SC_TEST_EXPECT(fs.makeDirectoryRecursive(binRoot.view()));
            SC_TEST_EXPECT(fs.writeString(toolPath.view(), "fake"));

            Package package;
            package.installDirectoryLink = packageRoot.view();
            PackageReceiptInfo info;
            info.packageName                     = "clang-binaries";
            info.packageVersion                  = "1";
            info.recipeVersion                   = "1";
            info.hostPlatform                    = "test";
            info.packageVariant                  = "host";
            info.source                          = "test";
            info.sourceHash                      = "";
            info.validation                      = "passed";
            const StringView phases[]            = {"writeReceipt"};
            info.phases                          = phases;
            const PackageReceiptExport exports[] = {
                {PackageExportKind::Tool, "clang-format", "bin/clang-format"},
            };
            SC_TEST_EXPECT(writePackageReceipt(package, info, exports));

            arguments.tool      = "package";
            arguments.action    = "receipt";
            args[0]             = "clang";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments));
            SC_TEST_EXPECT(fs.removeDirectoriesRecursive(packageRoot.view()));
        }
        if (test_section("package exports prints installed receipt exports"))
        {
            FileSystem fs;
            SC_TEST_EXPECT(fs.init("."));

            String packagesRoot = StringEncoding::Utf8;
            String packageRoot  = StringEncoding::Utf8;
            String binRoot      = StringEncoding::Utf8;
            String toolPath     = StringEncoding::Utf8;
            SC_TEST_EXPECT(Path::join(packagesRoot, {outputDirectory.view(), PackagesInstallDirectory}));
            SC_TEST_EXPECT(Path::join(packageRoot, {packagesRoot.view(), "_ExportsCommandClang"}));
            SC_TEST_EXPECT(Path::join(binRoot, {packageRoot.view(), "bin"}));
            SC_TEST_EXPECT(Path::join(toolPath, {binRoot.view(), "clang-format"}));
            if (fs.existsAndIsDirectory(packageRoot.view()))
            {
                SC_TEST_EXPECT(fs.removeDirectoriesRecursive(packageRoot.view()));
            }
            SC_TEST_EXPECT(fs.makeDirectoryRecursive(binRoot.view()));
            SC_TEST_EXPECT(fs.writeString(toolPath.view(), "fake"));

            Package package;
            package.installDirectoryLink = packageRoot.view();
            PackageReceiptInfo info;
            info.packageName                     = "clang-binaries";
            info.packageVersion                  = "1";
            info.recipeVersion                   = "1";
            info.hostPlatform                    = "test";
            info.packageVariant                  = "host";
            info.source                          = "test";
            info.sourceHash                      = "";
            info.validation                      = "passed";
            const StringView phases[]            = {"writeReceipt"};
            info.phases                          = phases;
            const PackageReceiptExport exports[] = {
                {PackageExportKind::Tool, "clang-format", "bin/clang-format"},
            };
            SC_TEST_EXPECT(writePackageReceipt(package, info, exports));

            arguments.tool      = "package";
            arguments.action    = "exports";
            args[0]             = "clang";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments));
            SC_TEST_EXPECT(fs.removeDirectoriesRecursive(packageRoot.view()));
        }
        if (test_section("package lock writes package identities"))
        {
            arguments.tool      = "package";
            arguments.action    = "lock";
            arguments.arguments = {};
            SC_TEST_EXPECT(runPackageTool(arguments));

            String lockPath = StringEncoding::Utf8;
            String lockText = StringEncoding::Utf8;
            SC_TEST_EXPECT(Path::join(lockPath, {outputDirectory.view(), "SC-package.lock"}));
            SC_TEST_EXPECT(readFileIntoString(lockPath.view(), lockText));
            SC_TEST_EXPECT(StringView(lockText.view()).containsString("\"tool\""));
            SC_TEST_EXPECT(StringView(lockText.view()).containsString("\"generatedAt\""));
            SC_TEST_EXPECT(StringView(lockText.view()).containsString("\"hostPlatform\""));
            SC_TEST_EXPECT(StringView(lockText.view()).containsString("\"packageCount\""));
            SC_TEST_EXPECT(StringView(lockText.view()).containsString("\"packages\""));
            SC_TEST_EXPECT(StringView(lockText.view()).containsString("\"exports\""));
        }
        if (runHeavySections and test_section("clang-format execute"))
        {
            arguments.tool      = "format";
            arguments.action    = "execute";
            args[0]             = "clang";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runFormatTool(arguments));
        }
        if (runHeavySections and test_section("clang-format check"))
        {
            arguments.tool      = "format";
            arguments.action    = "check";
            args[0]             = "clang";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runFormatTool(arguments));
        }
    }
};
void runSupportToolsTest(SC::TestReport& report) { SupportToolsTest test(report); }
} // namespace SC
