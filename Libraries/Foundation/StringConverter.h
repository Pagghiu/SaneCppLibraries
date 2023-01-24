// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "StringView.h"
#include "Vector.h"

namespace SC
{
struct StringConverter;
}

struct SC::StringConverter
{
    /// Converts text to null terminated utf8 sequence. Uses the passed in buffer if necessary.
    [[nodiscard]] static bool toNullTerminatedUTF8(StringView text, Vector<char_t>& buffer,
                                                   const char_t** nullTerminatedUTF8);

    /// Converts text to null terminated utf16 sequence. Uses the passed in buffer if necessary.
    [[nodiscard]] static bool toNullTerminatedUTF16(StringView text, Vector<char_t>& buffer,
                                                    const wchar_t** nullTerminatedUTF16);

    /// Converts text to null terminated utf8 sequence. Uses the passed in buffer if necessary.
    [[nodiscard]] static bool toNullTerminatedUTF8(StringView text, Vector<wchar_t>& buffer,
                                                   const char_t** nullTerminatedUTF8)
    {
        return toNullTerminatedUTF8(text, buffer.unsafeReinterpretAs<char_t>(), nullTerminatedUTF8);
    }

    /// Converts text to null terminated utf16 sequence. Uses the passed in buffer if necessary.
    [[nodiscard]] static bool toNullTerminatedUTF16(StringView text, Vector<wchar_t>& buffer,
                                                    const wchar_t** nullTerminatedUTF16)
    {
        return toNullTerminatedUTF16(text, buffer.unsafeReinterpretAs<char_t>(), nullTerminatedUTF16);
    }
};
