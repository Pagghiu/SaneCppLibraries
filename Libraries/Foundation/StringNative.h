// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "String.h"
#include "StringConverter.h"

namespace SC
{
template <int N>
struct StringNative;
} // namespace SC

// Allows obtaining a null terminated char pointer to use with SystemDebug native api
template <int N>
struct SC::StringNative
{
    void clear() { text.data.clearWithoutInitializing(); }

    [[nodiscard]] bool convertNullTerminateFastPath(StringView input, StringView& encodedText)
    {
        text.data.clearWithoutInitializing();
        SC_TRY_IF(internalAppend(input, false, encodedText));
        return true;
    }

    /// Appends the input string null terminated
    [[nodiscard]] bool appendNullTerminated(StringView input)
    {
        SC_TRY_IF(text.popNulltermIfExists());
        StringView encodedText;
        return internalAppend(input, true, encodedText);
    }

    [[nodiscard]] bool setTextLengthInBytesIncludingTerminator(size_t newDataSize)
    {
        const auto zeroSize = StringEncodingGetSize(text.getEncoding());
        if (newDataSize >= zeroSize)
        {
            const bool res = text.data.resizeWithoutInitializing(newDataSize - zeroSize);
            return res && text.data.resize(newDataSize, 0); // Adds the null terminator
        }
        return true;
    }

    StringView view() const { return text.view(); }

    [[nodiscard]] bool growToFullCapacity() { return text.data.resizeWithoutInitializing(text.data.capacity()); }
    SmallString<N * sizeof(utf_char_t)> text = StringEncoding::Native;

  private:
    /// Appends the input string null terminated
    [[nodiscard]] bool internalAppend(StringView input, bool forceCopy, StringView& encodedText)
    {
        return StringConverter::toNullTerminated(StringEncoding::Native, input, text.data, encodedText, forceCopy);
    }
};
