// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "String.h"
#include "StringConverter.h"

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
    StringBuilder(Vector<char>& stringData, StringEncoding encoding, Flags f = DoNotClear);
    StringBuilder(String& str, Flags f = DoNotClear);

    template <typename... Types>
    [[nodiscard]] bool format(StringView fmt, Types&&... args)
    {
        clear();
        return append(fmt, forward<Types>(args)...);
    }

    template <typename... Types>
    [[nodiscard]] bool append(StringView fmt, Types&&... args)
    {
        SC_TRY_IF(StringConverter::popNulltermIfExists(stringData, encoding));
        StringFormatOutput sfo(encoding);
        sfo.redirectToBuffer(stringData);
        if (fmt.getEncoding() == StringEncoding::Ascii || fmt.getEncoding() == StringEncoding::Utf8)
        {
            // It's ok parsing format string '{' and '}' both for utf8 and ascii with StringIteratorASCII
            // because on a valid UTF8 string, these chars are unambiguously recognizable
            return StringFormat<StringIteratorASCII>::format(sfo, fmt, forward<Types>(args)...);
        }
        return false; // UTF16/32 format strings are not supported
    }

    [[nodiscard]] bool format(StringView text);

    [[nodiscard]] bool append(StringView str);

    [[nodiscard]] bool appendReplaceAll(StringView source, StringView occurencesOf, StringView with);

    [[nodiscard]] bool appendReplaceMultiple(StringView source, Span<const StringView[2]> substitutions);

    [[nodiscard]] bool appendHex(SpanVoid<const void> data);

  private:
    StringView view() const;

    void clear();

    Vector<char>&  stringData;
    StringEncoding encoding;
};
} // namespace SC
