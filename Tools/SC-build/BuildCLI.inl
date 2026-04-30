// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Libraries/Process/Process.h"

namespace SC
{
namespace Tools
{
namespace detail
{
static constexpr StringView PROJECTS_SUBDIR      = "_Projects";
static constexpr StringView OUTPUTS_SUBDIR       = "_Outputs";
static constexpr StringView INTERMEDIATES_SUBDIR = "_Intermediates";
static constexpr StringView BUILD_CACHE_SUBDIR   = "_BuildCache";

static bool isShortOptionToken(StringView token)
{
    return token.sizeInBytes() >= 2 and token.startsWith("-") and not token.startsWith("--");
}

static bool isLongOptionToken(StringView token) { return token.sizeInBytes() >= 3 and token.startsWith("--"); }

Result appendBuildActionHelpAddendum(StringFormatOutput& output, Build::Action::Type actionType)
{
    if (actionType == Build::Action::Compile or actionType == Build::Action::Run)
    {
        SC_TRY_MSG(output.append("\nTarget profiles:\n"
                                 "  - host / native: build for the current host machine\n"
                                 "  - windows-gnu-x86_64: Windows GNU target through llvm-mingw\n"
                                 "  - linux-glibc-x86_64: Linux glibc target profile\n"
                                 "  - linux-glibc-arm64: Linux glibc arm64 target profile\n"
                                 "  - linux-musl-x86_64: Linux musl target profile\n"
                                 "  - linux-musl-arm64: Linux musl arm64 target profile\n"
                                 "  - windows-msvc-x86_64: Windows MSVC target through portable MSVC + Wine\n"
                                 "  - windows-msvc-arm64: Windows MSVC arm64 target through portable MSVC + Wine\n"
                                 "  - windows-gnu-arm64: Windows GNU arm64 target through llvm-mingw\n"),
                   "Failed writing SC-build help");
        SC_TRY_MSG(output.append("\nToolchain values:\n"
                                 "  - default / host-default: host-default compiler family\n"
                                 "  - clang: explicit clang-family driver\n"
                                 "  - gcc: explicit GCC-family driver\n"
                                 "  - msvc: explicit MSVC toolchain family\n"
                                 "  - clang-cl: explicit clang-cl toolchain family\n"
                                 "  - llvm-mingw: packaged Windows GNU cross-toolchain\n"
                                 "  - filc: experimental Linux-only Fil-C compiler track\n"),
                   "Failed writing SC-build help");
        SC_TRY_MSG(
            output.append(
                "\nCurrent tested cross-target support:\n"
                "  - macOS and Linux hosts can compile windows-gnu-x86_64 and windows-gnu-arm64\n"
                "  - macOS hosts can compile windows-msvc-x86_64 and windows-msvc-arm64 through portable MSVC + Wine\n"
                "  - macOS hosts can compile linux-glibc-x86_64, linux-glibc-arm64, linux-musl-x86_64, and "
                "linux-musl-arm64 through packaged LLVM + packaged sysroots\n"
                "  - Linux hosts can also experiment with Fil-C through --toolchain filc for native Linux builds\n"
                "  - build run can auto-route x86_64 Windows targets through Wine on macOS and Linux\n"
                "  - build run can also wrap foreign Linux targets through qemu-user when a suitable packaged or "
                "host qemu executable and sysroot are available\n"
                "  - Linux arm64 can auto-wrap box64 + wine64 for build run when those host tools are installed\n"
                "  - Windows arm64 runs now require a Wine runtime that ships an arm64 Windows loader; the packaged "
                "macOS runner does not yet\n"),
            "Failed writing SC-build help");
        SC_TRY_MSG(output.append(
                       "\nRaw override escape hatches:\n"
                       "  - --triple overrides the resolved compiler target triple\n"
                       "  - --sysroot overrides the resolved toolchain sysroot\n"
                       "  - raw overrides apply after --target and therefore take precedence over friendly profiles\n"),
                   "Failed writing SC-build help");
    }

    if (actionType == Build::Action::Run)
    {
        SC_TRY_MSG(output.append("\nRunner values:\n"
                                 "  - auto: use a host-specific runner when the host/target pair supports it\n"
                                 "  - none: disable foreign-runner wrapping\n"
                                 "  - wine: force Wine for Windows GNU targets\n"
                                 "  - qemu: wrap foreign Linux targets through qemu-user\n"
                                 "  - custom: wrap execution with a custom executable\n"),
                   "Failed writing SC-build help");
        SC_TRY_MSG(output.append("\nArguments after -- are forwarded to the built executable.\n"),
                   "Failed writing SC-build help");
    }
    SC_TRY_MSG(output.append("\nLegacy compatibility: after <target> you can still pass up to four positional "
                             "values in this order: <config> <generator> <arch> <output>.\n"),
               "Failed writing SC-build help");
    return Result(true);
}

static Result printBuildActionHelp(const CommandLineSpec& spec, Build::Action::Type actionType, Console& console)
{
    StringFormatOutput output(StringEncoding::Utf8, console, true);
    SC_TRY_MSG(spec.writeHelp(output), "Failed writing SC-build help");
    SC_TRY(appendBuildActionHelpAddendum(output, actionType));
    console.flush();
    return Result(true);
}

static Result printBuildActionParseError(const CommandLineSpec& spec, const CommandLineParseResult& parseResult,
                                         Console& console)
{
    StringFormatOutput output(StringEncoding::Utf8, console, false);
    if (parseResult.error == CommandLineParseResult::Error::InsufficientPositionalStorage)
    {
        SC_TRY_MSG(output.append("Too many legacy positional arguments after <target>. Supported order is: <config> "
                                 "<generator> <arch> <output>.\nUse --help to show usage.\n"),
                   "Failed writing SC-build parse error");
    }
    else
    {
        SC_TRY_MSG(spec.writeError(parseResult, output), "Failed writing SC-build parse error");
    }
    console.flushStdErr();
    return Result(true);
}

static Result printBuildActionValueError(Console& console, StringView optionName, StringView value, StringView message)
{
    console.printError("{} {}: {}\n", message, optionName, value);
    console.flushStdErr();
    return Result::Error("Invalid SC-build option value");
}

static Result printBuildActionAmbiguity(Console& console, StringView optionName, StringView value,
                                        Span<const StringView> matches)
{
    SmallString<256> details = StringEncoding::Ascii;
    auto             builder = StringBuilder::create(details);
    SC_TRY(builder.append("Ambiguous value for {}: {} (matches: ", optionName, value));
    for (size_t idx = 0; idx < matches.sizeInElements(); ++idx)
    {
        if (idx > 0)
        {
            SC_TRY(builder.append(", "));
        }
        SC_TRY(builder.append(matches[idx]));
    }
    SC_TRY(builder.append(")\n"));
    builder.finalize();
    console.printError(details.view());
    console.flushStdErr();
    return Result::Error("Ambiguous SC-build option value");
}

static Result splitBuildArgumentsAtTerminator(Span<const StringView>  arguments,
                                              Span<const StringView>& beforeTerminator,
                                              Span<const StringView>& afterTerminator)
{
    beforeTerminator = arguments;
    afterTerminator  = {};
    for (size_t idx = 0; idx < arguments.sizeInElements(); ++idx)
    {
        if (arguments[idx] == "--")
        {
            beforeTerminator = {arguments.data(), idx};
            SC_TRY(arguments.sliceStart(idx + 1, afterTerminator));
            break;
        }
    }
    return Result(true);
}

static void applyHostDefaultBuildParameters(Build::Action& action)
{
    switch (HostPlatform)
    {
    case Platform::Windows: action.parameters.hostMachine.platform = Build::Platform::Windows; break;
    case Platform::Apple: action.parameters.hostMachine.platform = Build::Platform::Apple; break;
    case Platform::Linux: action.parameters.hostMachine.platform = Build::Platform::Linux; break;
    default: break;
    }
    switch (HostInstructionSet)
    {
    case InstructionSet::ARM64: action.parameters.hostMachine.architecture = Build::Architecture::Arm64; break;
    case InstructionSet::Intel64: action.parameters.hostMachine.architecture = Build::Architecture::Intel64; break;
    case InstructionSet::Intel32: action.parameters.hostMachine.architecture = Build::Architecture::Intel32; break;
    }
    action.parameters.hostMachine.environment = Build::TargetEnvironment::Native;

    switch (HostPlatform)
    {
    case Platform::Windows:
        action.parameters.generator = Build::Generator::Native;
        action.parameters.platform  = Build::Platform::Windows;
        break;
    case Platform::Apple:
        action.parameters.generator = Build::Generator::Native;
        action.parameters.platform  = Build::Platform::Apple;
        break;
    case Platform::Linux:
        action.parameters.generator = Build::Generator::Native;
        action.parameters.platform  = Build::Platform::Linux;
        break;
    default: break;
    }
    action.parameters.targetMachine.platform     = action.parameters.platform;
    action.parameters.targetMachine.architecture = action.parameters.architecture;
    action.parameters.targetMachine.environment  = Build::TargetEnvironment::Native;
}

static Result applyTargetProfileValue(Build::Action& action, StringView targetProfile, Console& console)
{
    if (targetProfile.isEmpty())
    {
        return Result(true);
    }

    StringView resolved = targetProfile;
    if (targetProfile.equalsIgnoreCaseASCII("h"))
    {
        resolved = "host";
    }
    else if (targetProfile.equalsIgnoreCaseASCII("n"))
    {
        resolved = "native";
    }
    else if (targetProfile.equalsIgnoreCaseASCII("windows-gnu-x64"))
    {
        resolved = "windows-gnu-x86_64";
    }
    else if (targetProfile.equalsIgnoreCaseASCII("windows-gnu-aarch64"))
    {
        resolved = "windows-gnu-arm64";
    }
    else if (targetProfile.equalsIgnoreCaseASCII("linux-glibc-x64"))
    {
        resolved = "linux-glibc-x86_64";
    }
    else if (targetProfile.equalsIgnoreCaseASCII("linux-glibc-aarch64"))
    {
        resolved = "linux-glibc-arm64";
    }
    else if (targetProfile.equalsIgnoreCaseASCII("linux-musl-x64"))
    {
        resolved = "linux-musl-x86_64";
    }
    else if (targetProfile.equalsIgnoreCaseASCII("linux-musl-aarch64"))
    {
        resolved = "linux-musl-arm64";
    }
    else if (targetProfile.equalsIgnoreCaseASCII("windows-msvc-x64"))
    {
        resolved = "windows-msvc-x86_64";
    }
    else if (targetProfile.equalsIgnoreCaseASCII("windows-msvc-aarch64"))
    {
        resolved = "windows-msvc-arm64";
    }
    else if (not(targetProfile.equalsIgnoreCaseASCII("host") or targetProfile.equalsIgnoreCaseASCII("native") or
                 targetProfile.equalsIgnoreCaseASCII("linux-glibc-x86_64") or
                 targetProfile.equalsIgnoreCaseASCII("linux-glibc-arm64") or
                 targetProfile.equalsIgnoreCaseASCII("linux-musl-x86_64") or
                 targetProfile.equalsIgnoreCaseASCII("linux-musl-arm64") or
                 targetProfile.equalsIgnoreCaseASCII("windows-msvc-x86_64") or
                 targetProfile.equalsIgnoreCaseASCII("windows-msvc-arm64") or
                 targetProfile.equalsIgnoreCaseASCII("windows-gnu-x86_64") or
                 targetProfile.equalsIgnoreCaseASCII("windows-gnu-arm64")))
    {
        return printBuildActionValueError(console, "--target", targetProfile, "Unknown value for");
    }

    if (resolved.equalsIgnoreCaseASCII("host") or resolved.equalsIgnoreCaseASCII("native"))
    {
        action.parameters.targetMachine    = action.parameters.hostMachine;
        action.parameters.platform         = action.parameters.targetMachine.platform;
        action.parameters.architecture     = action.parameters.targetMachine.architecture;
        action.parameters.toolchain.family = Build::Toolchain::HostDefault;
        SC_TRY(action.parameters.toolchain.targetTriple.assign({}));
        return Result(true);
    }

    action.parameters.targetMachine.platform = Build::Platform::Windows;
    action.parameters.platform               = Build::Platform::Windows;
    action.parameters.generator              = Build::Generator::Native;

    if (resolved.equalsIgnoreCaseASCII("linux-glibc-x86_64") or resolved.equalsIgnoreCaseASCII("linux-glibc-arm64") or
        resolved.equalsIgnoreCaseASCII("linux-musl-x86_64") or resolved.equalsIgnoreCaseASCII("linux-musl-arm64"))
    {
        action.parameters.targetMachine.platform = Build::Platform::Linux;
        action.parameters.platform               = Build::Platform::Linux;
        action.parameters.generator              = Build::Generator::Native;
        action.parameters.toolchain.family       = Build::Toolchain::Clang;

        const bool isArm64 =
            resolved.equalsIgnoreCaseASCII("linux-glibc-arm64") or resolved.equalsIgnoreCaseASCII("linux-musl-arm64");
        const bool isMusl =
            resolved.equalsIgnoreCaseASCII("linux-musl-x86_64") or resolved.equalsIgnoreCaseASCII("linux-musl-arm64");

        action.parameters.targetMachine.architecture =
            isArm64 ? Build::Architecture::Arm64 : Build::Architecture::Intel64;
        action.parameters.architecture = action.parameters.targetMachine.architecture;
        action.parameters.targetMachine.environment =
            isMusl ? Build::TargetEnvironment::LinuxMusl : Build::TargetEnvironment::LinuxGlibc;
        const StringView targetTriple =
            isArm64 ? (isMusl ? "aarch64-unknown-linux-musl"_a8 : "aarch64-unknown-linux-gnu"_a8)
                    : (isMusl ? "x86_64-unknown-linux-musl"_a8 : "x86_64-unknown-linux-gnu"_a8);
        SC_TRY(action.parameters.toolchain.targetTriple.assign(targetTriple));
        return Result(true);
    }

    if (resolved.equalsIgnoreCaseASCII("windows-msvc-x86_64") or resolved.equalsIgnoreCaseASCII("windows-msvc-arm64"))
    {
        action.parameters.targetMachine.environment = Build::TargetEnvironment::WindowsMSVC;
        if (resolved.equalsIgnoreCaseASCII("windows-msvc-arm64"))
        {
            action.parameters.targetMachine.architecture = Build::Architecture::Arm64;
            action.parameters.architecture               = Build::Architecture::Arm64;
        }
        else
        {
            action.parameters.targetMachine.architecture = Build::Architecture::Intel64;
            action.parameters.architecture               = Build::Architecture::Intel64;
        }
        action.parameters.toolchain.family = Build::Toolchain::MSVC;
        SC_TRY(action.parameters.toolchain.targetTriple.assign({}));
        return Result(true);
    }

    action.parameters.targetMachine.environment = Build::TargetEnvironment::WindowsGNU;
    action.parameters.toolchain.family          = Build::Toolchain::LLVMMingw;

    if (resolved.equalsIgnoreCaseASCII("windows-gnu-arm64"))
    {
        action.parameters.targetMachine.architecture = Build::Architecture::Arm64;
        action.parameters.architecture               = Build::Architecture::Arm64;
        SC_TRY(action.parameters.toolchain.targetTriple.assign("aarch64-w64-windows-gnu"));
    }
    else
    {
        action.parameters.targetMachine.architecture = Build::Architecture::Intel64;
        action.parameters.architecture               = Build::Architecture::Intel64;
        SC_TRY(action.parameters.toolchain.targetTriple.assign("x86_64-w64-windows-gnu"));
    }
    return Result(true);
}

static Result setBuildActionTarget(Build::Action& action, StringView target)
{
    if (target.isEmpty())
    {
        return Result(true);
    }
    if (target.splitBefore(SC_NATIVE_STR(":"), action.workspaceName))
    {
        SC_TRY(target.splitAfter(SC_NATIVE_STR(":"), action.projectName));
    }
    else
    {
        action.projectName = target;
    }
    return Result(true);
}

template <size_t N>
static Result resolveKeywordValue(StringView optionName, StringView input, const StringView (&candidates)[N],
                                  StringView& resolved, Console& console)
{
    SmallVector<StringView, N> matches;

    for (const auto& candidate : candidates)
    {
        if (candidate.equalsIgnoreCaseASCII(input))
        {
            resolved = candidate;
            return Result(true);
        }
    }

    for (const auto& candidate : candidates)
    {
        if (candidate.startsWithIgnoreCaseASCII(input))
        {
            SC_TRY(matches.push_back(candidate));
        }
    }

    if (matches.size() == 1)
    {
        resolved = matches[0];
        return Result(true);
    }
    if (matches.size() > 1)
    {
        return printBuildActionAmbiguity(console, optionName, input, matches.toSpanConst());
    }
    return printBuildActionValueError(console, optionName, input, "Unknown value for");
}

static Result resolveConfigurationValue(StringView input, BuildCLIResolvedStorage& storage, StringView& resolved)
{
    if (input.equalsIgnoreCaseASCII("d"))
    {
        SC_TRY(storage.configurationName.assign("Debug"));
        resolved = storage.configurationName.view();
        return Result(true);
    }
    if (input.equalsIgnoreCaseASCII("r"))
    {
        SC_TRY(storage.configurationName.assign("Release"));
        resolved = storage.configurationName.view();
        return Result(true);
    }
    if (input.equalsIgnoreCaseASCII("dc"))
    {
        SC_TRY(storage.configurationName.assign("DebugCoverage"));
        resolved = storage.configurationName.view();
        return Result(true);
    }
    if (input.equalsIgnoreCaseASCII("dv"))
    {
        SC_TRY(storage.configurationName.assign("DebugValgrind"));
        resolved = storage.configurationName.view();
        return Result(true);
    }

    SC_TRY(storage.configurationName.assign(input));
    resolved = storage.configurationName.view();
    return Result(true);
}

static Result applyConfigurationValue(Build::Action& action, StringView configurationValue,
                                      BuildCLIResolvedStorage& storage, Console& console)
{
    if (configurationValue.isEmpty())
    {
        return Result(true);
    }
    StringView resolved;
    SC_COMPILER_UNUSED(console);
    SC_TRY(resolveConfigurationValue(configurationValue, storage, resolved));
    action.configurationName = resolved;
    return Result(true);
}

static Result applyGeneratorValue(Build::Action& action, StringView generatorValue, Console& console)
{
    if (generatorValue.isEmpty())
    {
        return Result(true);
    }
    static constexpr StringView generatorNames[] = {"default", "native", "make", "xcode", "vs2022", "vs2019"};
    StringView                  resolved;
    SC_TRY(resolveKeywordValue("--generator", generatorValue, generatorNames, resolved, console));
    if (resolved.equalsIgnoreCaseASCII("native"))
    {
        action.parameters.generator = Build::Generator::Native;
    }
    else if (resolved.equalsIgnoreCaseASCII("make"))
    {
        action.parameters.generator = Build::Generator::Make;
    }
    else if (resolved.equalsIgnoreCaseASCII("xcode"))
    {
        action.parameters.generator = Build::Generator::XCode;
    }
    else if (resolved.equalsIgnoreCaseASCII("vs2022"))
    {
        action.parameters.generator = Build::Generator::VisualStudio2022;
    }
    else if (resolved.equalsIgnoreCaseASCII("vs2019"))
    {
        action.parameters.generator = Build::Generator::VisualStudio2019;
    }
    return Result(true);
}

static Result applyArchitectureValue(Build::Action& action, StringView architectureValue, Console& console)
{
    if (architectureValue.isEmpty())
    {
        return Result(true);
    }
    static constexpr StringView architectureNames[] = {"arm64", "intel32", "intel64", "wasm", "any"};
    StringView                  resolved;
    SC_TRY(resolveKeywordValue("--arch", architectureValue, architectureNames, resolved, console));
    if (resolved.equalsIgnoreCaseASCII("arm64"))
    {
        action.parameters.architecture = Build::Architecture::Arm64;
    }
    else if (resolved.equalsIgnoreCaseASCII("intel32"))
    {
        action.parameters.architecture = Build::Architecture::Intel32;
    }
    else if (resolved.equalsIgnoreCaseASCII("intel64"))
    {
        action.parameters.architecture = Build::Architecture::Intel64;
    }
    else if (resolved.equalsIgnoreCaseASCII("wasm"))
    {
        action.parameters.architecture = Build::Architecture::Wasm;
    }
    else if (resolved.equalsIgnoreCaseASCII("any"))
    {
        action.parameters.architecture = Build::Architecture::Any;
    }
    return Result(true);
}

static Result applyToolchainValue(Build::Action& action, StringView toolchainValue, Console& console)
{
    if (toolchainValue.isEmpty())
    {
        return Result(true);
    }
    static constexpr StringView toolchainNames[] = {"default", "host-default", "clang",    "filc",
                                                    "gcc",     "msvc",         "clang-cl", "llvm-mingw"};
    StringView                  resolved;
    SC_TRY(resolveKeywordValue("--toolchain", toolchainValue, toolchainNames, resolved, console));
    action.parameters.toolchain.platform     = Build::Platform::Unknown;
    action.parameters.toolchain.architecture = Build::Architecture::Any;
    if (resolved.equalsIgnoreCaseASCII("clang"))
    {
        action.parameters.toolchain.family = Build::Toolchain::Clang;
    }
    else if (resolved.equalsIgnoreCaseASCII("filc"))
    {
        action.parameters.toolchain.family       = Build::Toolchain::FilC;
        action.parameters.toolchain.platform     = Build::Platform::Linux;
        action.parameters.toolchain.architecture = Build::Architecture::Intel64;
        if (action.parameters.architecture == Build::Architecture::Any)
        {
            action.parameters.architecture = Build::Architecture::Intel64;
        }
        if (action.parameters.targetMachine.architecture == Build::Architecture::Any)
        {
            action.parameters.targetMachine.architecture = Build::Architecture::Intel64;
        }
    }
    else if (resolved.equalsIgnoreCaseASCII("gcc"))
    {
        action.parameters.toolchain.family = Build::Toolchain::GCC;
    }
    else if (resolved.equalsIgnoreCaseASCII("msvc"))
    {
        action.parameters.toolchain.family = Build::Toolchain::MSVC;
    }
    else if (resolved.equalsIgnoreCaseASCII("clang-cl"))
    {
        action.parameters.toolchain.family = Build::Toolchain::ClangCL;
    }
    else if (resolved.equalsIgnoreCaseASCII("llvm-mingw"))
    {
        action.parameters.toolchain.family = Build::Toolchain::LLVMMingw;
    }
    else
    {
        action.parameters.toolchain.family = Build::Toolchain::HostDefault;
    }
    return Result(true);
}

static Result resolveOutputModeValue(StringView value, Build::OutputMode::Type& outputMode, Console& console)
{
    static constexpr StringView outputNames[] = {"quiet", "normal", "verbose"};
    StringView                  resolved;
    SC_TRY(resolveKeywordValue("--output", value, outputNames, resolved, console));
    if (resolved.equalsIgnoreCaseASCII("quiet"))
    {
        outputMode = Build::OutputMode::Quiet;
    }
    else if (resolved.equalsIgnoreCaseASCII("normal"))
    {
        outputMode = Build::OutputMode::Normal;
    }
    else
    {
        outputMode = Build::OutputMode::Verbose;
    }
    return Result(true);
}

static Result applyRunnerValue(Build::Action& action, StringView runnerValue, Console& console)
{
    if (runnerValue.isEmpty())
    {
        return Result(true);
    }

    static constexpr StringView runnerNames[] = {"auto", "none", "wine", "qemu", "custom"};
    StringView                  resolved;
    SC_TRY(resolveKeywordValue("--runner", runnerValue, runnerNames, resolved, console));
    if (resolved.equalsIgnoreCaseASCII("none"))
    {
        action.parameters.runner.type = Build::RunnerSpec::None;
    }
    else if (resolved.equalsIgnoreCaseASCII("wine"))
    {
        action.parameters.runner.type = Build::RunnerSpec::Wine;
    }
    else if (resolved.equalsIgnoreCaseASCII("qemu"))
    {
        action.parameters.runner.type = Build::RunnerSpec::QEMU;
    }
    else if (resolved.equalsIgnoreCaseASCII("custom"))
    {
        action.parameters.runner.type = Build::RunnerSpec::Custom;
    }
    else
    {
        action.parameters.runner.type = Build::RunnerSpec::Auto;
    }
    return Result(true);
}

static Result printBuildActionCombinationError(Console& console, StringView message)
{
    console.printError("{}\n", message);
    console.flushStdErr();
    return Result::Error("Invalid SC-build option combination");
}

static Result resolveBuildGeneratorKeyword(StringView value, StringView& resolved, Console& console)
{
    static constexpr StringView generatorNames[] = {"default", "native", "make", "xcode", "vs2022", "vs2019"};
    return resolveKeywordValue("--generator", value, generatorNames, resolved, console);
}

static Result resolveBuildArchitectureKeyword(StringView value, StringView& resolved, Console& console)
{
    static constexpr StringView architectureNames[] = {"arm64", "intel32", "intel64", "wasm", "any"};
    return resolveKeywordValue("--arch", value, architectureNames, resolved, console);
}

static bool targetMachineCanRunDirectly(const Build::Machine& hostMachine, const Build::Machine& target)
{
    if (target.platform != hostMachine.platform)
    {
        return false;
    }
    if (target.environment != Build::TargetEnvironment::Native)
    {
        return false;
    }
    return target.architecture == Build::Architecture::Any or target.architecture == hostMachine.architecture;
}

static bool isWindowsGNUTargetMachine(const Build::Machine& target)
{
    return target.platform == Build::Platform::Windows and target.environment == Build::TargetEnvironment::WindowsGNU;
}

static bool isWindowsMSVCTargetMachine(const Build::Machine& target)
{
    return target.platform == Build::Platform::Windows and target.environment == Build::TargetEnvironment::WindowsMSVC;
}

static bool isLinuxGlibcTargetMachine(const Build::Machine& target)
{
    return target.platform == Build::Platform::Linux and target.environment == Build::TargetEnvironment::LinuxGlibc;
}

static bool isLinuxMuslTargetMachine(const Build::Machine& target)
{
    return target.platform == Build::Platform::Linux and target.environment == Build::TargetEnvironment::LinuxMusl;
}

static bool tripleConflictsWithTargetArchitecture(StringView triple, Build::Architecture::Type architecture)
{
    const bool isX86 = triple.startsWithIgnoreCaseASCII("x86_64") or triple.startsWithIgnoreCaseASCII("amd64") or
                       triple.startsWithIgnoreCaseASCII("i686") or triple.startsWithIgnoreCaseASCII("i386");
    const bool isArm = triple.startsWithIgnoreCaseASCII("aarch64") or triple.startsWithIgnoreCaseASCII("arm64");

    switch (architecture)
    {
    case Build::Architecture::Intel64:
    case Build::Architecture::Intel32: return isArm;
    case Build::Architecture::Arm64: return isX86;
    case Build::Architecture::Any:
    case Build::Architecture::Wasm: return false;
    }
    Assert::unreachable();
}

static Result validateBuildActionCombination(Build::Action::Type actionType, const BuildCLIParseContext& context,
                                             const Build::Action& action, Console& console)
{
    const bool windowsGNUTarget  = isWindowsGNUTargetMachine(action.parameters.targetMachine);
    const bool windowsMSVCTarget = isWindowsMSVCTargetMachine(action.parameters.targetMachine);
    const bool linuxGlibcTarget  = isLinuxGlibcTargetMachine(action.parameters.targetMachine);
    const bool linuxMuslTarget   = isLinuxMuslTargetMachine(action.parameters.targetMachine);
    if (windowsGNUTarget)
    {
        if (not context.generator.isEmpty())
        {
            StringView resolvedGenerator;
            SC_TRY(resolveBuildGeneratorKeyword(context.generator, resolvedGenerator, console));
            if (not(resolvedGenerator.equalsIgnoreCaseASCII("default") or
                    resolvedGenerator.equalsIgnoreCaseASCII("native")))
            {
                return printBuildActionCombinationError(
                    console, "Windows GNU target profiles require --generator native (or default)");
            }
        }

        if (not context.architecture.isEmpty())
        {
            StringView resolvedArchitecture;
            SC_TRY(resolveBuildArchitectureKeyword(context.architecture, resolvedArchitecture, console));
            if ((action.parameters.targetMachine.architecture == Build::Architecture::Intel64 and
                 not resolvedArchitecture.equalsIgnoreCaseASCII("intel64")) or
                (action.parameters.targetMachine.architecture == Build::Architecture::Arm64 and
                 not resolvedArchitecture.equalsIgnoreCaseASCII("arm64")))
            {
                return printBuildActionCombinationError(
                    console, "Windows GNU target profiles require --arch to match the selected target profile");
            }
        }

        if (not context.targetTriple.isEmpty())
        {
            const StringView triple(context.targetTriple);
            if (not triple.containsStringIgnoreCaseASCII("windows") or not triple.containsStringIgnoreCaseASCII("gnu"))
            {
                return printBuildActionCombinationError(
                    console, "Windows GNU target profiles require a Windows GNU --triple override");
            }
            if (tripleConflictsWithTargetArchitecture(triple, action.parameters.targetMachine.architecture))
            {
                return printBuildActionCombinationError(
                    console, "The selected --triple conflicts with the architecture implied by --target");
            }
        }
    }
    if (windowsMSVCTarget)
    {
        if (not context.generator.isEmpty())
        {
            StringView resolvedGenerator;
            SC_TRY(resolveBuildGeneratorKeyword(context.generator, resolvedGenerator, console));
            if (not(resolvedGenerator.equalsIgnoreCaseASCII("default") or
                    resolvedGenerator.equalsIgnoreCaseASCII("native")))
            {
                return printBuildActionCombinationError(
                    console, "Windows MSVC target profiles require --generator native (or default)");
            }
        }

        if (not context.architecture.isEmpty())
        {
            StringView resolvedArchitecture;
            SC_TRY(resolveBuildArchitectureKeyword(context.architecture, resolvedArchitecture, console));
            if ((action.parameters.targetMachine.architecture == Build::Architecture::Intel64 and
                 not resolvedArchitecture.equalsIgnoreCaseASCII("intel64")) or
                (action.parameters.targetMachine.architecture == Build::Architecture::Arm64 and
                 not resolvedArchitecture.equalsIgnoreCaseASCII("arm64")))
            {
                return printBuildActionCombinationError(
                    console, "Windows MSVC target profiles require --arch to match the selected target profile");
            }
        }

        if (not context.targetTriple.isEmpty())
        {
            return printBuildActionCombinationError(
                console, "Windows MSVC target profiles do not accept --triple overrides yet");
        }
        if (not context.sysroot.isEmpty())
        {
            return printBuildActionCombinationError(
                console, "Windows MSVC target profiles do not accept --sysroot overrides yet");
        }
    }
    if (linuxGlibcTarget or linuxMuslTarget)
    {
        if (not context.generator.isEmpty())
        {
            StringView resolvedGenerator;
            SC_TRY(resolveBuildGeneratorKeyword(context.generator, resolvedGenerator, console));
            if (not(resolvedGenerator.equalsIgnoreCaseASCII("default") or
                    resolvedGenerator.equalsIgnoreCaseASCII("native")))
            {
                return printBuildActionCombinationError(
                    console, "Linux target profiles require --generator native (or default)");
            }
        }

        if (not context.architecture.isEmpty())
        {
            StringView resolvedArchitecture;
            SC_TRY(resolveBuildArchitectureKeyword(context.architecture, resolvedArchitecture, console));
            if ((action.parameters.targetMachine.architecture == Build::Architecture::Intel64 and
                 not resolvedArchitecture.equalsIgnoreCaseASCII("intel64")) or
                (action.parameters.targetMachine.architecture == Build::Architecture::Arm64 and
                 not resolvedArchitecture.equalsIgnoreCaseASCII("arm64")))
            {
                return printBuildActionCombinationError(
                    console, "Linux target profiles require --arch to match the selected target profile");
            }
        }

        if (not context.targetTriple.isEmpty())
        {
            const StringView triple(context.targetTriple);
            if (not triple.containsStringIgnoreCaseASCII("linux"))
            {
                return printBuildActionCombinationError(console,
                                                        "Linux target profiles require a Linux --triple override");
            }
            if (linuxMuslTarget and not triple.containsStringIgnoreCaseASCII("musl"))
            {
                return printBuildActionCombinationError(console,
                                                        "Linux musl target profiles require a musl --triple override");
            }
            if (linuxGlibcTarget and triple.containsStringIgnoreCaseASCII("musl"))
            {
                return printBuildActionCombinationError(
                    console, "Linux glibc target profiles require a glibc/GNU --triple override");
            }
            if (tripleConflictsWithTargetArchitecture(triple, action.parameters.targetMachine.architecture))
            {
                return printBuildActionCombinationError(
                    console, "The selected --triple conflicts with the architecture implied by --target");
            }
        }
    }
    if (action.parameters.toolchain.family == Build::Toolchain::FilC)
    {
        if (action.parameters.hostMachine.platform != Build::Platform::Linux)
        {
            return printBuildActionCombinationError(console, "Fil-C is only supported on Linux hosts");
        }
        if (not context.generator.isEmpty())
        {
            StringView resolvedGenerator;
            SC_TRY(resolveBuildGeneratorKeyword(context.generator, resolvedGenerator, console));
            if (not(resolvedGenerator.equalsIgnoreCaseASCII("default") or
                    resolvedGenerator.equalsIgnoreCaseASCII("native")))
            {
                return printBuildActionCombinationError(console, "Fil-C requires --generator native (or default)");
            }
        }
        if (action.parameters.targetMachine.platform != Build::Platform::Linux or
            action.parameters.targetMachine.environment != Build::TargetEnvironment::Native)
        {
            return printBuildActionCombinationError(console, "Fil-C currently only supports native Linux targets");
        }
        if (action.parameters.targetMachine.architecture != Build::Architecture::Intel64)
        {
            return printBuildActionCombinationError(
                console, "Fil-C currently only supports x86_64 Linux output in the packaged pizfix distribution");
        }
        if (not context.targetTriple.isEmpty())
        {
            return printBuildActionCombinationError(console, "Fil-C does not accept --triple overrides yet");
        }
        if (not context.sysroot.isEmpty())
        {
            return printBuildActionCombinationError(console, "Fil-C does not accept --sysroot overrides yet");
        }
    }

    if (actionType != Build::Action::Run)
    {
        return Result(true);
    }

    switch (action.parameters.runner.type)
    {
    case Build::RunnerSpec::Auto: break;
    case Build::RunnerSpec::None:
        if (not targetMachineCanRunDirectly(action.parameters.hostMachine, action.parameters.targetMachine))
        {
            return printBuildActionCombinationError(
                console, "--runner none cannot execute foreign targets directly; use compile or a supported runner");
        }
        break;
    case Build::RunnerSpec::Wine:
        if (not(windowsGNUTarget or windowsMSVCTarget))
        {
            return printBuildActionCombinationError(console, "Wine runner requires a Windows target");
        }
        break;
    case Build::RunnerSpec::QEMU:
        if (not(linuxGlibcTarget or linuxMuslTarget))
        {
            return printBuildActionCombinationError(console, "QEMU runner requires a Linux target");
        }
        break;
    case Build::RunnerSpec::Custom:
        if (action.parameters.runner.executable.isEmpty())
        {
            return printBuildActionCombinationError(console, "Custom runner requires --runner-path");
        }
        break;
    }
    return Result(true);
}

static Result scanNamedOutputMode(Span<const StringView> arguments, Build::OutputMode::Type& outputMode,
                                  bool& wasProvided, Console& console)
{
    wasProvided = false;
    for (size_t idx = 0; idx < arguments.sizeInElements(); ++idx)
    {
        const StringView argument = arguments[idx];
        if (isLongOptionToken(argument))
        {
            StringView nameAndMaybeValue = argument.sliceStart(2);
            StringView longName          = nameAndMaybeValue;
            StringView inlineValue;
            if (nameAndMaybeValue.splitBefore("=", longName))
            {
                SC_TRY(nameAndMaybeValue.splitAfter("=", inlineValue));
            }

            if (longName.equalsIgnoreCaseASCII("quiet"))
            {
                outputMode  = Build::OutputMode::Quiet;
                wasProvided = true;
                continue;
            }
            if (longName.equalsIgnoreCaseASCII("normal"))
            {
                outputMode  = Build::OutputMode::Normal;
                wasProvided = true;
                continue;
            }
            if (longName.equalsIgnoreCaseASCII("verbose"))
            {
                outputMode  = Build::OutputMode::Verbose;
                wasProvided = true;
                continue;
            }
            if (longName.equalsIgnoreCaseASCII("output"))
            {
                StringView value = inlineValue;
                if (value.isEmpty() and idx + 1 < arguments.sizeInElements())
                {
                    value = arguments[idx + 1];
                    idx += 1;
                }
                SC_TRY(resolveOutputModeValue(value, outputMode, console));
                wasProvided = true;
            }
            continue;
        }

        if (isShortOptionToken(argument))
        {
            const StringView shortGroup = argument.sliceStart(1);
            const char*      chars      = shortGroup.bytesWithoutTerminator();
            for (size_t shortIdx = 0; shortIdx < shortGroup.sizeInBytes(); ++shortIdx)
            {
                switch (chars[shortIdx])
                {
                case 'q':
                    outputMode  = Build::OutputMode::Quiet;
                    wasProvided = true;
                    break;
                case 'v':
                    outputMode  = Build::OutputMode::Verbose;
                    wasProvided = true;
                    break;
                case 'o':
                    if (shortGroup.sizeInBytes() != 1)
                    {
                        return Result::Error("Invalid short option group");
                    }
                    SC_TRY(resolveOutputModeValue(
                        idx + 1 < arguments.sizeInElements() ? arguments[idx + 1] : StringView(), outputMode, console));
                    idx += 1;
                    wasProvided = true;
                    break;
                default: break;
                }
            }
        }
    }
    return Result(true);
}

Result prepareBuildAction(Build::Action::Type actionType, Tool::Arguments& arguments, Build::Action& action,
                          BuildCLIResolvedStorage& resolvedStorage, BuildCLIStatus& status)
{
    status        = BuildCLIStatus::Ready;
    action        = Build::Action();
    action.action = actionType;
    SC_TRY(Path::join(action.parameters.directories.projectsDirectory,
                      {arguments.toolDestination.view(), PROJECTS_SUBDIR}));
    SC_TRY(
        Path::join(action.parameters.directories.outputsDirectory, {arguments.toolDestination.view(), OUTPUTS_SUBDIR}));
    SC_TRY(Path::join(action.parameters.directories.intermediatesDirectory,
                      {arguments.toolDestination.view(), INTERMEDIATES_SUBDIR}));
    SC_TRY(Path::join(action.parameters.directories.buildCacheDirectory,
                      {arguments.toolDestination.view(), BUILD_CACHE_SUBDIR}));
    SC_TRY(Path::join(action.parameters.directories.packagesCacheDirectory,
                      {arguments.toolDestination.view(), PackagesCacheDirectory}));
    SC_TRY(Path::join(action.parameters.directories.packagesInstallDirectory,
                      {arguments.toolDestination.view(), PackagesInstallDirectory}));
    action.parameters.directories.libraryDirectory = arguments.libraryDirectory.view();
    action.parameters.directories.projectDirectory = arguments.projectDirectory.view();

    switch (HostPlatform)
    {
    case Platform::Windows:
    case Platform::Apple:
    case Platform::Linux: applyHostDefaultBuildParameters(action); break;
    default: return Result::Error("Unsupported platform for compile");
    }

    Span<const StringView> preArguments;
    Span<const StringView> postArguments;
    SC_TRY(splitBuildArgumentsAtTerminator(arguments.arguments, preArguments, postArguments));

    StringSpan argumentStorage[16];
    for (size_t idx = 0; idx < preArguments.sizeInElements(); ++idx)
    {
        argumentStorage[idx] = preArguments[idx];
    }

    BuildCLIParseContext  context;
    CommandLineOption     options[13];
    CommandLinePositional positionals[2];
    CommandLineSpec       spec;
    size_t                numOptions = 0;

    options[numOptions].longName  = "config";
    options[numOptions].shortName = 'c';
    options[numOptions].help      = "Build configuration name";
    options[numOptions].valueName = "NAME";
    options[numOptions].value     = CommandLineValue::stringSpan(context.configuration);
    numOptions++;

    options[numOptions].longName  = "target";
    options[numOptions].shortName = 't';
    options[numOptions].help =
        "Build target profile (host, native, windows-gnu-x86_64, windows-gnu-arm64, windows-msvc-x86_64, "
        "windows-msvc-arm64, linux-glibc-x86_64, linux-glibc-arm64, linux-musl-x86_64, linux-musl-arm64)";
    options[numOptions].valueName = "PROFILE";
    options[numOptions].value     = CommandLineValue::stringSpan(context.targetProfile);
    numOptions++;

    options[numOptions].longName = "toolchain";
    options[numOptions].help = "Compiler family (default, host-default, clang, filc, gcc, msvc, clang-cl, llvm-mingw)";
    options[numOptions].valueName = "NAME";
    options[numOptions].value     = CommandLineValue::stringSpan(context.toolchain);
    numOptions++;

    options[numOptions].longName  = "generator";
    options[numOptions].shortName = 'g';
    options[numOptions].help      = "Build generator (default/native, make, xcode, vs2022, vs2019)";
    options[numOptions].valueName = "NAME";
    options[numOptions].value     = CommandLineValue::stringSpan(context.generator);
    numOptions++;

    options[numOptions].longName  = "arch";
    options[numOptions].shortName = 'a';
    options[numOptions].help      = "Build architecture (arm64, intel64, intel32, wasm, any)";
    options[numOptions].valueName = "NAME";
    options[numOptions].value     = CommandLineValue::stringSpan(context.architecture);
    numOptions++;

    options[numOptions].longName  = "triple";
    options[numOptions].help      = "Override the compiler target triple";
    options[numOptions].valueName = "VALUE";
    options[numOptions].value     = CommandLineValue::stringSpan(context.targetTriple);
    numOptions++;

    options[numOptions].longName  = "sysroot";
    options[numOptions].help      = "Override the toolchain sysroot";
    options[numOptions].valueName = "PATH";
    options[numOptions].value     = CommandLineValue::stringSpan(context.sysroot);
    numOptions++;

    if (actionType == Build::Action::Compile or actionType == Build::Action::Run)
    {
        options[numOptions].longName  = "output";
        options[numOptions].shortName = 'o';
        options[numOptions].help      = "Output mode (quiet, normal, verbose)";
        options[numOptions].valueName = "MODE";
        options[numOptions].value     = CommandLineValue::stringSpan(context.output);
        numOptions++;

        options[numOptions].longName  = "quiet";
        options[numOptions].shortName = 'q';
        options[numOptions].help      = "Shortcut for --output quiet";
        options[numOptions].value     = CommandLineValue::boolean(context.quietRequested);
        numOptions++;

        options[numOptions].longName = "normal";
        options[numOptions].help     = "Shortcut for --output normal";
        options[numOptions].value    = CommandLineValue::boolean(context.normalRequested);
        numOptions++;

        options[numOptions].longName  = "verbose";
        options[numOptions].shortName = 'v';
        options[numOptions].help      = "Shortcut for --output verbose";
        options[numOptions].value     = CommandLineValue::boolean(context.verboseRequested);
        numOptions++;
    }

    if (actionType == Build::Action::Run)
    {
        options[numOptions].longName  = "runner";
        options[numOptions].help      = "Execution runner (auto, none, wine, qemu, custom)";
        options[numOptions].valueName = "NAME";
        options[numOptions].value     = CommandLineValue::stringSpan(context.runner);
        numOptions++;

        options[numOptions].longName  = "runner-path";
        options[numOptions].help      = "Override the runner executable path";
        options[numOptions].valueName = "PATH";
        options[numOptions].value     = CommandLineValue::stringSpan(context.runnerPath);
        numOptions++;
    }

    positionals[0].name     = "target";
    positionals[0].help     = "Optional workspace:project or project name";
    positionals[0].required = false;
    positionals[0].value    = CommandLineValue::stringSpan(context.target);

    positionals[1].name             = "legacy";
    positionals[1].help             = "Legacy positional values";
    positionals[1].required         = false;
    positionals[1].remaining        = true;
    positionals[1].remainingStorage = context.legacyStorage;
    positionals[1].parsedValues     = &context.legacyValues;

    spec.programName = actionType == Build::Action::Compile ? "./SC.sh build compile"_a8
                       : actionType == Build::Action::Run   ? "./SC.sh build run"_a8
                                                            : "./SC.sh build coverage"_a8;
    spec.summary     = actionType == Build::Action::Compile ? "Compile one target or the default workspace."_a8
                       : actionType == Build::Action::Run ? "Build a target if needed and run the resulting executable."_a8
                                                          : "Build coverage output for a target."_a8;
    spec.options     = {options, numOptions};
    spec.positionals = positionals;

    const auto parseResult = spec.parse({argumentStorage, preArguments.sizeInElements()});
    if (parseResult.status == CommandLineParseResult::Status::HelpRequested)
    {
        SC_TRY(printBuildActionHelp(spec, actionType, arguments.console));
        status = BuildCLIStatus::HelpRequested;
        return Result(true);
    }
    if (parseResult.status == CommandLineParseResult::Status::Error)
    {
        SC_TRY(printBuildActionParseError(spec, parseResult, arguments.console));
        status = BuildCLIStatus::Error;
        return Result::Error("Invalid SC-build arguments");
    }

    if (actionType != Build::Action::Run and postArguments.sizeInElements() > 0)
    {
        arguments.console.printError("Arguments after -- are only supported by \"build run\".\n");
        arguments.console.flushStdErr();
        status = BuildCLIStatus::Error;
        return Result::Error("Unexpected arguments after --");
    }
    action.additionalArguments = postArguments;

    SC_TRY(setBuildActionTarget(action, context.target));

    if (context.legacyValues.sizeInElements() >= 1)
    {
        SC_TRY(applyConfigurationValue(action, context.legacyValues[0], resolvedStorage, arguments.console));
    }
    if (context.legacyValues.sizeInElements() >= 2)
    {
        SC_TRY(applyGeneratorValue(action, context.legacyValues[1], arguments.console));
    }
    if (context.legacyValues.sizeInElements() >= 3)
    {
        SC_TRY(applyArchitectureValue(action, context.legacyValues[2], arguments.console));
    }
    if (context.legacyValues.sizeInElements() >= 4 and
        (actionType == Build::Action::Compile or actionType == Build::Action::Run))
    {
        Build::OutputMode::Type legacyOutputMode = Build::OutputMode::Normal;
        SC_TRY(resolveOutputModeValue(context.legacyValues[3], legacyOutputMode, arguments.console));
        action.parameters.execution.outputMode = legacyOutputMode;
    }

    if (not context.configuration.isEmpty())
    {
        SC_TRY(applyConfigurationValue(action, context.configuration, resolvedStorage, arguments.console));
    }
    if (not context.generator.isEmpty())
    {
        SC_TRY(applyGeneratorValue(action, context.generator, arguments.console));
    }
    if (not context.architecture.isEmpty())
    {
        SC_TRY(applyArchitectureValue(action, context.architecture, arguments.console));
    }
    if (not context.targetProfile.isEmpty())
    {
        SC_TRY(applyTargetProfileValue(action, context.targetProfile, arguments.console));
    }
    if (not context.toolchain.isEmpty())
    {
        SC_TRY(applyToolchainValue(action, context.toolchain, arguments.console));
    }
    if (not context.targetTriple.isEmpty())
    {
        SC_TRY(action.parameters.toolchain.targetTriple.assign(context.targetTriple));
    }
    if (not context.sysroot.isEmpty())
    {
        SC_TRY(action.parameters.toolchain.sysroot.assign(context.sysroot));
    }
    if (not context.runner.isEmpty())
    {
        SC_TRY(applyRunnerValue(action, context.runner, arguments.console));
    }
    if (not context.runnerPath.isEmpty())
    {
        SC_TRY(action.parameters.runner.executable.assign(context.runnerPath));
    }
    SC_TRY(validateBuildActionCombination(actionType, context, action, arguments.console));

    if (actionType == Build::Action::Compile or actionType == Build::Action::Run)
    {
        Build::OutputMode::Type namedOutputMode = Build::OutputMode::Normal;
        bool                    namedOutputSet  = false;
        SC_TRY(scanNamedOutputMode(preArguments, namedOutputMode, namedOutputSet, arguments.console));
        if (namedOutputSet)
        {
            action.parameters.execution.outputMode = namedOutputMode;
        }
    }

    return Result(true);
}
} // namespace detail

static Result runBuildValidate(Tool::Arguments& arguments, Build::Directories& directories)
{
    SC_TRY(Path::join(directories.projectsDirectory, {arguments.toolDestination.view(), detail::PROJECTS_SUBDIR}));
    SC_TRY(Path::join(directories.outputsDirectory, {arguments.toolDestination.view(), detail::OUTPUTS_SUBDIR}));
    SC_TRY(Path::join(directories.intermediatesDirectory,
                      {arguments.toolDestination.view(), detail::INTERMEDIATES_SUBDIR}));
    SC_TRY(Path::join(directories.buildCacheDirectory, {arguments.toolDestination.view(), detail::BUILD_CACHE_SUBDIR}));
    SC_TRY(Path::join(directories.packagesCacheDirectory, {arguments.toolDestination.view(), PackagesCacheDirectory}));
    SC_TRY(
        Path::join(directories.packagesInstallDirectory, {arguments.toolDestination.view(), PackagesInstallDirectory}));
    directories.libraryDirectory = arguments.libraryDirectory.view();
    directories.projectDirectory = arguments.projectDirectory.view();

    SmallStringNative<256> buffer;

    Console& console = arguments.console;
    auto     builder = StringBuilder::create(buffer);
    SC_TRY(builder.append("projects         = \"{}\"\n", directories.projectsDirectory));
    SC_TRY(builder.append("outputs          = \"{}\"\n", directories.outputsDirectory));
    SC_TRY(builder.append("intermediates    = \"{}\"\n", directories.intermediatesDirectory));
    SC_TRY(builder.append("build-cache      = \"{}\"\n", directories.buildCacheDirectory));
    builder.finalize();
    console.print(buffer.view());
    if (not Path::isAbsolute(directories.projectsDirectory.view(), SC::Path::AsNative) or
        not Path::isAbsolute(arguments.libraryDirectory.view(), SC::Path::AsNative) or
        not Path::isAbsolute(arguments.projectDirectory.view(), SC::Path::AsNative))
    {
        return Result::Error(
            "The build output directory, the libraries directory, and the project directory must be absolute paths");
    }
    return Result(true);
}

static Result runBuildConfigure(Tool::Arguments& arguments)
{
    Build::Action action;
    SC_TRY(runBuildValidate(arguments, action.parameters.directories));
    action.action = Build::Action::Configure;

    action.parameters.directories.libraryDirectory = arguments.libraryDirectory.view();
    action.parameters.directories.projectDirectory = arguments.projectDirectory.view();
    if (arguments.arguments.sizeInElements() >= 1)
    {
        if (arguments.arguments[0].splitBefore(SC_NATIVE_STR(":"), action.workspaceName))
        {
            SC_TRUST_RESULT(arguments.arguments[0].splitAfter(SC_NATIVE_STR(":"), action.projectName));
        }
        else
        {
            action.projectName = arguments.arguments[0];
        }
    }
    action.parameters.generator = Build::Generator::VisualStudio2019;
    action.parameters.platform  = Build::Platform::Windows;
    arguments.console.print("Executing \"{}\" for Visual Studio 2019 on Windows\n", arguments.action);
    SC_TRY(Build::executeAction(action));
    action.parameters.generator = Build::Generator::VisualStudio2022;
    action.parameters.platform  = Build::Platform::Windows;
    arguments.console.print("Executing \"{}\" for Visual Studio 2022 on Windows\n", arguments.action);
    SC_TRY(Build::executeAction(action));
    action.parameters.generator = Build::Generator::XCode;
    action.parameters.platform  = Build::Platform::Apple;
    arguments.console.print("Executing \"{}\" for XCode on Apple platform\n", arguments.action);
    SC_TRY(Build::executeAction(action));
    action.parameters.generator = Build::Generator::Make;
    action.parameters.platform  = Build::Platform::Linux;
    arguments.console.print("Executing \"{}\" for Make on Linux platform\n", arguments.action);
    SC_TRY(Build::executeAction(action));
    action.parameters.generator = Build::Generator::Make;
    action.parameters.platform  = Build::Platform::Apple;
    arguments.console.print("Executing \"{}\" for Make on Apple platform\n", arguments.action);
    SC_TRY(Build::executeAction(action));
    action.parameters.generator = Build::Generator::Native;
    action.parameters.platform  = Build::Platform::Windows;
    arguments.console.print("Executing \"{}\" for Native on Windows platform\n", arguments.action);
    SC_TRY(Build::executeAction(action));
    action.parameters.generator = Build::Generator::Native;
    action.parameters.platform  = Build::Platform::Linux;
    arguments.console.print("Executing \"{}\" for Native on Linux platform\n", arguments.action);
    SC_TRY(Build::executeAction(action));
    action.parameters.generator = Build::Generator::Native;
    action.parameters.platform  = Build::Platform::Apple;
    arguments.console.print("Executing \"{}\" for Native on Apple platform\n", arguments.action);
    SC_TRY(Build::executeAction(action));
    return Result(true);
}

static Result runBuildAction(Build::Action::Type actionType, Tool::Arguments& arguments)
{
    Build::Action                   action;
    detail::BuildCLIResolvedStorage resolvedStorage;
    detail::BuildCLIStatus          status = detail::BuildCLIStatus::Ready;
    SC_TRY(detail::prepareBuildAction(actionType, arguments, action, resolvedStorage, status));
    if (status == detail::BuildCLIStatus::HelpRequested)
    {
        return Result(true);
    }
    if (status == detail::BuildCLIStatus::Error)
    {
        return Result::Error("Invalid SC-build arguments");
    }

    SC_TRY(runBuildValidate(arguments, action.parameters.directories));
    action.parameters.directories.libraryDirectory = arguments.libraryDirectory.view();
    action.parameters.directories.projectDirectory = arguments.projectDirectory.view();

    return Build::executeAction(action);
}

static Result runBuildDocumentation(StringView doxygenExecutable, Tool::Arguments& arguments)
{
    String outputDirectory;
    SC_TRY(Path::join(outputDirectory, {arguments.toolDestination.view(), "_Documentation"}));
    {
        FileSystem fs;
        if (fs.init(outputDirectory.view()))
        {
            SC_TRY(fs.removeDirectoryRecursive(outputDirectory.view()));
        }
    }
    String documentationDirectory;
    SC_TRY(Path::join(documentationDirectory, {arguments.libraryDirectory.view(), "Documentation", "Doxygen"}));

    Process process;
    SC_TRY(process.setWorkingDirectory(documentationDirectory.view()));
    SC_TRY(process.setEnvironment("STRIP_FROM_PATH", documentationDirectory.view()));
    switch (HostPlatform)
    {
    case Platform::Apple: SC_TRY(process.setEnvironment("PACKAGES_PLATFORM", "macos")); break;
    case Platform::Linux: SC_TRY(process.setEnvironment("PACKAGES_PLATFORM", "linux")); break;
    case Platform::Windows: SC_TRY(process.setEnvironment("PACKAGES_PLATFORM", "windows")); break;
    case Platform::Emscripten: return Result::Error("Unsupported platform");
    }
    SC_TRY(process.exec({doxygenExecutable}));
    SC_TRY_MSG(process.getExitStatus() == 0, "Build documentation failed");

    SC_TRY(Path::join(outputDirectory, {arguments.toolDestination.view(), "_Documentation", "docs"}));
    {
        FileSystem fs;
        SC_TRY(fs.init(outputDirectory.view()));
        SC_TRY(fs.writeString(".nojekyll", ""));
    }
    return Result(true);
}

Result runBuildTool(Tool::Arguments& arguments)
{
    if (arguments.action == "configure")
    {
        return runBuildConfigure(arguments);
    }
    else if (arguments.action == "compile")
    {
        return runBuildAction(Build::Action::Compile, arguments);
    }
    else if (arguments.action == "run")
    {
        return runBuildAction(Build::Action::Run, arguments);
    }
    else if (arguments.action == "coverage")
    {
        return runBuildAction(Build::Action::Coverage, arguments);
    }
#if SC_XCTEST
#else
    else if (arguments.action == "documentation")
    {
        StringView      additionalArgs[1];
        Tool::Arguments args = arguments;
        args.tool            = "packages";
        args.action          = "install";
        args.arguments       = {additionalArgs};
        Tools::Package doxygenPackage;
        additionalArgs[0] = "doxygen";
        SC_TRY(runPackageTool(args, &doxygenPackage));
        Tools::Package doxygenAwesomeCssPackage;
        additionalArgs[0] = "doxygen-awesome-css";
        SC_TRY(runPackageTool(args, &doxygenAwesomeCssPackage));
        String doxygenExecutable;
        SC_TRY(StringBuilder::format(doxygenExecutable, "{}/doxygen", doxygenPackage.installDirectoryLink));
        return runBuildDocumentation(doxygenExecutable.view(), arguments);
    }
#endif
    else
    {
        return Result::Error("SC-build unknown action (supported \"configure\", \"compile\", \"run\", \"coverage\", or "
                             "\"documentation\")");
    }
}
} // namespace Tools
namespace Build
{
Result executeAction(const Action& action) { return Build::Action::execute(action, configure); }
} // namespace Build

#if !defined(SC_TOOLS_COMPILED_SEPARATELY) && !defined(SC_TOOLS_IMPORT)
StringView Tools::Tool::getToolName() { return "SC-build"; }
StringView Tools::Tool::getDefaultAction() { return "compile"; }
Result     Tools::Tool::runTool(Tools::Tool::Arguments& arguments) { return Tools::runBuildTool(arguments); }
#endif
} // namespace SC
