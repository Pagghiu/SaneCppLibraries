// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "StringView.h"
#include "Vector.h"

namespace SC
{
struct StringConverter;
struct String;
} // namespace SC

struct SC::StringConverter
{
    enum NullTermination
    {
        AddZeroTerminator,
        DoNotAddZeroTerminator
    };
    /// Converts text to null terminated utf8 sequence. Uses the passed in buffer if necessary.
    [[nodiscard]] static bool convertEncodingToUTF8(StringView text, Vector<char>& buffer,
                                                    StringView*     encodedText   = nullptr,
                                                    NullTermination nullTerminate = AddZeroTerminator);

    /// Converts text to null terminated utf16 sequence. Uses the passed in buffer if necessary.
    [[nodiscard]] static bool convertEncodingToUTF16(StringView text, Vector<char>& buffer,
                                                     StringView*     encodedText   = nullptr,
                                                     NullTermination nullTerminate = AddZeroTerminator);

    /// Converts to one of the supported encoding
    [[nodiscard]] static bool convertEncodingTo(StringEncoding encoding, StringView text, Vector<char>& buffer,
                                                StringView*     encodedText   = nullptr,
                                                NullTermination nullTerminate = AddZeroTerminator);

    StringConverter(String& text) : text(text) {}

    void clear();

    [[nodiscard]] bool convertNullTerminateFastPath(StringView input, StringView& encodedText);

    /// Appends the input string null terminated
    [[nodiscard]] bool appendNullTerminated(StringView input);

    [[nodiscard]] bool setTextLengthInBytesIncludingTerminator(size_t newDataSize);

    [[nodiscard]] StringView view() const;

    [[nodiscard]] bool growToFullCapacity();

    String& text;

  private:
    /// Appends the input string null terminated
    [[nodiscard]] bool internalAppend(StringView input, StringView* encodedText);
};
