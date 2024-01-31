// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Containers/Vector.h"
#include "../Strings/StringFormat.h"
#include "../Strings/StringView.h"

namespace SC
{
struct String;

//! @addtogroup group_system
//! @{

/// @brief Writes to console using SC::StringFormat.
///
/// Example:
/// @code{.cpp}
/// // Create a buffer used for UTF conversions (if necessary)
/// SmallVector<char, 512 * sizeof(native_char_t)> consoleConversionBuffer;
/// // Construct console with the buffer
/// String str = StringView("Test Test\n");
/// // Have fun printing
/// console.print(str.view());
/// @endcode
struct SC_COMPILER_EXPORT Console
{
    /// @brief Constructs a console with a conversion buffer used for string conversions (UTF8 / UTF16)
    /// @param encodingConversionBuffer The buffer used for UTF conversions
    Console(Vector<char>& encodingConversionBuffer);

    /// @brief Prints a formatted string using SC::StringFormat
    /// @tparam Types Types of `args`
    /// @param fmt Format string
    /// @param args Arguments to be formatted in the string
    /// @return `true` if message has been printed successfully to Console
    template <typename... Types>
    bool print(StringView fmt, Types&&... args)
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

    /// @brief Prints a StringView to console
    /// @param str The StringView to print
    void print(const StringView str);

    /// @brief Prints a StringView to console and adds a newline at the end of it
    /// @param str The StringView to print
    void printLine(const StringView str);

  private:
    Vector<char>& encodingConversionBuffer;
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
