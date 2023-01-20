// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
//
// Path - Parse filesystem paths for windows and posix
#pragma once
#include "../Foundation/Result.h"
#include "../Foundation/SmallVector.h"
#include "../Foundation/StringFunctions.h"
#include "../Foundation/StringIterator.h"
#include "../Foundation/StringView.h"
#include "StringIterator.h"

namespace SC
{
struct Path;
struct PathView;
} // namespace SC

// Holds parsing windows and posix path and name/extension pairs
struct SC::PathView
{
    enum Type
    {
        TypeInvalid = 0, // Path is not valid (parsing failed)
        TypeWindows,     // Path is windows type
        TypePosix        // Path is a posix type
    };
    bool endsWithSeparator = false;

    Type       type = TypeInvalid; // Indicates if this is a windows or posix path
    StringView root;               // Ex. "C:\\" on windows - "/" on posix
    StringView directory;          // Ex. "C:\\dir" on windows - "/dir" on posix
    StringView base;               // Ex. "base" for "C:\\dir\\base" on windows or "/dir/base" on posix
    StringView name;               // Ex. "name" for "C:\\dir\\name.ext" on windows or "/dir/name.ext" on posix
    StringView ext;                // Ex. "ext" for "C:\\dir\\name.ext" on windows or "/dir/name.ext" on posix

    /// Parses all components on windows input path
    /// @param input A path in windows form (ex "C:\\directory\name.ext")
    /// @returns false if both name and extension will be empty after parsing or if parsing name/extension fails
    [[nodiscard]] bool parseWindows(StringView input);

    /// Parses all components on posix input path
    /// @param input A path in posix form (ex "/directory/name.ext")
    /// @returns false if both name and extension will be empty after parsing or if parsing name/extension fails
    [[nodiscard]] bool parsePosix(StringView input);
};

// Allow parsing windows and posix path and name/extension pairs
struct SC::Path
{
    /// Splits a StringView of type "name.ext" into "name" and "ext"
    /// @param[in] input        An input path coded as UTF8 sequence (ex. "name.ext")
    /// @param[out] name         Output string holding name ("name" in "name.ext")
    /// @param[out] extension    Output string holding extension ("ext" in "name.ext")
    /// @returns false if both name and extension will be empty after trying to parse them
    [[nodiscard]] static bool parseNameExtension(const StringView input, StringView& name, StringView& extension);

    /// Splits a Posix or Windows path of type "/usr/dir/base" into root=/ - directory=/usr/dir - base=base
    /// (or "C:\\directory\\base" into root=C:\\ - directory=C:\\directory\\ - base=base)
    [[nodiscard]] static Error parse(StringView input, PathView& pathView);

  private:
    friend struct PathView;
    struct Internal;
};
