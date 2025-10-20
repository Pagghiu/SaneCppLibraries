// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Libraries/Process/Process.h"
#include "../Libraries/Strings/Console.h"
#include "../Libraries/Strings/Path.h"
#include "../Libraries/Strings/StringBuilder.h"
#include "SC-build/Build.h"
#include "Tools.h"

#include "SC-package.h"

#if !defined(SC_LIBRARY_PATH)
#define SC_TOOLS_IMPORT
#include "SC-package.cpp"
#undef SC_TOOLS_IMPORT
#endif

namespace SC
{
namespace Tools
{

constexpr StringView PROJECTS_SUBDIR      = "_Projects";
constexpr StringView OUTPUTS_SUBDIR       = "_Outputs";
constexpr StringView INTERMEDIATES_SUBDIR = "_Intermediates";

[[nodiscard]] inline Result runBuildValidate(Tool::Arguments& arguments, Build::Directories& directories)
{
    SmallStringNative<256> buffer;

    Console& console = arguments.console;
    auto     builder = StringBuilder::create(buffer);
    SC_TRY(Path::join(directories.projectsDirectory, {arguments.toolDestination.view(), PROJECTS_SUBDIR}));
    SC_TRY(Path::join(directories.outputsDirectory, {arguments.toolDestination.view(), OUTPUTS_SUBDIR}));
    SC_TRY(Path::join(directories.intermediatesDirectory, {arguments.toolDestination.view(), INTERMEDIATES_SUBDIR}));
    SC_TRY(Path::join(directories.packagesCacheDirectory, {arguments.toolDestination.view(), PackagesCacheDirectory}));
    SC_TRY(
        Path::join(directories.packagesInstallDirectory, {arguments.toolDestination.view(), PackagesInstallDirectory}));
    SC_TRY(builder.append("projects         = \"{}\"\n", directories.projectsDirectory));
    SC_TRY(builder.append("outputs          = \"{}\"\n", directories.outputsDirectory));
    SC_TRY(builder.append("intermediates    = \"{}\"\n", directories.intermediatesDirectory));
    builder.finalize();
    console.print(buffer.view());
    if (not Path::isAbsolute(directories.projectsDirectory.view(), SC::Path::AsNative) or
        not Path::isAbsolute(arguments.libraryDirectory.view(), SC::Path::AsNative))
    {
        return Result::Error("Both --target and --sources must be absolute paths");
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
            SC_TRUST_RESULT(arguments.arguments[0].splitAfter(SC_NATIVE_STR(":"), action.target));
        }
        else
        {
            action.target = arguments.arguments[0];
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
    return Result(true);
}

[[nodiscard]] inline Result runBuildAction(Build::Action::Type actionType, Tool::Arguments& arguments)
{
    Build::Action action;
    action.action = actionType;
    SC_TRY(runBuildValidate(arguments, action.parameters.directories));
    action.parameters.directories.libraryDirectory = arguments.libraryDirectory.view();
    switch (HostPlatform)
    {
    case Platform::Windows: {
        action.parameters.generator = Build::Generator::VisualStudio2022;
        action.parameters.platform  = Build::Platform::Windows;
    }
    break;
    case Platform::Apple: {
        action.parameters.generator = Build::Generator::Make;
        action.parameters.platform  = Build::Platform::Apple;
    }
    break;
    case Platform::Linux: {
        action.parameters.generator = Build::Generator::Make;
        action.parameters.platform  = Build::Platform::Linux;
    }
    break;
    default: return Result::Error("Unsupported platform for compile");
    }

    if (arguments.arguments.sizeInElements() >= 1)
    {
        StringView afterSplit;
        if (arguments.arguments[0].splitBefore(SC_NATIVE_STR(":"), action.workspaceName))
        {
            SC_TRUST_RESULT(arguments.arguments[0].splitAfter(SC_NATIVE_STR(":"), action.target));
        }
        else
        {
            action.target = arguments.arguments[0];
        }
    }
    StringSpan args[3];

    for (size_t idx = 1; idx < min(size_t(4), arguments.arguments.sizeInElements()); ++idx)
    {
        auto arg = arguments.arguments[idx];
        if (arg == "--")
        {
            (void)arguments.arguments.sliceStart(idx + 1, action.additionalArguments);
            break;
        }
        args[idx - 1] = arg;
    }

    if (not args[0].isEmpty())
    {
        action.configuration = args[0];
    }

    if (args[1] == "xcode")
    {
        action.parameters.generator = Build::Generator::XCode;
    }
    else if (args[1] == "make")
    {
        action.parameters.generator = Build::Generator::Make;
    }
    else if (args[1] == "vs2022")
    {
        action.parameters.generator = Build::Generator::VisualStudio2022;
    }
    else if (args[1] == "vs2019")
    {
        action.parameters.generator = Build::Generator::VisualStudio2019;
    }
    else if (args[1] == "default")
    {
        // Defaults already set
    }

    if (args[2] == "arm64")
    {
        action.parameters.architecture = Build::Architecture::Arm64;
    }
    else if (args[2] == "intel32")
    {
        action.parameters.architecture = Build::Architecture::Intel32;
    }
    else if (args[2] == "intel64")
    {
        action.parameters.architecture = Build::Architecture::Intel64;
    }
    else if (args[2] == "wasm")
    {
        action.parameters.architecture = Build::Architecture::Wasm;
    }
    else if (args[2] == "any")
    {
        action.parameters.architecture = Build::Architecture::Any;
    }

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
        return Result::Error("SC-format unknown action (supported \"configure\" or \"compile\")");
    }
}

#if !defined(SC_LIBRARY_PATH) && !defined(SC_TOOLS_IMPORT)
StringView Tool::getToolName() { return "SC-build"; }
StringView Tool::getDefaultAction() { return "configure"; }
Result     Tool::runTool(Tool::Arguments& arguments) { return runBuildTool(arguments); }
#endif
} // namespace Tools

} // namespace SC
