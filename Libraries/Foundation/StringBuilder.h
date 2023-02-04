// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "String.h"
#include "StringConverter.h"
#include "StringFormat.h"

namespace SC
{
struct StringBuilder
{
    StringBuilder(String& backingString, Vector<char>* temporaryBuffer = nullptr)
        : backingString(backingString), temporaryBuffer(temporaryBuffer)
    {}

    template <typename... Types>
    [[nodiscard]] bool format(StringView fmt, Types&&... args)
    {
        reset();
        return append(fmt, forward(args)...);
    }

    [[nodiscard]] bool format(StringView text)
    {
        reset();
        return append(text);
    }

    template <typename... Types>
    [[nodiscard]] bool append(StringView fmt, Types&&... args)
    {
        SC_TRY_IF(backingString.popNulltermIfExists());
        if (temporaryBuffer)
        {
            return printWithTemporaryBuffer(*temporaryBuffer, fmt, forward(args)...);
        }
        else
        {
            SmallVector<char, 512> buffer;
            return printWithTemporaryBuffer(buffer, fmt, forward(args)...);
        }
    }

    [[nodiscard]] bool append(StringView str)
    {
        if (str.isEmpty())
            return true;
        SC_TRY_IF(backingString.popNulltermIfExists());
        StringView encodedText;
        switch (backingString.getEncoding())
        {
        case StringEncoding::Ascii:
            SC_TRY_IF(StringConverter::toNullTerminatedUTF8(str, backingString.data, encodedText, true));
            break;
        case StringEncoding::Utf8:
            SC_TRY_IF(StringConverter::toNullTerminatedUTF8(str, backingString.data, encodedText, true));
            break;
        case StringEncoding::Utf16:
            SC_TRY_IF(StringConverter::toNullTerminatedUTF16(str, backingString.data, encodedText, true));
            break;
        case StringEncoding::Utf32: return false;
        }
        return true;
    }

    [[nodiscard]] StringView view() { return backingString.view(); }

    void reset() { backingString.data.clearWithoutInitializing(); }

    [[nodiscard]] const String& getResultString() { return backingString; }

  private:
    template <typename... Types>
    [[nodiscard]] bool printWithTemporaryBuffer(Vector<char>& buffer, StringView fmt, Types&&... args)
    {
        StringFormatOutput sfo(buffer);
        sfo.encoding = backingString.getEncoding();
        sfo.redirectToBuffer(backingString.data);
        if (fmt.getEncoding() == StringEncoding::Ascii || fmt.getEncoding() == StringEncoding::Utf8)
        {
            // It's ok parsing format string '{' and '}' both for utf8 and ascii with StringIteratorASCII
            // because on a valid UTF8 string, these chars are unambiguously recognizable
            return StringFormat<StringIteratorASCII>::format(sfo, fmt, forward(args)...);
        }
        return false; // UTF16/32 format strings are not supported
    }
    String&       backingString;
    Vector<char>* temporaryBuffer;
};
} // namespace SC
