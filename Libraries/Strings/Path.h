// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Strings/StringView.h"

namespace SC
{
struct String;

//! @addtogroup group_file_system
//! @{

/// @brief Parse and compose filesystem paths for windows and posix
struct SC_COMPILER_EXPORT Path
{
    /// @brief Path type (windows or posix)
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
    /// @brief Holds the various parsed components of a path
    struct ParsedView
    {
        bool endsWithSeparator = false;

        // TODO: ParsedView::directory and base are not defined consistently

        Type       type = AsPosix; ///< Indicates if this is a windows or posix path
        StringView root;           ///< Ex. `"C:\\"` on windows - `"/"` on posix
        StringView directory;      ///< Ex. `"C:\\dir"` on windows - `"/dir"` on posix
        StringView base;           ///< Ex. `"base"` for `"C:\\dir\\base"` on windows or `"/dir/base"` on posix
        StringView name;           ///< Ex. `"name"` for `"C:\\dir\\name.ext"` on windows or `"/dir/name.ext"` on posix
        StringView ext;            ///< Ex. `"ext"` for `"C:\\dir\\name.ext"` on windows or `"/dir/name.ext"` on posix

        /// @brief Parses all components on windows input path
        ///
        /// For example:
        /// @code{.cpp}
        /// path.parseWindows("C:\\ASD\\bbb\\name.ext");
        /// path.root == "C:\\";
        /// path.directory == "C:\\ASD\\bbb";
        /// path.base == "name.ext";
        /// path.name == "name";
        /// path.ext == "ext";
        /// path.endsWithSeparator == false;
        /// @endcode
        /// @param input A path in windows form (ex "C:\\directory\name.ext")
        /// @returns false if both name and extension will be empty after parsing or if parsing name/extension fails
        [[nodiscard]] bool parseWindows(StringView input);

        /// @brief Parses all components on posix input path
        ///
        /// For example:
        ///
        /// @code{.cpp}
        /// Path::ParsedView path;
        /// path.parsePosix("/123/456");
        /// path.root == "/";
        /// path.directory == "/123";
        /// path.base == "456";
        /// path.endsWithSeparator == false;
        /// @endcode
        /// @param input A path in posix form (ex "/directory/name.ext")
        /// @returns false if both name and extension will be empty after parsing or if parsing name/extension fails
        [[nodiscard]] bool parsePosix(StringView input);
    };

    /// @brief Joins multiple StringView with a Separator into an output String
    /// @param[out] output The output string receiving the path
    /// @param[in] inputs The input paths to join
    /// @param[in] separator The separator to use. By default `/` on Posix and `\` on Windows
    /// @param[in] skipEmpty If true will skip empty entries in `inputs` Span
    /// @return true if the Path was successfully joined
    [[nodiscard]] static bool join(String& output, Span<const StringView> inputs,
                                   StringView separator = SeparatorStringView(), bool skipEmpty = false);

    /// @brief Splits a StringView of type "name.ext" into "name" and "ext"
    /// @param[in] input        An input path coded as UTF8 sequence (ex. "name.ext")
    /// @param[out] name         Output string holding name ("name" in "name.ext")
    /// @param[out] extension    Output string holding extension ("ext" in "name.ext")
    /// @returns `false` if both name and extension will be empty after trying to parse them
    ///
    /// Example:
    /// \snippet Tests/Libraries/FileSystem/PathTest.cpp parseNameExtensionSnippet
    [[nodiscard]] static bool parseNameExtension(const StringView input, StringView& name, StringView& extension);

    /// @brief Splits a Posix or Windows path into a ParsedView.
    /// @param[in] input The StringView with path to be parsed
    /// @param[out] pathView The output parsed ParsedView with all components of path
    /// @param[in] type Specify to parse as Windows or Posix path
    /// @return `true` if path was parsed successfully
    [[nodiscard]] static bool parse(StringView input, Path::ParsedView& pathView, Type type);

    /// @brief Returns the directory name of a path. Trailing separators are ignored.
    ///
    /// For example:
    ///
    /// @code{.cpp}
    /// Path::dirname("/dirname/basename", Path::AsPosix) == "/dirname";
    /// Path::dirname("/dirname/basename//", Path::AsPosix) == "/dirname";
    /// Path::dirname("C:\\dirname\\basename", Path::AsWindows) == "C:\\dirname";
    /// Path::dirname("\\dirname\\basename\\\\", Path::AsWindows) == "\\dirname";
    /// @endcode
    /// @param[in] input The StringView with path to be parsed. Trailing separators are ignored.
    /// @param[in] type Specify to parse as Windows or Posix path
    /// @param repeat how many directory levels should be removed `dirname("/1/2/3/4", repeat=1) == "/1/2"`
    /// @return Substring of `input` holding the directory name
    [[nodiscard]] static StringView dirname(StringView input, Type type, int repeat = 0);

    /// @brief Returns the base name of a path. Trailing separators are ignored.
    ///
    /// For example:
    ///
    /// @code{.cpp}
    /// Path::basename("/a/basename", Path::AsPosix) == "basename";
    /// Path::basename("/a/basename//", Path::AsPosix) == "basename";
    /// @endcode
    /// @param[in] input The StringView with path to be parsed. Trailing separators are ignored.
    /// @param[in] type Specify to parse as Windows or Posix path
    /// @return Substring of `input` holding the base name
    [[nodiscard]] static StringView basename(StringView input, Type type);

    /// @brief Returns the base name of a path. Suffix is stripped if existing. Trailing separators are ignored.
    ///
    /// For example:
    ///
    /// @code{.cpp}
    /// Path::basename("/a/basename.html", ".html") == "basename";
    /// @endcode
    /// @param[in] input The StringView with path to be parsed. Trailing separators are ignored.
    /// @param[in] suffix The StringView extension (or suffix in general) to strip if existing.
    /// @return Substring of `input` holding the base name
    [[nodiscard]] static StringView basename(StringView input, StringView suffix);

    /// @brief Checks if a path is absolute.
    ///
    /// For example:
    ///
    /// @code{.cpp}
    /// Path::isAbsolute("/dirname/basename", Path::AsPosix) == true;        // Posix Absolute
    /// Path::isAbsolute("./dirname/basename", Path::AsPosix) == false;      // Posix Relative
    /// Path::isAbsolute("C:\\dirname\\basename", Path::AsWindows) == true;  // Windows with Drive
    /// Path::isAbsolute("\\\\server\\dir", Path::AsWindows) == true;        // Windows with Network
    /// Path::isAbsolute("\\\\?\\C:\\server\\dir", Path::AsWindows) == true; // Windows with Long
    /// Path::isAbsolute("..\\dirname\\basename", Path::AsWindows) == false; // Windows relative
    /// @endcode
    /// @param[in] input The StringView with path to be parsed. Trailing separators are ignored.
    /// @param[in] type Specify to parse as Windows or Posix path
    /// @return `true` if `input` is absolute
    [[nodiscard]] static bool isAbsolute(StringView input, Type type);

    struct Windows
    {
        static const char Separator = '\\';

        [[nodiscard]] static constexpr StringView SeparatorStringView() { return "\\"_a8; };
    };

    struct Posix
    {
        static const char Separator = '/';

        [[nodiscard]] static constexpr StringView SeparatorStringView() { return "/"_a8; }
    };

/// Path separator char for current platform
#if SC_PLATFORM_WINDOWS
    static constexpr char Separator = '\\';
#else
    static constexpr char Separator = '/';
#endif

/// Path separator StringView for current platform
#if SC_PLATFORM_WINDOWS
    [[nodiscard]] static constexpr StringView SeparatorStringView() { return "\\"_a8; };
#else
    [[nodiscard]] static constexpr StringView SeparatorStringView() { return "/"_a8; }
#endif

    /// @brief Resolves all `..` to output a normalized path String
    ///
    /// For example:
    ///
    /// @code{.cpp}
    /// Path::normalize("/Users/SC/../Documents/", &path, Path::AsPosix);
    /// SC_RELEASE_ASSERT(path == "/Users/Documents");
    /// @endcode
    /// @param[out] output Reference to String that will receive the normalized Path
    /// @param view The path to be normalized (but it should not be a view() of the output String)
    /// @param type Specify to parse as Windows or Posix path
    /// @return `true` if the Path was successfully parsed and normalized
    template <int numComponents = 64>
    [[nodiscard]] static bool normalize(String& output, StringView view, Type type)
    {
        StringView components[numComponents];
        return normalize(output, view, type, components);
    }

    /// @brief Resolves all `..` to output a normalized path String (allows custom span of components)
    [[nodiscard]] static bool normalize(String& output, StringView view, Type type, Span<StringView> components);

    /// @brief Get relative path that appended to `source` resolves to `destination`.
    ///
    /// For example:
    ///
    /// @code{.cpp}
    /// Path::relativeFromTo("/a/b/1/2/3", "/a/b/d/e", path, Path::AsPosix, Path::AsPosix);
    /// SC_TEST_ASSERT(path == "../../../d/e");
    /// @endcode
    /// @param[in] source The source Path
    /// @param[in] destination The destination Path
    /// @param[out] output The output relative path computed that transforms source into destination
    /// @param[in] type Specify to parse as Windows or Posix path
    /// @param[in] outputType Specify if the output relative path should be formatted as a Posix or Windows path
    /// @return `true` if source and destination paths can be properly parsed as absolute paths
    [[nodiscard]] static bool relativeFromTo(StringView source, StringView destination, String& output, Type type,
                                             Type outputType = AsNative);

    /// @brief Append to an existing path a series of StringView with a separator
    /// @param[out] output The destination string containing the existing path, that will be extended
    /// @param[in] paths The path components to join, appended to `output`
    /// @param[in] inputType Specify to append as Windows or Posix path components
    /// @return `true` if the `output` path can joined properly
    [[nodiscard]] static bool append(String& output, Span<const StringView> paths, Type inputType);

    /// @brief Check if the path ends with a Windows or Posix separator
    /// @param path The path to check
    /// @return `true` if path ends with a separator
    [[nodiscard]] static bool endsWithSeparator(StringView path);

    /// @brief Return a path without its (potential) starting separator
    /// @param path The path to use
    /// @return A StringView without its initial separator
    [[nodiscard]] static StringView removeStartingSeparator(StringView path);

    /// @brief An extended Path::normalize handling a bug with incorrect __FILE__ backslash escape on Windows when
    /// using UNC Paths, and also removing quote characters '"' added when passing such paths to compiler command line.
    [[nodiscard]] static bool normalizeUNCAndTrimQuotes(String& outputPath, StringView fileLocation, Type type,
                                                        Span<StringView> components);

  private:
    [[nodiscard]] static bool appendTrailingSeparator(String& path, Type type);

    [[nodiscard]] static StringView removeTrailingSeparator(StringView path);
    struct Internal;
};

//! @}
} // namespace SC
