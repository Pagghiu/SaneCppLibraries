// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Libraries/Foundation/Result.h"
#include "../Libraries/Strings/StringView.h"

namespace SC
{
struct Console;

namespace Tools
{
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

        Span<StringView> arguments;
    };
    static StringView getToolName();
    static StringView getDefaultAction();
    static Result     runTool(Arguments& arguments);
};
struct Package;
// Tools
Result runFormatTool(Tool::Arguments& arguments);
Result runBuildTool(Tool::Arguments& arguments);

inline Result runPackageTool(Tool::Arguments& arguments, Tools::Package* package = nullptr);
} // namespace Tools

} // namespace SC
