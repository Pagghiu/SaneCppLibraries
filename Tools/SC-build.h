// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Libraries/Process/Process.h"
#include "../Libraries/Strings/CommandLine.h"
#include "../Libraries/Strings/Console.h"
#include "../Libraries/Strings/Path.h"
#include "../Libraries/Strings/StringBuilder.h"
#include "../Libraries/Strings/StringFormat.h"
#include "SC-build/Build.h"
#include "Tools.h"

#include "SC-package.h"

#if !defined(SC_TOOLS_COMPILED_SEPARATELY)
#define SC_TOOLS_IMPORT
#include "SC-package.cpp"
#undef SC_TOOLS_IMPORT
#endif

namespace SC
{
namespace Build
{
Result configure(Definition& definition, const Parameters& parameters);
}
} // namespace SC

namespace SC
{
namespace Tools
{

constexpr StringView PROJECTS_SUBDIR      = "_Projects";
constexpr StringView OUTPUTS_SUBDIR       = "_Outputs";
constexpr StringView INTERMEDIATES_SUBDIR = "_Intermediates";
constexpr StringView BUILD_CACHE_SUBDIR   = "_BuildCache";
constexpr StringView DEFAULT_WORKSPACE    = "SCWorkspace";

enum class BuildCLIStatus : uint8_t
{
    Ready,
    HelpRequested,
    Error,
};

struct BuildCLIResolvedStorage
{
    String configurationName = StringEncoding::Ascii;
};

struct BuildCLIParseContext
{
    StringSpan target           = {};
    StringSpan targetProfile    = {};
    StringSpan configuration    = {};
    StringSpan generator        = {};
    StringSpan architecture     = {};
    StringSpan targetTriple     = {};
    StringSpan sysroot          = {};
    StringSpan runner           = {};
    StringSpan runnerPath       = {};
    StringSpan output           = {};
    bool       quietRequested   = false;
    bool       normalRequested  = false;
    bool       verboseRequested = false;

    StringSpan       legacyStorage[4];
    Span<StringSpan> legacyValues = {};
};

[[nodiscard]] inline char asciiToLower(char c)
{
    return (c >= 'A' and c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

[[nodiscard]] inline uint32_t asciiToLowerCodePoint(uint32_t c)
{
    return (c >= 'A' and c <= 'Z') ? static_cast<uint32_t>(c - 'A' + 'a') : c;
}

[[nodiscard]] inline bool equalsAsciiIgnoreCase(StringView lhs, StringView rhs)
{
    return StringView::withIterators(
        lhs, rhs,
        [](auto lhsIterator, auto rhsIterator)
        {
            uint32_t lhsCodePoint = 0;
            uint32_t rhsCodePoint = 0;
            while (lhsIterator.advanceRead(lhsCodePoint) and rhsIterator.advanceRead(rhsCodePoint))
            {
                if (asciiToLowerCodePoint(lhsCodePoint) != asciiToLowerCodePoint(rhsCodePoint))
                {
                    return false;
                }
            }
            return lhsIterator.isAtEnd() and rhsIterator.isAtEnd();
        });
}

[[nodiscard]] inline bool startsWithAsciiIgnoreCase(StringView value, StringView prefix)
{
    return StringView::withIterators(value, prefix,
                                     [](auto valueIterator, auto prefixIterator)
                                     {
                                         uint32_t valueCodePoint  = 0;
                                         uint32_t prefixCodePoint = 0;
                                         while (prefixIterator.advanceRead(prefixCodePoint))
                                         {
                                             if (not valueIterator.advanceRead(valueCodePoint))
                                             {
                                                 return false;
                                             }
                                             if (asciiToLowerCodePoint(valueCodePoint) !=
                                                 asciiToLowerCodePoint(prefixCodePoint))
                                             {
                                                 return false;
                                             }
                                         }
                                         return true;
                                     });
}

[[nodiscard]] inline bool containsAsciiIgnoreCase(StringView value, StringView needle)
{
    if (needle.isEmpty())
    {
        return true;
    }
    if (needle.sizeInBytes() > value.sizeInBytes())
    {
        return false;
    }

    const char* valueBytes  = value.bytesWithoutTerminator();
    const char* needleBytes = needle.bytesWithoutTerminator();
    for (size_t valueIndex = 0; valueIndex + needle.sizeInBytes() <= value.sizeInBytes(); ++valueIndex)
    {
        bool match = true;
        for (size_t needleIndex = 0; needleIndex < needle.sizeInBytes(); ++needleIndex)
        {
            if (asciiToLower(valueBytes[valueIndex + needleIndex]) != asciiToLower(needleBytes[needleIndex]))
            {
                match = false;
                break;
            }
        }
        if (match)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] inline bool isShortOptionToken(StringView token)
{
    return token.sizeInBytes() >= 2 and token.bytesWithoutTerminator()[0] == '-' and
           token.bytesWithoutTerminator()[1] != '-';
}

[[nodiscard]] inline bool isLongOptionToken(StringView token)
{
    return token.sizeInBytes() >= 3 and token.bytesWithoutTerminator()[0] == '-' and
           token.bytesWithoutTerminator()[1] == '-';
}

[[nodiscard]] inline Result appendBuildActionHelpAddendum(StringFormatOutput& output, Build::Action::Type actionType)
{
    if (actionType == Build::Action::Compile or actionType == Build::Action::Run)
    {
        SC_TRY_MSG(output.append("\nTarget profiles:\n"
                                 "  - host / native: build for the current host machine\n"
                                 "  - windows-gnu-x86_64: Windows GNU target through llvm-mingw\n"
                                 "  - windows-msvc-x86_64: Windows MSVC target through portable MSVC + Wine\n"
                                 "  - windows-msvc-arm64: Windows MSVC arm64 target through portable MSVC + Wine\n"
                                 "  - windows-gnu-arm64: Windows GNU arm64 target through llvm-mingw\n"),
                   "Failed writing SC-build help");
        SC_TRY_MSG(
            output.append(
                "\nCurrent tested cross-target support:\n"
                "  - macOS and Linux hosts can compile windows-gnu-x86_64 and windows-gnu-arm64\n"
                "  - macOS hosts can compile windows-msvc-x86_64 and windows-msvc-arm64 through portable MSVC + Wine\n"
                "  - build run can auto-route x86_64 Windows targets through Wine on macOS and Linux\n"
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
                                 "  - qemu: reserved for future Linux-target runner support\n"
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

[[nodiscard]] inline Result printBuildActionHelp(const CommandLineSpec& spec, Build::Action::Type actionType,
                                                 Console& console)
{
    StringFormatOutput output(StringEncoding::Utf8, console, true);
    SC_TRY_MSG(spec.writeHelp(output), "Failed writing SC-build help");
    SC_TRY(appendBuildActionHelpAddendum(output, actionType));
    console.flush();
    return Result(true);
}

[[nodiscard]] inline Result printBuildActionParseError(const CommandLineSpec&        spec,
                                                       const CommandLineParseResult& parseResult, Console& console)
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

[[nodiscard]] inline Result printBuildActionValueError(Console& console, StringView optionName, StringView value,
                                                       StringView message)
{
    console.printError("{} {}: {}\n", message, optionName, value);
    console.flushStdErr();
    return Result::Error("Invalid SC-build option value");
}

[[nodiscard]] inline Result printBuildActionAmbiguity(Console& console, StringView optionName, StringView value,
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

[[nodiscard]] inline Result splitBuildArgumentsAtTerminator(Span<const StringView>  arguments,
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

inline void applyHostDefaultBuildParameters(Build::Action& action)
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
        action.parameters.generator = Build::Generator::VisualStudio2022;
        action.parameters.platform  = Build::Platform::Windows;
        break;
    case Platform::Apple:
        action.parameters.generator = Build::Generator::Make;
        action.parameters.platform  = Build::Platform::Apple;
        break;
    case Platform::Linux:
        action.parameters.generator = Build::Generator::Make;
        action.parameters.platform  = Build::Platform::Linux;
        break;
    default: break;
    }
    action.parameters.targetMachine.platform     = action.parameters.platform;
    action.parameters.targetMachine.architecture = action.parameters.architecture;
    action.parameters.targetMachine.environment  = Build::TargetEnvironment::Native;
}

[[nodiscard]] inline Result applyTargetProfileValue(Build::Action& action, StringView targetProfile, Console& console)
{
    if (targetProfile.isEmpty())
    {
        return Result(true);
    }

    StringView resolved = targetProfile;
    if (equalsAsciiIgnoreCase(targetProfile, "h"))
    {
        resolved = "host";
    }
    else if (equalsAsciiIgnoreCase(targetProfile, "n"))
    {
        resolved = "native";
    }
    else if (equalsAsciiIgnoreCase(targetProfile, "windows-gnu-x64"))
    {
        resolved = "windows-gnu-x86_64";
    }
    else if (equalsAsciiIgnoreCase(targetProfile, "windows-gnu-aarch64"))
    {
        resolved = "windows-gnu-arm64";
    }
    else if (equalsAsciiIgnoreCase(targetProfile, "windows-msvc-x64"))
    {
        resolved = "windows-msvc-x86_64";
    }
    else if (equalsAsciiIgnoreCase(targetProfile, "windows-msvc-aarch64"))
    {
        resolved = "windows-msvc-arm64";
    }
    else if (not(equalsAsciiIgnoreCase(targetProfile, "host") or equalsAsciiIgnoreCase(targetProfile, "native") or
                 equalsAsciiIgnoreCase(targetProfile, "windows-msvc-x86_64") or
                 equalsAsciiIgnoreCase(targetProfile, "windows-msvc-arm64") or
                 equalsAsciiIgnoreCase(targetProfile, "windows-gnu-x86_64") or
                 equalsAsciiIgnoreCase(targetProfile, "windows-gnu-arm64")))
    {
        return printBuildActionValueError(console, "--target", targetProfile, "Unknown value for");
    }

    if (equalsAsciiIgnoreCase(resolved, "host") or equalsAsciiIgnoreCase(resolved, "native"))
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

    if (equalsAsciiIgnoreCase(resolved, "windows-msvc-x86_64") or equalsAsciiIgnoreCase(resolved, "windows-msvc-arm64"))
    {
        action.parameters.targetMachine.environment = Build::TargetEnvironment::WindowsMSVC;
        if (equalsAsciiIgnoreCase(resolved, "windows-msvc-arm64"))
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

    if (equalsAsciiIgnoreCase(resolved, "windows-gnu-arm64"))
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

[[nodiscard]] inline Result setBuildActionTarget(Build::Action& action, StringView target)
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

template <typename Configurations>
[[nodiscard]] inline Result addUniqueConfigName(const Configurations&        configurations,
                                                SmallVector<StringView, 16>& names)
{
    for (const auto& configuration : configurations)
    {
        const StringView name = configuration.name.view();
        if (not names.contains(name))
        {
            SC_TRY(names.push_back(name));
        }
    }
    return Result(true);
}

[[nodiscard]] inline Result collectBuildConfigurations(const Build::Action& action, SmallVector<StringView, 16>& names)
{
    Build::Definition definition;
    SC_TRY(Build::configure(definition, action.parameters));

    const StringView workspaceName = action.workspaceName.isEmpty() ? DEFAULT_WORKSPACE : action.workspaceName;

    size_t workspaceIndex = 0;
    if (definition.workspaces.find([&](const Build::Workspace& workspace) { return workspace.name == workspaceName; },
                                   &workspaceIndex))
    {
        const Build::Workspace& workspace = definition.workspaces[workspaceIndex];
        if (action.projectName.isEmpty())
        {
            for (const auto& project : workspace.projects)
            {
                SC_TRY(addUniqueConfigName(project.configurations, names));
            }
        }
        else
        {
            size_t projectIndex = 0;
            if (workspace.projects.find([&](const Build::Project& project)
                                        { return project.name == action.projectName; }, &projectIndex))
            {
                SC_TRY(addUniqueConfigName(workspace.projects[projectIndex].configurations, names));
            }
        }
    }

    if (names.isEmpty())
    {
        for (const auto& workspace : definition.workspaces)
        {
            for (const auto& project : workspace.projects)
            {
                SC_TRY(addUniqueConfigName(project.configurations, names));
            }
        }
    }

    if (names.isEmpty())
    {
        SC_TRY(names.push_back("Debug"));
        SC_TRY(names.push_back("Release"));
    }
    return Result(true);
}

template <size_t N>
[[nodiscard]] inline Result resolveKeywordValue(StringView optionName, StringView              input,
                                                const StringView (&candidates)[N], StringView& resolved,
                                                Console& console)
{
    SmallVector<StringView, N> matches;

    for (const auto& candidate : candidates)
    {
        if (equalsAsciiIgnoreCase(candidate, input))
        {
            resolved = candidate;
            return Result(true);
        }
    }

    for (const auto& candidate : candidates)
    {
        if (startsWithAsciiIgnoreCase(candidate, input))
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

[[nodiscard]] inline Result resolveConfigurationValue(StringView input, BuildCLIResolvedStorage& storage,
                                                      StringView& resolved)
{
    if (equalsAsciiIgnoreCase(input, "d"))
    {
        SC_TRY(storage.configurationName.assign("Debug"));
        resolved = storage.configurationName.view();
        return Result(true);
    }
    if (equalsAsciiIgnoreCase(input, "r"))
    {
        SC_TRY(storage.configurationName.assign("Release"));
        resolved = storage.configurationName.view();
        return Result(true);
    }
    if (equalsAsciiIgnoreCase(input, "dc"))
    {
        SC_TRY(storage.configurationName.assign("DebugCoverage"));
        resolved = storage.configurationName.view();
        return Result(true);
    }
    if (equalsAsciiIgnoreCase(input, "dv"))
    {
        SC_TRY(storage.configurationName.assign("DebugValgrind"));
        resolved = storage.configurationName.view();
        return Result(true);
    }

    SC_TRY(storage.configurationName.assign(input));
    resolved = storage.configurationName.view();
    return Result(true);
}

[[nodiscard]] inline Result applyConfigurationValue(Build::Action& action, StringView configurationValue,
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

[[nodiscard]] inline Result applyGeneratorValue(Build::Action& action, StringView generatorValue, Console& console)
{
    if (generatorValue.isEmpty())
    {
        return Result(true);
    }
    static constexpr StringView generatorNames[] = {"default", "native", "make", "xcode", "vs2022", "vs2019"};
    StringView                  resolved;
    SC_TRY(resolveKeywordValue("--generator", generatorValue, generatorNames, resolved, console));
    if (equalsAsciiIgnoreCase(resolved, "native"))
    {
        action.parameters.generator = Build::Generator::Native;
    }
    else if (equalsAsciiIgnoreCase(resolved, "make"))
    {
        action.parameters.generator = Build::Generator::Make;
    }
    else if (equalsAsciiIgnoreCase(resolved, "xcode"))
    {
        action.parameters.generator = Build::Generator::XCode;
    }
    else if (equalsAsciiIgnoreCase(resolved, "vs2022"))
    {
        action.parameters.generator = Build::Generator::VisualStudio2022;
    }
    else if (equalsAsciiIgnoreCase(resolved, "vs2019"))
    {
        action.parameters.generator = Build::Generator::VisualStudio2019;
    }
    return Result(true);
}

[[nodiscard]] inline Result applyArchitectureValue(Build::Action& action, StringView architectureValue,
                                                   Console& console)
{
    if (architectureValue.isEmpty())
    {
        return Result(true);
    }
    static constexpr StringView architectureNames[] = {"arm64", "intel32", "intel64", "wasm", "any"};
    StringView                  resolved;
    SC_TRY(resolveKeywordValue("--arch", architectureValue, architectureNames, resolved, console));
    if (equalsAsciiIgnoreCase(resolved, "arm64"))
    {
        action.parameters.architecture = Build::Architecture::Arm64;
    }
    else if (equalsAsciiIgnoreCase(resolved, "intel32"))
    {
        action.parameters.architecture = Build::Architecture::Intel32;
    }
    else if (equalsAsciiIgnoreCase(resolved, "intel64"))
    {
        action.parameters.architecture = Build::Architecture::Intel64;
    }
    else if (equalsAsciiIgnoreCase(resolved, "wasm"))
    {
        action.parameters.architecture = Build::Architecture::Wasm;
    }
    else if (equalsAsciiIgnoreCase(resolved, "any"))
    {
        action.parameters.architecture = Build::Architecture::Any;
    }
    return Result(true);
}

[[nodiscard]] inline Result resolveOutputModeValue(StringView value, Build::OutputMode::Type& outputMode,
                                                   Console& console)
{
    static constexpr StringView outputNames[] = {"quiet", "normal", "verbose"};
    StringView                  resolved;
    SC_TRY(resolveKeywordValue("--output", value, outputNames, resolved, console));
    if (equalsAsciiIgnoreCase(resolved, "quiet"))
    {
        outputMode = Build::OutputMode::Quiet;
    }
    else if (equalsAsciiIgnoreCase(resolved, "normal"))
    {
        outputMode = Build::OutputMode::Normal;
    }
    else
    {
        outputMode = Build::OutputMode::Verbose;
    }
    return Result(true);
}

[[nodiscard]] inline Result applyRunnerValue(Build::Action& action, StringView runnerValue, Console& console)
{
    if (runnerValue.isEmpty())
    {
        return Result(true);
    }

    static constexpr StringView runnerNames[] = {"auto", "none", "wine", "qemu", "custom"};
    StringView                  resolved;
    SC_TRY(resolveKeywordValue("--runner", runnerValue, runnerNames, resolved, console));
    if (equalsAsciiIgnoreCase(resolved, "none"))
    {
        action.parameters.runner.type = Build::RunnerSpec::None;
    }
    else if (equalsAsciiIgnoreCase(resolved, "wine"))
    {
        action.parameters.runner.type = Build::RunnerSpec::Wine;
    }
    else if (equalsAsciiIgnoreCase(resolved, "qemu"))
    {
        action.parameters.runner.type = Build::RunnerSpec::QEMU;
    }
    else if (equalsAsciiIgnoreCase(resolved, "custom"))
    {
        action.parameters.runner.type = Build::RunnerSpec::Custom;
    }
    else
    {
        action.parameters.runner.type = Build::RunnerSpec::Auto;
    }
    return Result(true);
}

[[nodiscard]] inline Result printBuildActionCombinationError(Console& console, StringView message)
{
    console.printError("{}\n", message);
    console.flushStdErr();
    return Result::Error("Invalid SC-build option combination");
}

[[nodiscard]] inline Result resolveBuildGeneratorKeyword(StringView value, StringView& resolved, Console& console)
{
    static constexpr StringView generatorNames[] = {"default", "native", "make", "xcode", "vs2022", "vs2019"};
    return resolveKeywordValue("--generator", value, generatorNames, resolved, console);
}

[[nodiscard]] inline Result resolveBuildArchitectureKeyword(StringView value, StringView& resolved, Console& console)
{
    static constexpr StringView architectureNames[] = {"arm64", "intel32", "intel64", "wasm", "any"};
    return resolveKeywordValue("--arch", value, architectureNames, resolved, console);
}

[[nodiscard]] inline bool targetMachineCanRunDirectly(const Build::Machine& hostMachine, const Build::Machine& target)
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

[[nodiscard]] inline bool isWindowsGNUTargetMachine(const Build::Machine& target)
{
    return target.platform == Build::Platform::Windows and target.environment == Build::TargetEnvironment::WindowsGNU;
}

[[nodiscard]] inline bool isWindowsMSVCTargetMachine(const Build::Machine& target)
{
    return target.platform == Build::Platform::Windows and target.environment == Build::TargetEnvironment::WindowsMSVC;
}

[[nodiscard]] inline bool tripleConflictsWithTargetArchitecture(StringView                triple,
                                                                Build::Architecture::Type architecture)
{
    const bool isX86 = startsWithAsciiIgnoreCase(triple, "x86_64") or startsWithAsciiIgnoreCase(triple, "amd64") or
                       startsWithAsciiIgnoreCase(triple, "i686") or startsWithAsciiIgnoreCase(triple, "i386");
    const bool isArm = startsWithAsciiIgnoreCase(triple, "aarch64") or startsWithAsciiIgnoreCase(triple, "arm64");

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

[[nodiscard]] inline Result validateBuildActionCombination(Build::Action::Type         actionType,
                                                           const BuildCLIParseContext& context,
                                                           const Build::Action& action, Console& console)
{
    const bool windowsGNUTarget  = isWindowsGNUTargetMachine(action.parameters.targetMachine);
    const bool windowsMSVCTarget = isWindowsMSVCTargetMachine(action.parameters.targetMachine);
    if (windowsGNUTarget)
    {
        if (not context.generator.isEmpty())
        {
            StringView resolvedGenerator;
            SC_TRY(resolveBuildGeneratorKeyword(context.generator, resolvedGenerator, console));
            if (not(equalsAsciiIgnoreCase(resolvedGenerator, "default") or
                    equalsAsciiIgnoreCase(resolvedGenerator, "native")))
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
                 not equalsAsciiIgnoreCase(resolvedArchitecture, "intel64")) or
                (action.parameters.targetMachine.architecture == Build::Architecture::Arm64 and
                 not equalsAsciiIgnoreCase(resolvedArchitecture, "arm64")))
            {
                return printBuildActionCombinationError(
                    console, "Windows GNU target profiles require --arch to match the selected target profile");
            }
        }

        if (not context.targetTriple.isEmpty())
        {
            if (not containsAsciiIgnoreCase(context.targetTriple, "windows") or
                not containsAsciiIgnoreCase(context.targetTriple, "gnu"))
            {
                return printBuildActionCombinationError(
                    console, "Windows GNU target profiles require a Windows GNU --triple override");
            }
            if (tripleConflictsWithTargetArchitecture(context.targetTriple,
                                                      action.parameters.targetMachine.architecture))
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
            if (not(equalsAsciiIgnoreCase(resolvedGenerator, "default") or
                    equalsAsciiIgnoreCase(resolvedGenerator, "native")))
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
                 not equalsAsciiIgnoreCase(resolvedArchitecture, "intel64")) or
                (action.parameters.targetMachine.architecture == Build::Architecture::Arm64 and
                 not equalsAsciiIgnoreCase(resolvedArchitecture, "arm64")))
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
        return printBuildActionCombinationError(console, "QEMU runner is not implemented yet");
    case Build::RunnerSpec::Custom:
        if (action.parameters.runner.executable.isEmpty())
        {
            return printBuildActionCombinationError(console, "Custom runner requires --runner-path");
        }
        break;
    }
    return Result(true);
}

[[nodiscard]] inline Result scanNamedOutputMode(Span<const StringView> arguments, Build::OutputMode::Type& outputMode,
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

            if (equalsAsciiIgnoreCase(longName, "quiet"))
            {
                outputMode  = Build::OutputMode::Quiet;
                wasProvided = true;
                continue;
            }
            if (equalsAsciiIgnoreCase(longName, "normal"))
            {
                outputMode  = Build::OutputMode::Normal;
                wasProvided = true;
                continue;
            }
            if (equalsAsciiIgnoreCase(longName, "verbose"))
            {
                outputMode  = Build::OutputMode::Verbose;
                wasProvided = true;
                continue;
            }
            if (equalsAsciiIgnoreCase(longName, "output"))
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

[[nodiscard]] inline Result prepareBuildAction(Build::Action::Type actionType, Tool::Arguments& arguments,
                                               Build::Action& action, BuildCLIResolvedStorage& resolvedStorage,
                                               BuildCLIStatus& status)
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
    CommandLineOption     options[12];
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
    options[numOptions].help      = "Build target profile (host, native, windows-gnu-x86_64, windows-gnu-arm64, "
                                    "windows-msvc-x86_64, windows-msvc-arm64)";
    options[numOptions].valueName = "PROFILE";
    options[numOptions].value     = CommandLineValue::stringSpan(context.targetProfile);
    numOptions++;

    options[numOptions].longName  = "generator";
    options[numOptions].shortName = 'g';
    options[numOptions].help      = "Build generator (default, native, make, xcode, vs2022, vs2019)";
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

[[nodiscard]] inline Result runBuildValidate(Tool::Arguments& arguments, Build::Directories& directories)
{
    SC_TRY(Path::join(directories.projectsDirectory, {arguments.toolDestination.view(), PROJECTS_SUBDIR}));
    SC_TRY(Path::join(directories.outputsDirectory, {arguments.toolDestination.view(), OUTPUTS_SUBDIR}));
    SC_TRY(Path::join(directories.intermediatesDirectory, {arguments.toolDestination.view(), INTERMEDIATES_SUBDIR}));
    SC_TRY(Path::join(directories.buildCacheDirectory, {arguments.toolDestination.view(), BUILD_CACHE_SUBDIR}));
    SC_TRY(Path::join(directories.packagesCacheDirectory, {arguments.toolDestination.view(), PackagesCacheDirectory}));
    SC_TRY(
        Path::join(directories.packagesInstallDirectory, {arguments.toolDestination.view(), PackagesInstallDirectory}));

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
        not Path::isAbsolute(arguments.libraryDirectory.view(), SC::Path::AsNative))
    {
        return Result::Error("Both the build output directory and the library directory must be absolute paths");
    }
    return Result(true);
}

[[nodiscard]] inline Result runBuildConfigure(Tool::Arguments& arguments)
{
    Build::Action action;
    SC_TRY(runBuildValidate(arguments, action.parameters.directories));
    action.action = Build::Action::Configure;

    action.parameters.directories.libraryDirectory = arguments.libraryDirectory.view();
    if (arguments.arguments.sizeInElements() >= 1)
    {
        StringView afterSplit;
        if (arguments.arguments[0].splitBefore(SC_NATIVE_STR(":"), action.workspaceName))
        {
            SC_TRUST_RESULT(arguments.arguments[0].splitAfter(SC_NATIVE_STR(":"), action.projectName));
        }
        else
        {
            action.projectName = arguments.arguments[0];
        }
    }
    // TODO: We should run a matrix of all generators / platforms / architectures
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
    action.parameters.platform  = Build::Platform::Linux;
    arguments.console.print("Executing \"{}\" for Native on Linux platform\n", arguments.action);
    SC_TRY(Build::executeAction(action));
    action.parameters.generator = Build::Generator::Native;
    action.parameters.platform  = Build::Platform::Apple;
    arguments.console.print("Executing \"{}\" for Native on Apple platform\n", arguments.action);
    SC_TRY(Build::executeAction(action));
    return Result(true);
}

[[nodiscard]] inline Result runBuildAction(Build::Action::Type actionType, Tool::Arguments& arguments)
{
    Build::Action           action;
    BuildCLIResolvedStorage resolvedStorage;
    BuildCLIStatus          status = BuildCLIStatus::Ready;
    SC_TRY(prepareBuildAction(actionType, arguments, action, resolvedStorage, status));
    if (status == BuildCLIStatus::HelpRequested)
    {
        return Result(true);
    }
    if (status == BuildCLIStatus::Error)
    {
        return Result::Error("Invalid SC-build arguments");
    }

    SC_TRY(runBuildValidate(arguments, action.parameters.directories));
    action.parameters.directories.libraryDirectory = arguments.libraryDirectory.view();

    return Build::executeAction(action);
}

[[nodiscard]] inline Result runBuildDocumentation(StringView doxygenExecutable, Tool::Arguments& arguments)
{
    String outputDirectory;
    // TODO: De-hardcode the output "_Documentation" path
    SC_TRY(Path::join(outputDirectory, {arguments.toolDestination.view(), "_Documentation"}));
    {
        FileSystem fs;
        if (fs.init(outputDirectory.view()))
        {
            SC_TRY(fs.removeDirectoryRecursive(outputDirectory.view()));
        }
    }
    String documentationDirectory;
    // TODO: De-hardcode the source "Documentation" path
    SC_TRY(Path::join(documentationDirectory, {arguments.libraryDirectory.view(), "Documentation", "Doxygen"}));

    Process process;
    SC_TRY(process.setWorkingDirectory(documentationDirectory.view()));
    SC_TRY(process.setEnvironment("STRIP_FROM_PATH", documentationDirectory.view()));
    switch (HostPlatform)
    {
    case Platform::Apple: //
        SC_TRY(process.setEnvironment("PACKAGES_PLATFORM", "macos"));
        break;
    case Platform::Linux: //
        SC_TRY(process.setEnvironment("PACKAGES_PLATFORM", "linux"));
        break;
    case Platform::Windows: //
        SC_TRY(process.setEnvironment("PACKAGES_PLATFORM", "windows"));
        break;
    case Platform::Emscripten: return Result::Error("Unsupported platform");
    }
    SC_TRY(process.exec({doxygenExecutable}));
    SC_TRY_MSG(process.getExitStatus() == 0, "Build documentation failed");

    // TODO: Move this to the github CI file once automatic documentation publishing will been setup
    SC_TRY(Path::join(outputDirectory, {arguments.toolDestination.view(), "_Documentation", "docs"}));
    {
        // touch .nojekyll
        FileSystem fs;
        SC_TRY(fs.init(outputDirectory.view()));
        SC_TRY(fs.writeString(".nojekyll", ""));
    }
    return Result(true);
}

inline Result runBuildTool(Tool::Arguments& arguments)
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

} // namespace SC
