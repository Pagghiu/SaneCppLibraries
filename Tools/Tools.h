// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Libraries/Foundation/Result.h"
#include "../Libraries/Strings/SmallString.h"
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
    (void)StringBuilder(result).format(fmt, forward<Types>(types)...);
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
[[nodiscard]] Result runFormatTool(Tool::Arguments& arguments);
[[nodiscard]] Result runBuildTool(Tool::Arguments& arguments);
[[nodiscard]] Result runPackageTool(Tool::Arguments& arguments, Tools::Package* package = nullptr);
[[nodiscard]] Result findSystemClangFormat(Console& console, StringView wantedMajorVersion, String& foundPath);
} // namespace Tools

} // namespace SC
