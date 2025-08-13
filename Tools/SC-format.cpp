// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

// TODO: Figure out a way to avoid this macro

#include "../Libraries/Async/Internal/IntrusiveDoubleLinkedList.inl" // IWYU pragma: keep
#include "../Libraries/Foundation/Deferred.h"
#include "SC-package.h"

#if !defined(SC_LIBRARY_PATH)
#define SC_TOOLS_IMPORT
#include "SC-package.cpp"
#undef SC_TOOLS_IMPORT
#endif

#include "SC-format.h"
extern SC::Console* globalConsole; // Defined in Tools/SC-format.cpp

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
        globalConsole->print("{}\n", path);
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
[[nodiscard]] Result runFormatTool(Tool::Arguments& arguments)
{
    SmallString<256> clangFormat;
    if (not Tools::findSystemClangFormat(arguments.console, "19", clangFormat))
    {
        StringView      additionalArgs[1];
        Tool::Arguments args = arguments;
        args.tool            = "packages";
        args.action          = "install";
        additionalArgs[0]    = "clang";
        args.arguments       = {additionalArgs};
        // If no system installed clang-format (matching version 19) has been found, we install a local copy
        Tools::Package clangPackage;
        SC_TRY(runPackageTool(args, &clangPackage));
        SC_TRY(StringBuilder(clangFormat).format("{}/bin/clang-format", clangPackage.installDirectoryLink));
    }
    arguments.console.print("Using: {}\n", clangFormat);

    if (arguments.action == "execute")
    {
        return Tools::formatSourceFiles(Tools::FormatSources::Execute, clangFormat.view(),
                                        arguments.libraryDirectory.view());
    }
    else if (arguments.action == "check")
    {
        return Tools::formatSourceFiles(Tools::FormatSources::Check, clangFormat.view(),
                                        arguments.libraryDirectory.view());
    }
    else
    {
        return Result::Error("SC-format unknown action (supported \"execute\" or \"check\")");
    }
}
#if !defined(SC_LIBRARY_PATH) && !defined(SC_TOOLS_IMPORT)
StringView Tool::getToolName() { return "SC-format"; }
StringView Tool::getDefaultAction() { return "execute"; }
Result     Tool::runTool(Tool::Arguments& arguments) { return runFormatTool(arguments); }
#endif
} // namespace Tools
} // namespace SC
