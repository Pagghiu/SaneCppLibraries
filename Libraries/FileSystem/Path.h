// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
//
// Path - Parse filesystem paths for windows and posix
#pragma once
#include "../Foundation/StringView.h"

namespace SC
{
struct Path;
struct String;
template <typename T>
struct Vector;
} // namespace SC

struct SC::Path
{
    enum Type
    {
        AsPosix,
        AsWindows,
#if SC_PLATFORM_WINDOWS
        AsNative = AsWindows
#else
        AsNative = AsPosix
#endif
    };
    // Allow parsing windows and posix path and name/extension pairs
    struct ParsedView
    {
        bool endsWithSeparator = false;

        // TODO: ParsedView::directory and base are not defined consistently

        Type       type = AsPosix; // Indicates if this is a windows or posix path
        StringView root;           // Ex. "C:\\" on windows - "/" on posix
        StringView directory;      // Ex. "C:\\dir" on windows - "/dir" on posix
        StringView base;           // Ex. "base" for "C:\\dir\\base" on windows or "/dir/base" on posix
        StringView name;           // Ex. "name" for "C:\\dir\\name.ext" on windows or "/dir/name.ext" on posix
        StringView ext;            // Ex. "ext" for "C:\\dir\\name.ext" on windows or "/dir/name.ext" on posix

        /// Parses all components on windows input path
        /// @param input A path in windows form (ex "C:\\directory\name.ext")
        /// @returns false if both name and extension will be empty after parsing or if parsing name/extension fails
        [[nodiscard]] bool parseWindows(StringView input);

        /// Parses all components on posix input path
        /// @param input A path in posix form (ex "/directory/name.ext")
        /// @returns false if both name and extension will be empty after parsing or if parsing name/extension fails
        [[nodiscard]] bool parsePosix(StringView input);
    };

    [[nodiscard]] static bool join(String& output, Span<const StringView> inputs,
                                   StringView separator = SeparatorStringView());
    /// Splits a StringView of type "name.ext" into "name" and "ext"
    /// @param[in] input        An input path coded as UTF8 sequence (ex. "name.ext")
    /// @param[out] name         Output string holding name ("name" in "name.ext")
    /// @param[out] extension    Output string holding extension ("ext" in "name.ext")
    /// @returns false if both name and extension will be empty after trying to parse them
    [[nodiscard]] static bool parseNameExtension(const StringView input, StringView& name, StringView& extension);

    /// Splits a Posix or Windows path of type "/usr/dir/base" into root=/ - directory=/usr/dir - base=base
    /// (or "C:\\directory\\base" into root=C:\\ - directory=C:\\directory\\ - base=base)
    [[nodiscard]] static bool parse(StringView input, Path::ParsedView& pathView, Type type);

    /// Return the directory name of a path. Trailing spearators are ignored.
    [[nodiscard]] static StringView dirname(StringView input);
    /// Return the base name of a path. Trailing spearators are ignored.
    [[nodiscard]] static StringView basename(StringView input);
    /// Return the base name of a path. Suffix is stripped if existing. Trailing spearators are ignored.
    [[nodiscard]] static StringView basename(StringView input, StringView suffix);
    /// Returns true if path is an absolute native path (depending on platform)
    [[nodiscard]] static bool isAbsolute(StringView input, Type type);

    struct Windows
    {
        static const char                         Separator = '\\';
        [[nodiscard]] static constexpr StringView SeparatorStringView() { return "\\"_a8; };
        /// Return the directory name of a path. Trailing spearators are ignored.
        [[nodiscard]] static StringView dirname(StringView input);
        /// Return the base name of a path. Trailing spearators are ignored.
        [[nodiscard]] static StringView basename(StringView input);
        /// Return the base name of a path. Suffix is stripped if existing. Trailing spearators are ignored.
        [[nodiscard]] static StringView basename(StringView input, StringView suffix);
        /// Returns true if path is an absolute Windows path (Starts with drive letter + backslatsh or double backslash)
        [[nodiscard]] static bool isAbsolute(StringView input);
    };
    struct Posix
    {
        static const char                         Separator = '/';
        [[nodiscard]] static constexpr StringView SeparatorStringView() { return "/"_a8; }
        /// Return the directory name of a path. Trailing spearators are ignored.
        [[nodiscard]] static StringView dirname(StringView input);
        /// Return the base name of a path. Trailing spearators are ignored.
        [[nodiscard]] static StringView basename(StringView input);
        /// Return the base name of a path. Suffix is stripped if existing. Trailing spearators are ignored.
        [[nodiscard]] static StringView basename(StringView input, StringView suffix);
        /// Returns true if path is an absolute Posix path (starts with /)
        [[nodiscard]] static bool isAbsolute(StringView input);
    };
#if SC_PLATFORM_WINDOWS
    static constexpr char                     Separator = '\\';
    [[nodiscard]] static constexpr StringView SeparatorStringView() { return "\\"_a8; };
#else
    static constexpr char Separator = '/';
    [[nodiscard]] static constexpr StringView SeparatorStringView() { return "/"_a8; }
#endif
    [[nodiscard]] static bool extractDirectoryFromFILE(StringView fileLocation, String& outputPath,
                                                       Vector<StringView>& components);

    [[nodiscard]] static bool normalize(StringView view, Vector<StringView>& components, String* output, Type type);

    [[nodiscard]] static bool relativeFromTo(StringView source, StringView destination, String& output, Type type);

    [[nodiscard]] static bool append(String& output, Span<const StringView> paths, Type type);

    [[nodiscard]] static bool endsWithSeparator(StringView path);

    [[nodiscard]] static bool appendTrailingSeparator(String& path, Type type);

    [[nodiscard]] static StringView removeTrailingSeparator(StringView path);

  private:
    struct Internal;
};
