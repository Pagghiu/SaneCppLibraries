// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../Libraries/Containers/SmallVector.h"
#include "../Libraries/FileSystem/Path.h"
#include "../Libraries/Strings/Console.h"
#include "../Libraries/Strings/SmallString.h"
#include "../Libraries/Time/Time.h"

#include "../Bindings/cpp/SC.cpp"

#include "Tools.h"

static SC::SmallVector<char, 512> gConsoleBuffer;

int main(int argc, const char* argv[])
{
    using namespace SC;

    Console        console(gConsoleBuffer);
    ToolsArguments arguments{console};
    arguments.sourcesDirectory = StringView::fromNullTerminated(argv[1], StringEncoding::Ascii);
    arguments.outputsDirectory = StringView::fromNullTerminated(argv[2], StringEncoding::Ascii);
    arguments.argc             = argc - 3;
    arguments.argv             = argv + 3;

    Result result = RunCommand(arguments);
    if (not result)
    {
        console.printLine(StringView::fromNullTerminated(result.message, StringEncoding::Ascii));
        return -1;
    }
    return 0;
}
