// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "StringView.h"

namespace SC
{
template <typename T>
struct Vector;
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
    enum Flags
    {
        Clear,
        DoNotClear
    };

    StringConverter(String& text, Flags flags = DoNotClear);
    StringConverter(Vector<char>& text, StringEncoding encoding);

    [[nodiscard]] bool convertNullTerminateFastPath(StringView input, StringView& encodedText);

    /// Appends the input string null terminated
    [[nodiscard]] bool appendNullTerminated(StringView input);

    [[nodiscard]] static bool popNulltermIfExists(Vector<char>& stringData, StringEncoding encoding);

    [[nodiscard]] static bool pushNullTerm(Vector<char>& stringData, StringEncoding encoding);

    [[nodiscard]] static bool ensureZeroTermination(Vector<char>& data, StringEncoding encoding);

  private:
    void internalClear();
    // TODO: FileSystemWalker should just use a Vector<char>
    friend struct FileSystemWalker;
    [[nodiscard]] bool        setTextLengthInBytesIncludingTerminator(size_t newDataSize);
    [[nodiscard]] static bool convertSameEncoding(StringView text, Vector<char>& buffer, StringView* encodedText,
                                                  NullTermination terminate);
    static void               eventuallyNullTerminate(Vector<char>& buffer, StringEncoding destinationEncoding,
                                                      StringView* encodedText, NullTermination terminate);

    StringEncoding encoding;
    Vector<char>&  data;
    /// Appends the input string null terminated
    [[nodiscard]] bool internalAppend(StringView input, StringView* encodedText);
};
