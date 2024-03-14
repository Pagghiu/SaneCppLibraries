// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Libraries/Foundation/Result.h"
#include "../Libraries/Strings/StringView.h"

namespace SC
{
struct Console;

struct ToolsArguments
{
    Console&   console;
    StringView sourcesDirectory;
    StringView outputsDirectory;

    const char** argv = nullptr;
    int          argc = 0;
};
// Entry point
Result RunCommand(ToolsArguments& commandArguments);

// Tools
Result runBuildCommand(ToolsArguments& arguments);

} // namespace SC
