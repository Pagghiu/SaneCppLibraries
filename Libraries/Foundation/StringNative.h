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

// Allows obtaining a null terminated char pointer to use with OS native api
template <int N>
struct SC::StringNative
{
    void clear() { (void)buffer.data.resizeWithoutInitializing(0); }

    [[nodiscard]] bool convertNullTerminateFastPath(StringView input, StringView& encodedText)
    {
        SC_TRY_IF(buffer.data.resizeWithoutInitializing(0));
        SC_TRY_IF(internalAppend(input, false, encodedText));
        return true;
    }

    /// Appends the input string null terminated
    [[nodiscard]] bool appendNullTerminated(StringView input)
    {
        SC_TRY_IF(buffer.popNulltermIfExists());
        StringView encodedText;
        return internalAppend(input, true, encodedText);
    }

    [[nodiscard]] bool setTextLengthInBytesIncludingTerminator(size_t newDataSize)
    {
        const auto zeroSize = StringEncodingGetSize(buffer.getEncoding());
        if (newDataSize >= zeroSize)
        {
            const bool res = buffer.data.resizeWithoutInitializing(newDataSize - zeroSize);
            return res && buffer.data.resize(newDataSize, 0); // Adds the null terminator
        }
        return true;
    }

    StringView view() const { return buffer.view(); }
#if SC_PLATFORM_WINDOWS
    typedef wchar_t CharType;
    SmallString<N>  buffer = StringEncoding::Utf16;
#else
    typedef char   CharType;
    SmallString<N> buffer = StringEncoding::Utf8;
#endif
  private:
    /// Appends the input string null terminated
    [[nodiscard]] bool internalAppend(StringView input, bool forceCopy, StringView& encodedText)
    {
#if SC_PLATFORM_WINDOWS
        SC_TRY_IF(StringConverter::toNullTerminatedUTF16(input, buffer.data, encodedText, forceCopy));
#else
        SC_TRY_IF(StringConverter::toNullTerminatedUTF8(input, buffer.data, encodedText, forceCopy));
#endif
        return true;
    }
};
