// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

// TODO: Figure out a way to avoid this macro
#define SC_TOOLS_IMPORT
#include "SC-package.cpp"
#undef SC_TOOLS_IMPORT

#include "Libraries/Foundation/Deferred.h"
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
static Result formatSourceFiles(FormatSources action, StringView clangFormatExecutable, StringView sourcesDirectory)
{
    // This would be the equivalent more or less of:
    //
    // cd "${sourcesDirectory}" && \
    // find . \( -iname \*.h -o -iname \*.cpp -o -iname \*.inl \) -not \( -path "*/_Build/*" \)
    // | xargs "${clangFormatExecutable}" -i
    // or
    // | xargs "${clangFormatExecutable}" --dry-run -Werror

    AsyncProcessExit processExits[32]; // No more than 32 processes in any case

    ProcessLimiter processLimiter;
    SC_TRY(processLimiter.create(16, {processExits})); // TODO: Replace hardcoded 16 processes with nproc

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
    SC_TRY(FileSystemFinder::forEachFile(sourcesDirectory, {".h", ".cpp", ".inl"}, {"_Build"}, clangFormatFile));

    return processLimiter.close();
}
} // namespace Tools

} // namespace SC

namespace SC
{
Result runFormatCommand(ToolsArguments& arguments)
{
    Console& console = arguments.console;

    StringView action = "execute"; // If no action is passed we assume "format"
    if (arguments.argc > 0)
    {
        action = StringView::fromNullTerminated(arguments.argv[0], StringEncoding::Ascii);
    }

    StringNative<256> packagesDirectory;
    StringNative<256> toolsDirectory;
    StringNative<256> buffer;
    StringBuilder     builder(buffer);
    SC_TRY(Path::join(packagesDirectory, {arguments.outputsDirectory, PackagesDirectory}));
    SC_TRY(Path::join(toolsDirectory, {arguments.outputsDirectory, ToolsDirectory}));
    SC_TRY(builder.append("sourcesDirectory  = \"{}\"\n", arguments.sourcesDirectory));
    SC_TRY(builder.append("packagesDirectory = \"{}\"\n", packagesDirectory.view()));
    SC_TRY(builder.append("toolsDirectory    = \"{}\"", toolsDirectory.view()));
    console.printLine(buffer.view());

    SC::Time::Absolute started = SC::Time::Absolute::now();
    SC_TRY(builder.format("SC-format \"{}\" started...", action));
    console.printLine(buffer.view());
    if (action == "execute" or action == "check")
    {
        String clangFormatPath;
        if (not Tools::findSystemClangFormat("15.", clangFormatPath))
        {
            // If no system installed clang-format version 15 has been found, we install our own copy
            Tools::Package clangPackage;
            SC_TRY(Tools::installClangBinaries(packagesDirectory.view(), toolsDirectory.view(), clangPackage));
            SC_TRY(StringBuilder(clangFormatPath).format("{}/bin/clang-format", clangPackage.installDirectoryLink));
        }

        if (action == "execute")
        {
            SC_TRY(Tools::formatSourceFiles(Tools::FormatSources::Execute, clangFormatPath.view(),
                                            arguments.sourcesDirectory));
        }
        else
        {
            SC_TRY(Tools::formatSourceFiles(Tools::FormatSources::Check, clangFormatPath.view(),
                                            arguments.sourcesDirectory));
        }
    }
    else
    {
        SC_TRY(builder.format("SC-format no action named \"{}\" exists", action));
        console.printLine(buffer.view());
        return Result::Error("SC-format error executing action");
    }

    Time::Relative elapsed = SC::Time::Absolute::now().subtract(started);
    SC_TRY(builder.format("SC-format \"{}\" finished (took {} ms)", action, elapsed.inRoundedUpperMilliseconds().ms));
    console.printLine(buffer.view());
    return Result(true);
}
#if !defined(SC_LIBRARY_PATH) && !defined(SC_TOOLS_IMPORT)
Result RunCommand(ToolsArguments& arguments) { return runFormatCommand(arguments); }
#endif
} // namespace SC
