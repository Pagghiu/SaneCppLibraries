// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "String.h"
#include "StringFormat.h"

namespace SC
{
struct String;
struct StringBuilder
{
    constexpr StringBuilder(String& backingString) : backingString(backingString) {}

    template <typename... Types>
    [[nodiscard]] bool format(StringView fmt, Types&&... args)
    {
        clear();
        return append(fmt, forward<Types>(args)...);
    }

    [[nodiscard]] bool format(StringView text)
    {
        clear();
        return append(text);
    }

    template <typename... Types>
    [[nodiscard]] bool append(StringView fmt, Types&&... args)
    {
        SC_TRY_IF(backingString.popNulltermIfExists());
        StringFormatOutput sfo(backingString.getEncoding());
        sfo.redirectToBuffer(backingString.data);
        if (fmt.getEncoding() == StringEncoding::Ascii || fmt.getEncoding() == StringEncoding::Utf8)
        {
            // It's ok parsing format string '{' and '}' both for utf8 and ascii with StringIteratorASCII
            // because on a valid UTF8 string, these chars are unambiguously recognizable
            return StringFormat<StringIteratorASCII>::format(sfo, fmt, forward<Types>(args)...);
        }
        return false; // UTF16/32 format strings are not supported
    }

    [[nodiscard]] bool append(StringView str);

    [[nodiscard]] bool appendReplaceAll(StringView source, StringView occurencesOf, StringView with);

  private:
    void clear() { backingString.data.clearWithoutInitializing(); }

    String& backingString;
};
} // namespace SC
