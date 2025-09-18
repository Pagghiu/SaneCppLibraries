// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../Libraries/Containers/Vector.h"
#include "../Libraries/Foundation/Deferred.h"
#include "../Libraries/Strings/Console.h"
#include "../Libraries/Strings/Path.h"
#include "../Libraries/Strings/String.h"
#include "../Libraries/Time/Time.h"

#include "../SC.cpp"

#include "Tools.h"

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

static SC::SmallString<512> gFormatString;

SC::Console* globalConsole;

int main(int argc, const char* argv[])
{
    using namespace SC;
    using namespace SC::Tools;
    Console::tryAttachingToParentConsole();
    Console console;

    Tool::Arguments arguments{console};

    globalConsole = &console;

    if (argc < 4)
    {
        console.printLine("Usage: ${TOOL} libraryDirectory toolSource toolDestination [tool] [action]");
        return -1;
    }
    int numArguments = 0;

    // TODO: It could be worth cleanup and move command-line utf handling somewhere it could be reused
#if SC_PLATFORM_WINDOWS
    wchar_t* const* nativeArgs = nullptr;
    SC_COMPILER_UNUSED(argc);
    SC_COMPILER_UNUSED(argv);
    LPWSTR* args = ::CommandLineToArgvW(::GetCommandLineW(), &numArguments);
    if (args == nullptr)
    {
        console.printLine("Failed parsing command line");
        return -1;
    }
    auto freeDefer = [args]() { ::LocalFree(args); };
    nativeArgs     = args;
#else
    const char** nativeArgs = nullptr;
    nativeArgs              = argv;
    numArguments            = argc;
#endif
    const StringView libraryDirectory = StringView::fromNullTerminated(nativeArgs[1], StringEncoding::Native);
    const StringView toolSource       = StringView::fromNullTerminated(nativeArgs[2], StringEncoding::Native);
    const StringView toolDestination  = StringView::fromNullTerminated(nativeArgs[3], StringEncoding::Native);

    {
        (void)Path::normalize(arguments.libraryDirectory, libraryDirectory, Path::Type::AsNative);
        (void)Path::normalize(arguments.toolSource, toolSource, Path::Type::AsNative);
        (void)Path::normalize(arguments.toolDestination, toolDestination, Path::Type::AsNative);
    }

    arguments.tool = Tool::getToolName();

    if (numArguments > 5)
    {
        arguments.action = StringView::fromNullTerminated(argv[5], StringEncoding::Ascii);
    }
    else
    {
        arguments.action = Tool::getDefaultAction();
    }
    constexpr int maxAdditionalArguments = 16;
    StringView    additionalArguments[maxAdditionalArguments];
    if (numArguments - 4 > maxAdditionalArguments)
    {
        console.printLine("Error: Exceeded maximum number of additional arguments (20)");
        return -1;
    }

    for (int idx = 6; idx < numArguments; ++idx)
    {
        additionalArguments[idx - 6] = StringView::fromNullTerminated(nativeArgs[idx], StringEncoding::Native);
    }

    if (numArguments >= 6)
    {
        arguments.arguments = {additionalArguments, static_cast<size_t>(numArguments - 6)};
    }

    SC::Time::Realtime started = SC::Time::Realtime::now();

    SC_TRY(StringBuilder::format(gFormatString, "{} \"{}\" started\n", arguments.tool, arguments.action));
    StringBuilder builder(gFormatString, StringBuilder::DoNotClear);
    SC_TRY(builder.append("librarySource    = \"{}\"\n", arguments.libraryDirectory));
    SC_TRY(builder.append("toolSource       = \"{}\"\n", arguments.toolSource));
    SC_TRY(builder.append("toolDestination  = \"{}\"\n", arguments.toolDestination));
    builder.finalize();
    console.print(gFormatString.view());

    const Result   result  = Tool::runTool(arguments);
    const uint64_t elapsed = SC::Time::Realtime::now().subtractExact(started).ms;

    SC_TRY(StringBuilder::format(gFormatString, "{} \"{}\" finished (took {} ms)\n", arguments.tool, arguments.action,
                                 elapsed));

    console.print(gFormatString.view());
    if (not result)
    {
        console.printLine(StringView::fromNullTerminated(result.message, StringEncoding::Ascii));
        return -1;
    }
    return 0;
}
