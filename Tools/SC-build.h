// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Libraries/Build/Build.h"
#include "../Libraries/FileSystem/Path.h"
#include "../Libraries/Strings/Console.h"
#include "../Libraries/Strings/SmallString.h"
#include "../Libraries/Strings/StringBuilder.h"
#include "../Libraries/Time/Time.h"
#include "Tools.h"

namespace SC
{
namespace Tools
{

constexpr StringView PROJECTS_SUBDIR = "_Projects";

[[nodiscard]] inline Result runBuildValidate(Tool::Arguments& arguments, String& projectsDirectory)
{
    Console&          console = arguments.console;
    StringNative<256> buffer;
    StringBuilder     builder(buffer);
    SC_TRY(Path::join(projectsDirectory, {arguments.outputsDirectory, PROJECTS_SUBDIR}));
    SC_TRY(builder.format("projects         = \"{}\"\n", projectsDirectory));
    console.print(buffer.view());
    if (not Path::isAbsolute(projectsDirectory.view(), SC::Path::AsNative) or
        not Path::isAbsolute(arguments.libraryDirectory, SC::Path::AsNative))
    {
        return Result::Error("Both --target and --sources must be absolute paths");
    }
    return Result(true);
}

[[nodiscard]] inline Result runBuildConfigure(Tool::Arguments& arguments)
{
    StringNative<256> projectsDirectory;
    SC_TRY(runBuildValidate(arguments, projectsDirectory));
    Result        res(true);
    Build::Action action;
    action.action           = Build::Action::Configure;
    action.targetDirectory  = projectsDirectory.view();
    action.libraryDirectory = arguments.libraryDirectory;

    action.generator = Build::Generator::VisualStudio2022;
    res              = Build::executeAction(action);
    SC_TRY_MSG(res, "Build error Visual Studio 2022");
    action.generator = Build::Generator::XCode;
    res              = Build::executeAction(action);
    SC_TRY_MSG(res, "Build error XCode");
    action.generator = Build::Generator::Make;
    res              = Build::executeAction(action);
    SC_TRY_MSG(res, "Build error Makefile");
    return Result(true);
}

[[nodiscard]] inline Result runBuildCompile(Tool::Arguments& arguments)
{
    StringNative<256> projectsDirectory;
    SC_TRY(runBuildValidate(arguments, projectsDirectory));

    Build::Action action;
    action.action           = Build::Action::Compile;
    action.targetDirectory  = projectsDirectory.view();
    action.libraryDirectory = arguments.libraryDirectory;
    switch (HostPlatform)
    {
    case Platform::Windows: action.generator = Build::Generator::VisualStudio2022; break;
    case Platform::Apple: action.generator = Build::Generator::Make; break;
    case Platform::Linux: action.generator = Build::Generator::Make; break;
    default: return Result::Error("Unsupported platform for compile");
    }

    if (arguments.arguments.sizeInElements() >= 1)
    {
        action.configuration = arguments.arguments[0];
    }
    if (arguments.arguments.sizeInElements() >= 2)
    {
        if (arguments.arguments[1] == "xcode")
        {
            action.generator = Build::Generator::XCode;
        }
        else if (arguments.arguments[1] == "make")
        {
            action.generator = Build::Generator::Make;
        }
        else if (arguments.arguments[1] == "vs2022")
        {
            action.generator = Build::Generator::VisualStudio2022;
        }
        else if (arguments.arguments[1] == "default")
        {
            switch (HostPlatform)
            {
            case Platform::Windows: action.generator = Build::Generator::VisualStudio2022; break;
            default: action.generator = Build::Generator::Make; break;
            }
        }
    }
    if (arguments.arguments.sizeInElements() >= 3)
    {
        if (arguments.arguments[2] == "arm64")
        {
            action.architecture = Build::Architecture::Arm64;
        }
        else if (arguments.arguments[2] == "intel32")
        {
            action.architecture = Build::Architecture::Intel32;
        }
        else if (arguments.arguments[2] == "intel64")
        {
            action.architecture = Build::Architecture::Intel64;
        }
        else if (arguments.arguments[2] == "wasm")
        {
            action.architecture = Build::Architecture::Wasm;
        }
        else if (arguments.arguments[2] == "any")
        {
            action.architecture = Build::Architecture::Any;
        }
    }

    return Build::executeAction(action);
}

[[nodiscard]] inline Result runBuildTool(Tool::Arguments& arguments)
{
    if (arguments.action == "configure")
    {
        return runBuildConfigure(arguments);
    }
    else if (arguments.action == "compile")
    {
        return runBuildCompile(arguments);
    }
    else
    {
        return Result::Error("SC-format unknown action (supported \"configure\" or \"compile\")");
    }
}

#if !defined(SC_LIBRARY_PATH) && !defined(SC_TOOLS_IMPORT)
StringView Tool::getToolName() { return "build"; }
StringView Tool::getDefaultAction() { return "configure"; }
Result     Tool::runTool(Tool::Arguments& arguments) { return runBuildTool(arguments); }
#endif
} // namespace Tools

} // namespace SC
