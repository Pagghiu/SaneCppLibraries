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
namespace Build
{
// Defined in SC-build.cpp
Result executeAction(Build::Actions::Type action, Build::Generator::Type generator, StringView targetDirectory,
                     StringView libraryDirectory);
} // namespace Build
} // namespace SC

namespace SC
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
    Result res(true);
    res = Build::executeAction(Build::Actions::Configure, Build::Generator::VisualStudio2022, projectsDirectory.view(),
                               arguments.libraryDirectory);
    SC_TRY_MSG(res, "Build error Visual Studio 2022");
    res = Build::executeAction(Build::Actions::Configure, Build::Generator::XCode, projectsDirectory.view(),
                               arguments.libraryDirectory);
    SC_TRY_MSG(res, "Build error XCode");
    res = Build::executeAction(Build::Actions::Configure, Build::Generator::Make, projectsDirectory.view(),
                               arguments.libraryDirectory);
    SC_TRY_MSG(res, "Build error Makefile");
    return Result(true);
}

[[nodiscard]] inline Result runBuildCompile(Tool::Arguments& arguments)
{
    StringNative<256> projectsDirectory;
    SC_TRY(runBuildValidate(arguments, projectsDirectory));
    switch (HostPlatform)
    {
    case Platform::Windows:
        return Build::executeAction(Build::Actions::Compile, Build::Generator::VisualStudio2022,
                                    projectsDirectory.view(), arguments.libraryDirectory);
    case Platform::Apple:
        return Build::executeAction(Build::Actions::Compile, Build::Generator::XCode, projectsDirectory.view(),
                                    arguments.libraryDirectory);
    case Platform::Linux:
        return Build::executeAction(Build::Actions::Compile, Build::Generator::Make, projectsDirectory.view(),
                                    arguments.libraryDirectory);
    default: //
        return Result::Error("Unsupported platform for compile");
    }
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

} // namespace SC
