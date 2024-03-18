// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../Libraries/Containers/SmallVector.h"
#include "../Libraries/FileSystem/Path.h"
#include "../Libraries/Foundation/Deferred.h"
#include "../Libraries/Strings/Console.h"
#include "../Libraries/Strings/SmallString.h"
#include "../Libraries/Time/Time.h"

#include "../Bindings/cpp/SC.cpp"

#include "Tools.h"

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

static SC::SmallVector<char, 512> gConsoleBuffer;
static SC::SmallString<512>       gFormatString;

int main(int argc, const char* argv[])
{
    using namespace SC;

    Console         console(gConsoleBuffer);
    Tool::Arguments arguments{console};

    if (argc < 4)
    {
        console.printLine("Usage: SC-${TOOL} libraryDirectory toolDirectory outputsDirectory [tool] [action]");
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
    arguments.libraryDirectory = StringView::fromNullTerminated(nativeArgs[1], StringEncoding::Native);
    arguments.toolDirectory    = StringView::fromNullTerminated(nativeArgs[2], StringEncoding::Native);
    arguments.outputsDirectory = StringView::fromNullTerminated(nativeArgs[3], StringEncoding::Native);

    if (numArguments > 4)
    {
        arguments.tool = StringView::fromNullTerminated(argv[4], StringEncoding::Ascii);
    }
    else
    {
        arguments.tool = Tool::getToolName();
    }

    if (numArguments > 5)
    {
        arguments.action = StringView::fromNullTerminated(argv[5], StringEncoding::Ascii);
    }
    else
    {
        arguments.action = Tool::getDefaultAction();
    }

    StringBuilder builder(gFormatString);

    SC::Time::Absolute started = SC::Time::Absolute::now();

    SC_TRY(builder.format("SC-{} \"{}\" started...\n", arguments.tool, arguments.action));

    SC_TRY(builder.append("librarySource    = \"{}\"\n", arguments.libraryDirectory));
    SC_TRY(builder.append("toolSource       = \"{}\"\n", arguments.toolDirectory));
    SC_TRY(builder.append("outputs          = \"{}\"\n", arguments.outputsDirectory));
    console.print(gFormatString.view());

    const Result   result  = Tool::runTool(arguments);
    const uint64_t elapsed = SC::Time::Absolute::now().subtract(started).inRoundedUpperMilliseconds().ms;

    SC_TRY(builder.format("SC-{} \"{}\" finished (took {} ms)\n", arguments.tool, arguments.action, elapsed));

    console.print(gFormatString.view());
    if (not result)
    {
        console.printLine(StringView::fromNullTerminated(result.message, StringEncoding::Ascii));
        return -1;
    }
    return 0;
}
