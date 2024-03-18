// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Libraries/Foundation/Result.h"
#include "../Libraries/Strings/StringView.h"

namespace SC
{
struct Console;
struct Tool
{
    struct Arguments
    {
        Console&   console;
        StringView libraryDirectory;
        StringView toolDirectory;
        StringView outputsDirectory;
        StringView tool   = StringView();
        StringView action = StringView();
    };
    static StringView getToolName();
    static StringView getDefaultAction();
    static Result     runTool(Arguments& arguments);
};

// Tools
Result runFormatTool(Tool::Arguments& arguments);
Result runBuildTool(Tool::Arguments& arguments);
Result runPackageTool(Tool::Arguments& arguments);

} // namespace SC
