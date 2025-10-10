// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Strings/StringFormat.h"

namespace SC
{
struct String;

//! @addtogroup group_strings
//! @{

/// @brief Writes to console using SC::StringFormat.
///
/// Example:
/// @code{.cpp}
/// // Use a custom buffer UTF conversions on windows (optional)
/// char optionalConversionBuffer[512];
/// Console console(optionalConversionBuffer);
/// String str = StringView("Test Test\n");
/// // Have fun printing
/// console.print(str.view());
/// @endcode
struct SC_COMPILER_EXPORT Console
{
    /// @brief Constructs a console with an OPTIONAL conversion buffer used for UTF encoding conversions on Windows
    /// @param conversionBuffer The optional buffer used for UTF conversions
    Console(Span<char> conversionBuffer = {});

    /// @brief Prints a formatted string using SC::StringFormat
    /// @tparam Types Types of `args`
    /// @param fmt Format string
    /// @param args Arguments to be formatted in the string
    /// @return `true` if message has been printed successfully to Console
    template <typename... Types>
    bool print(StringSpan fmt, Types&&... args)
    {
        StringFormatOutput output(fmt.getEncoding(), *this);
        if (fmt.getEncoding() == StringEncoding::Ascii || fmt.getEncoding() == StringEncoding::Utf8)
        {
            // It's ok parsing format string '{' and '}' both for utf8 and ascii with StringIteratorASCII
            // because on a valid UTF8 string, these chars are unambiguously recognizable
            return StringFormat<StringIteratorASCII>::format(output, fmt, forward<Types>(args)...);
        }
        return false; // UTF16/32 format strings are not supported
    }

    /// @brief Prints a StringSpan to console
    /// @param str The StringSpan to print
    void print(const StringSpan str);

    /// @brief Prints a StringSpan to console and adds a newline at the end of it
    /// @param str The StringSpan to print
    void printLine(const StringSpan str);

    /// @brief Flushes the console output buffer
    void flush();

    /// @brief Tries attaching current process to parent console (Windows only, has no effect elsewhere)
    /// @returns `true` if the parent console has been attached (Windows only, returns true elsewhere)
    static bool tryAttachingToParentConsole();

  private:
    Span<char> conversionBuffer;
#if SC_PLATFORM_WINDOWS
    void* handle;
    bool  isConsole  = true;
    bool  isDebugger = true;
#endif
};

//! @}

} // namespace SC
extern SC::Console* globalConsole;
#if !defined(SC_LOG_MESSAGE)
#define SC_LOG_MESSAGE(fmt, ...)                                                                                       \
    if (globalConsole)                                                                                                 \
    globalConsole->print(fmt, ##__VA_ARGS__)
#endif
