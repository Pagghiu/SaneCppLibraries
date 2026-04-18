// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Tools/Tools.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Strings/StringFormat.h"
#include "Libraries/Testing/Testing.h"
#include "Tools/SC-build.h"

extern SC::Console* globalConsole;
namespace SC
{
static bool writeBuildHelpAddendumToString(Build::Action::Type actionType, String& text)
{
    GrowableBuffer<String> growable(text);
    StringFormatOutput     output(StringEncoding::Utf8, growable);
    const bool             result = Tools::appendBuildActionHelpAddendum(output, actionType);
    growable.finalize();
    return result;
}

struct SupportToolsTest : public TestCase
{
    SupportToolsTest(SC::TestReport& report) : TestCase(report, "SupportToolsTest")
    {
        using namespace SC::Tools;
        StringPath outputDirectory;
        (void)StringBuilder::format(outputDirectory, "{0}/_Build", report.libraryRootDirectory);
        Tool::Arguments arguments{*globalConsole,
                                  report.libraryRootDirectory,
                                  report.libraryRootDirectory,
                                  outputDirectory,
                                  StringView(),
                                  StringView(),
                                  {}};

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
            SC_TEST_EXPECT(
                StringView(text.view()).containsString("--triple overrides the resolved compiler target triple"));
            SC_TEST_EXPECT(
                StringView(text.view()).containsString("non-Linux hosts auto-select a packaged LLVM toolchain"));
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
        if (test_section("coverage"))
        {
            arguments.tool      = "build";
            arguments.action    = "coverage";
            args[0]             = "SCTest";
            args[1]             = "DebugCoverage";
            arguments.arguments = {args, 2};
            SC_TEST_EXPECT(runBuildTool(arguments));
        }
        if (test_section("compile"))
        {
            arguments.tool      = "build";
            arguments.action    = "compile";
            args[0]             = "SCTest";
            args[1]             = "Debug";
            arguments.arguments = {args, 2};
            SC_TEST_EXPECT(runBuildTool(arguments));
        }
        if (test_section("run"))
        {
            arguments.tool      = "build";
            arguments.action    = "run";
            args[0]             = "SCTest";
            args[1]             = "Debug";
            arguments.arguments = {args, 2};
            SC_TEST_EXPECT(runBuildTool(arguments));
        }
        if (test_section("build documentation"))
        {
            arguments.tool      = "build";
            arguments.action    = "documentation";
            arguments.arguments = {};
            SC_TEST_EXPECT(runBuildTool(arguments));
        }
        if (test_section("install doxygen-awesome-css"))
        {
            arguments.tool      = "package";
            arguments.action    = "install";
            args[0]             = "doxygen-awesome-css";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments));
        }
        if (test_section("install doxygen"))
        {
            arguments.tool      = "package";
            arguments.action    = "install";
            args[0]             = "doxygen";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments));
        }
        if (test_section("install clang"))
        {
            arguments.tool      = "package";
            arguments.action    = "install";
            args[0]             = "clang";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runPackageTool(arguments));
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
        if (test_section("clang-format execute"))
        {
            arguments.tool      = "format";
            arguments.action    = "execute";
            args[0]             = "clang";
            arguments.arguments = {args, 1};
            SC_TEST_EXPECT(runFormatTool(arguments));
        }
        if (test_section("clang-format check"))
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
