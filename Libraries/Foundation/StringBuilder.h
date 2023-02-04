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
    StringBuilder(String& backingString) : backingString(backingString) {}

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
        StringFormatOutput sfo;
        sfo.encoding       = backingString.getEncoding();
        sfo.data           = move(backingString.data);
        const bool res     = StringFormat<StringIteratorASCII>::format(sfo, fmt, args...);
        backingString.data = move(sfo.data);
        return res;
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

    void reset() { (void)backingString.data.resizeWithoutInitializing(0); }

    [[nodiscard]] const String& getResultString() { return backingString; }

  private:
    String& backingString;
};
} // namespace SC
