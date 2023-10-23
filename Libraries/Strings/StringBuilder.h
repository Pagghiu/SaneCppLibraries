// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Strings/StringConverter.h" // popNulltermIfExists
#include "../Strings/StringFormat.h"
namespace SC
{
struct String;
struct StringBuilder
{
    enum Flags
    {
        Clear,
        DoNotClear
    };
    StringBuilder(Vector<char>& stringData, StringEncoding encoding, Flags flags = DoNotClear);
    StringBuilder(String& str, Flags flags = DoNotClear);

    template <typename... Types>
    [[nodiscard]] bool format(StringView fmt, Types&&... args);

    template <typename... Types>
    [[nodiscard]] bool append(StringView fmt, Types&&... args);

    [[nodiscard]] bool format(StringView text);

    [[nodiscard]] bool append(StringView str);

    [[nodiscard]] bool appendReplaceAll(StringView source, StringView occurencesOf, StringView with);

    [[nodiscard]] bool appendReplaceMultiple(StringView source, Span<const StringView[2]> substitutions);

    [[nodiscard]] bool appendHex(Span<const uint8_t> data);

  private:
    void clear();

    Vector<char>&  stringData;
    StringEncoding encoding;
};
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
