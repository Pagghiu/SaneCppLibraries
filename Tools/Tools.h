// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Libraries/Foundation/Result.h"
#include "../Libraries/Memory/String.h"
#include "../Libraries/Strings/StringBuilder.h"

namespace SC
{
struct Console;
struct String;

namespace Tools
{
template <typename... Types>
inline String format(const StringView fmt, Types&&... types)
{
    String result = StringEncoding::Utf8;
    (void)StringBuilder::format(result, fmt, forward<Types>(types)...);
    return result;
}
struct Tool
{
    struct Arguments
    {
        Console&          console;
        SmallString<1024> libraryDirectory;
        SmallString<1024> toolSource;
        SmallString<1024> toolDestination;
        StringView        tool   = StringView();
        StringView        action = StringView();

        Span<const StringView> arguments;
    };
    static StringView getToolName();
    static StringView getDefaultAction();
    static Result     runTool(Arguments& arguments);
};
struct Package;
// Tools
Result runFormatTool(Tool::Arguments& arguments);
Result runBuildTool(Tool::Arguments& arguments);
Result runPackageTool(Tool::Arguments& arguments, Tools::Package* package = nullptr);
Result findSystemClangFormat(Console& console, StringView wantedMajorVersion, String& foundPath);
} // namespace Tools

} // namespace SC
