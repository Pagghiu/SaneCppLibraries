// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Strings/StringConverter.h" // popNulltermIfExists
#include "../Strings/StringFormat.h"
namespace SC
{
struct String;
//! @addtogroup group_strings
//! @{

/// @brief Builds String out of a sequence of StringView or formatting through StringFormat
struct StringBuilder
{
    /// @brief Clearing flags used when initializing destination buffer
    enum Flags
    {
        Clear,     ///< Destination buffer will be cleared before pushing to it
        DoNotClear ///< Destination buffer will not be cleared before pushing to it
    };
    /// @brief Create a StringBuilder that will push to given Vector, with specific encoding.
    /// @param stringData Destination buffer where code points will be pushed
    /// @param encoding The encoding to be used
    /// @param flags Specifies if destination buffer must be emptied or not before pushing
    StringBuilder(Vector<char>& stringData, StringEncoding encoding, Flags flags = DoNotClear);

    /// @brief Create a StringBuilder that will push to given String, with specific encoding.
    /// @param str Destination buffer where code points will be pushed
    /// @param flags Specifies if destination buffer must be emptied or not before pushing
    StringBuilder(String& str, Flags flags = DoNotClear);

    /// @brief Uses StringFormat to format the given StringView against args, replacing destination contents.
    /// @tparam Types Type of Args
    /// @param fmt The format strings
    /// @param args arguments to format
    /// @return `true` if format succeeded
    template <typename... Types>
    [[nodiscard]] bool format(StringView fmt, Types&&... args);

    /// @brief Uses StringFormat to format the given StringView against args, appending to destination contents.
    /// @tparam Types Type of Args
    /// @param fmt The format strings
    /// @param args arguments to format
    /// @return `true` if format succeeded
    template <typename... Types>
    [[nodiscard]] bool append(StringView fmt, Types&&... args);

    /// @brief Assigns StringView to destination buffer
    /// @param text StringView to assign to destination buffer
    /// @return `true` if assign succeeded
    [[nodiscard]] bool format(StringView text);

    /// @brief Appends StringView to destination buffer
    /// @param str StringView to append to destination buffer
    /// @return `true` if append succeeded
    [[nodiscard]] bool append(StringView str);

    /// @brief Appends source to destination buffer, replacing `occurencesOf` StringView with StringView `with`
    /// @param source The StringView to be appended
    /// @param occurencesOf The StringView to be searched inside `source`
    /// @param with The replacement StringView to be written in destination uffer
    /// @return `true` if append succeeded
    [[nodiscard]] bool appendReplaceAll(StringView source, StringView occurencesOf, StringView with);

    /// @brief Appends source to destination buffer, replacing multiple substitutions pairs
    /// @param source The StringView to be appended
    /// @param substitutions For each substitution in the span, the first is searched and replaced with the second.
    /// @return `true` if append succeeded
    [[nodiscard]] bool appendReplaceMultiple(StringView source, Span<const StringView[2]> substitutions);

    /// @brief Appends given binary data escaping it as hexadecimal ASCII characters
    /// @param data Binary data to append to destination buffer
    /// @return `true` if append succeeded
    [[nodiscard]] bool appendHex(Span<const uint8_t> data);

  private:
    void clear();

    Vector<char>&  stringData;
    StringEncoding encoding;
};
//! @}

} // namespace SC

//-----------------------------------------------------------------------------------------------------------------------
// Implementations Details
//-----------------------------------------------------------------------------------------------------------------------
template <typename... Types>
inline bool SC::StringBuilder::format(StringView fmt, Types&&... args)
{
    clear();
    return append(fmt, forward<Types>(args)...);
}

template <typename... Types>
inline bool SC::StringBuilder::append(StringView fmt, Types&&... args)
{
    if (not StringConverter::popNulltermIfExists(stringData, encoding))
    {
        return false;
    }

    StringFormatOutput sfo(encoding, stringData);
    if (fmt.getEncoding() == StringEncoding::Ascii || fmt.getEncoding() == StringEncoding::Utf8)
    {
        // It's ok parsing format string '{' and '}' both for utf8 and ascii with StringIteratorASCII
        // because on a valid UTF8 string, these chars are unambiguously recognizable
        return StringFormat<StringIteratorASCII>::format(sfo, fmt, forward<Types>(args)...);
    }
    return false; // UTF16/32 format strings are not supported
}
