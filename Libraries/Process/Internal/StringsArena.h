// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

namespace SC
{
struct StringsArena;
}

/// @brief Appends a variable number of null-terminated StringView in asingle String by keeping track of their starts
struct SC::StringsArena
{
    String&      bufferString;    // The underlying buffer / arena where strings are written to
    size_t&      numberOfStrings; // Counts number of arguments
    Span<size_t> stringsStart; // Tracking start of each string in the table, its size is used as max number of elements

    [[nodiscard]] Result appendMultipleStrings(Span<const StringView> strings)
    {
        for (const StringView string : strings)
        {
            SC_TRY(appendAsSingleString(string));
        }
        return Result(true);
    }

    // Appends a single string by joining multiple StringView together
    [[nodiscard]] Result appendAsSingleString(Span<const StringView> strings)
    {
        if (numberOfStrings >= stringsStart.sizeInElements())
        {
            return Result::Error("StringTable::append exceeded MAX_STRINGS");
        }
        StringConverter converter(bufferString, StringConverter::DoNotClear);
        stringsStart[numberOfStrings] = bufferString.sizeInBytesIncludingTerminator();
        for (size_t idx = 0; idx < strings.sizeInElements(); ++idx)
        {
            SC_TRY(converter.appendNullTerminated(strings[idx], idx > 0)); // idx > 0 == popExistingNullTerminator
        }
        numberOfStrings++;
        return Result(true);
    }

    [[nodiscard]] Result writeTo(Span<StringView> strings) const
    {
        const char* tableStart = bufferString.bytesIncludingTerminator();
        if (strings.sizeInElements() < numberOfStrings)
        {
            return Result::Error("StringTable::writeTo insufficient destination span");
        }
        const auto sizeOfZero = StringEncodingGetSize(bufferString.getEncoding());
        for (size_t idx = 0; idx < numberOfStrings; ++idx)
        {
            const size_t lengthInBytes =
                idx + 1 < numberOfStrings ? stringsStart[idx + 1] : bufferString.sizeInBytesIncludingTerminator();
            strings[idx] = StringView({tableStart + stringsStart[idx], lengthInBytes - stringsStart[idx] - sizeOfZero},
                                      true, bufferString.getEncoding());
        }
        return Result(true);
    }
};
