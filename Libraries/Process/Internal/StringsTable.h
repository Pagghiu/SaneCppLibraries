// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

namespace SC
{
struct StringsTable;
}

/// @brief Appends a number of null-terminated StringView in a String keeping track of their starts
/// Making this internal for now
struct SC::StringsTable
{
    String&      bufferString;
    size_t&      numberOfStrings; // Counts number of arguments (including executable name)
    Span<size_t> stringsStart;    // Tracking start of each string in the table

    [[nodiscard]] Result append(Span<const StringView> strings)
    {
        if (numberOfStrings >= stringsStart.sizeInElements())
        {
            return Result::Error("StringTable::append exceeded MAX_STRINGS");
        }
        StringConverter converter(bufferString, StringConverter::DoNotClear);
        stringsStart[numberOfStrings] = bufferString.sizeInBytesIncludingTerminator();
        for (size_t idx = 0; idx < strings.sizeInElements(); ++idx)
        {
            SC_TRY(converter.appendNullTerminated(strings[idx], idx > 0)); // 0 == false
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
        for (size_t idx = 0; idx < numberOfStrings; ++idx)
        {
            const size_t lengthInBytes =
                idx + 1 < numberOfStrings ? stringsStart[idx + 1] : bufferString.sizeInBytesIncludingTerminator();
            strings[idx] = StringView({tableStart + stringsStart[idx], lengthInBytes - stringsStart[idx]}, true,
                                      bufferString.getEncoding());
        }
        return Result(true);
    }
};
