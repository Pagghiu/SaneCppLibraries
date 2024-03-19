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

    AsyncProcessExit processExits[32]; // Never launch more than 32 processes
    ProcessLimiter   processLimiter;
    SC_TRY(processLimiter.create(Process::getNumberOfProcessors(), {processExits}));

    struct LambdaVariables
    {
        ProcessLimiter& processLimiter;
        FormatSources   action;
        StringView      clangFormatExecutable;
    } lambda = {processLimiter, action, clangFormatExecutable};

    auto formatSourceFile = [&lambda](const StringView path)
    {
        switch (lambda.action)
        {
        case FormatSources::Execute: // Executes actual formatting
            return lambda.processLimiter.launch({lambda.clangFormatExecutable, path, "-i"});
        case FormatSources::Check: // Just checks if files are formatted
            return lambda.processLimiter.launch({lambda.clangFormatExecutable, path, "-dry-run", "-Werror"});
        }
        Assert::unreachable();
    };
    SC_TRY(FileSystemFinder::forEachFile(libraryDirectory, {".h", ".cpp", ".inl"}, {"_Build"}, formatSourceFile));
    return processLimiter.close();
}
} // namespace Tools
} // namespace SC

namespace SC
{
[[nodiscard]] Result runFormatTool(Tool::Arguments& arguments)
{
    SmallString<256> clangFormat;
    if (not Tools::findSystemClangFormat(arguments.console, "15", clangFormat))
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
