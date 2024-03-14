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
                     StringView sourcesDirectory);
} // namespace Build
} // namespace SC

namespace SC
{
constexpr StringView PROJECTS_SUBDIR = "_Projects";

[[nodiscard]] inline Result runBuildValidate(ToolsArguments& arguments, String& targetDirectory)
{
    Console&          console = arguments.console;
    StringNative<256> buffer;
    StringBuilder     builder(buffer);
    SC_TRY(Path::join(targetDirectory, {arguments.outputsDirectory, PROJECTS_SUBDIR}));
    SC_TRY(builder.format("targetDirectory   = {}", targetDirectory));
    console.printLine(buffer.view());
    SC_TRY(builder.format("sourcesDirectory  = {}", arguments.sourcesDirectory));
    console.printLine(buffer.view());
    if (not Path::isAbsolute(targetDirectory.view(), SC::Path::AsNative) or
        not Path::isAbsolute(arguments.sourcesDirectory, SC::Path::AsNative))
    {
        return Result::Error("Both --target and --sources must be absolute paths");
    }
    return Result(true);
}

[[nodiscard]] inline Result runBuildConfigure(ToolsArguments& arguments)
{
    StringNative<256> targetDirectory;
    SC_TRY(runBuildValidate(arguments, targetDirectory));
    Result res(true);
    res = Build::executeAction(Build::Actions::Configure, Build::Generator::VisualStudio2022, targetDirectory.view(),
                               arguments.sourcesDirectory);
    SC_TRY_MSG(res, "Build error Visual Studio 2022");
    res = Build::executeAction(Build::Actions::Configure, Build::Generator::XCode, targetDirectory.view(),
                               arguments.sourcesDirectory);
    SC_TRY_MSG(res, "Build error XCode");
    res = Build::executeAction(Build::Actions::Configure, Build::Generator::Make, targetDirectory.view(),
                               arguments.sourcesDirectory);
    SC_TRY_MSG(res, "Build error Makefile");
    return Result(true);
}

[[nodiscard]] inline Result runBuildCompile(ToolsArguments& arguments)
{
    StringNative<256> targetDirectory;
    SC_TRY(runBuildValidate(arguments, targetDirectory));
    switch (HostPlatform)
    {
    case Platform::Windows:
        return Build::executeAction(Build::Actions::Compile, Build::Generator::VisualStudio2022, targetDirectory.view(),
                                    arguments.sourcesDirectory);
    case Platform::Apple:
        return Build::executeAction(Build::Actions::Compile, Build::Generator::XCode, targetDirectory.view(),
                                    arguments.sourcesDirectory);
    case Platform::Linux:
        return Build::executeAction(Build::Actions::Compile, Build::Generator::Make, targetDirectory.view(),
                                    arguments.sourcesDirectory);
    default: //
        return Result::Error("Unsupported platform for compile");
    }
}

[[nodiscard]] inline Result runBuildCommand(ToolsArguments& arguments)
{
    Console& console = arguments.console;

    StringView action = "configure"; // If no action is passed we assume "configure"
    if (arguments.argc > 0)
    {
        action = StringView::fromNullTerminated(arguments.argv[0], StringEncoding::Ascii);
    }

    StringNative<256>  buffer;
    StringBuilder      builder(buffer);
    SC::Time::Absolute started = SC::Time::Absolute::now();
    SC_TRY(builder.format("SC-build \"{}\" started...", action));
    if (action == "configure")
    {
        SC_TRY(runBuildConfigure(arguments));
    }
    else if (action == "compile")
    {
        SC_TRY(runBuildCompile(arguments));
    }
    else
    {
        SC_TRY(builder.format("SC-build no action named \"{}\" exists", action));
        console.printLine(buffer.view());
        return Result::Error("SC-build error executing action");
    }
    Time::Relative elapsed = SC::Time::Absolute::now().subtract(started);
    SC_TRY(builder.format("SC-build \"{}\" finished (took {} ms)", action, elapsed.inRoundedUpperMilliseconds().ms));
    console.printLine(buffer.view());
    return Result(true);
}

#if !defined(SC_LIBRARY_PATH)
Result RunCommand(ToolsArguments& arguments) { return runBuildCommand(arguments); }
#endif

} // namespace SC
