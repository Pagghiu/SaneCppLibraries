// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/Result.h"
#include "../../Foundation/StringSpan.h"

namespace SC
{
/// @brief Appends a variable number of null-terminated StringSpan in asingle String by keeping track of their starts
struct StringsArena
{
    Result appendMultipleStrings(Span<const StringSpan> strings)
    {
        for (const StringSpan item : strings)
        {
            SC_TRY(appendAsSingleString(item));
        }
        return Result(true);
    }

    // Appends a single string by joining multiple StringSpan together
    Result appendAsSingleString(Span<const StringSpan> strings)
    {
        if (numberOfStrings >= stringsStart.sizeInElements())
        {
            return Result::Error("StringTable::append exceeded MAX_STRINGS");
        }
        stringsStart[numberOfStrings] = view().sizeInBytesIncludingTerminator();
        for (size_t idx = 0; idx < strings.sizeInElements(); ++idx)
        {
            SC_TRY(strings[idx].appendNullTerminatedTo(string, idx > 0)); // idx > 0 == removePreviousNullTerminator
        }
        numberOfStrings++;
        return Result(true);
    }

    Result writeTo(Span<StringSpan> strings) const
    {
        const char* tableStart = view().bytesIncludingTerminator();
        if (strings.sizeInElements() < numberOfStrings)
        {
            return Result::Error("StringTable::writeTo insufficient destination span");
        }
        const auto sizeOfZero = StringEncodingGetSize(view().getEncoding());
        for (size_t idx = 0; idx < numberOfStrings; ++idx)
        {
            const size_t lengthInBytes =
                idx + 1 < numberOfStrings ? stringsStart[idx + 1] : view().sizeInBytesIncludingTerminator();
            strings[idx] = StringSpan({tableStart + stringsStart[idx], lengthInBytes - stringsStart[idx] - sizeOfZero},
                                      true, view().getEncoding());
        }
        return Result(true);
    }

    StringsArena(StringSpan::NativeWritable& string, size_t& numberOfStrings, Span<size_t> stringsStart)
        : numberOfStrings(numberOfStrings), stringsStart(stringsStart), string(string)
    {}

    StringSpan view() const { return string.view(); }

    size_t&      numberOfStrings; // Counts number of arguments
    Span<size_t> stringsStart; // Tracking start of each string in the table, its size is used as max number of elements
  private:
    StringSpan::NativeWritable& string; // The underlying buffer / arena where strings are written to
};

} // namespace SC
