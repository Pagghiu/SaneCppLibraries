// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

// TODO: Figure out a way to avoid this macro
#define SC_TOOLS_IMPORT
#include "SC-package.cpp"
#undef SC_TOOLS_IMPORT

#include "Tools/SC-format.h"

namespace SC
{
namespace Tools
{
enum class FormatSources
{
    Execute,
    Check,
};
static Result formatSourceFiles(FormatSources action, StringView clangFormatExecutable, StringView libraryDirectory)
{
    // This would be the equivalent more or less of:
    //
    // cd "${libraryDirectory}" && \
    // find . \( -iname \*.h -o -iname \*.cpp -o -iname \*.inl \) -not \( -path "*/_Build/*" \)
    // | xargs "${clangFormatExecutable}" -i
    // or
    // | xargs "${clangFormatExecutable}" --dry-run -Werror

    // Create Process::getNumberOfProcessors() processes but never more than 32
    AsyncProcessExit processExits[32];
    ProcessLimiter   processLimiter;
    SC_TRY(processLimiter.create(Process::getNumberOfProcessors(), {processExits}));

    Span<StringView> clangFormatArguments;
    StringView       commandArgument[4];
    commandArgument[0] = clangFormatExecutable;
    switch (action)
    {
    case FormatSources::Execute:
        commandArgument[2]   = "-i";
        clangFormatArguments = {commandArgument, 3};
        break;
    case FormatSources::Check:
        commandArgument[2]   = "--dry-run";
        commandArgument[3]   = "-Werror";
        clangFormatArguments = {commandArgument, 4};
        break;
    default: return Result::Error("Invalid action");
    }

    auto clangFormatFile = [&](const StringView path)
    {
        clangFormatArguments[1] = path;
        return processLimiter.launch(clangFormatArguments);
    };
    SC_TRY(FileSystemFinder::forEachFile(libraryDirectory, {".h", ".cpp", ".inl"}, {"_Build"}, clangFormatFile));

    return processLimiter.close();
}
} // namespace Tools
} // namespace SC

namespace SC
{
[[nodiscard]] Result runFormatTool(Tool::Arguments& arguments)
{
    SmallString<256> clangFormat;
    if (not Tools::findSystemClangFormat("15.", clangFormat))
    {
        Tool::Arguments args = arguments;
        args.tool            = "packages";
        args.action          = "install";
        // If no system installed clang-format (matching version 15) has been found, we install a local copy
        Tools::Package clangPackage;
        SC_TRY(runPackageTool(args, &clangPackage));
        SC_TRY(StringBuilder(clangFormat).format("{}/bin/clang-format", clangPackage.installDirectoryLink));
    }

    if (arguments.action == "execute")
    {
        return Tools::formatSourceFiles(Tools::FormatSources::Execute, clangFormat.view(), arguments.libraryDirectory);
    }
    else if (arguments.action == "check")
    {
        return Tools::formatSourceFiles(Tools::FormatSources::Check, clangFormat.view(), arguments.libraryDirectory);
    }
    else
    {
        return Result::Error("SC-format unknown action (supported \"execute\" or \"check\")");
    }
}
#if !defined(SC_LIBRARY_PATH) && !defined(SC_TOOLS_IMPORT)
StringView Tool::getToolName() { return "format"; }
StringView Tool::getDefaultAction() { return "execute"; }
Result     Tool::runTool(Tool::Arguments& arguments) { return runFormatTool(arguments); }
#endif
} // namespace SC
