// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "StringView.h"

namespace SC
{
template <typename StringIterator>
struct StringFunctions;

struct SplitOptions
{

    enum Value
    {
        None          = 0,
        SkipEmpty     = 1,
        SkipSeparator = 2
    };
    Value value;
    SplitOptions(std::initializer_list<Value> ilist)
    {
        value = None;
        for (auto v : ilist)
        {
            value = static_cast<Value>(static_cast<uint32_t>(value) | static_cast<uint32_t>(v));
        }
    }
    bool has(Value v) const { return (value & v) != None; }
};
} // namespace SC

template <typename StringIterator>
struct SC::StringFunctions
{
    const StringView sv;
    StringFunctions(const StringView sv) : sv(sv) {}

    [[nodiscard]] StringView fromTo(size_t from, size_t to) const
    {
        StringIterator it = sv.getIterator<StringIterator>();
        SC_RELEASE_ASSERT(it.advanceCodePoints(from));
        StringIterator start = it;
        SC_RELEASE_ASSERT(from <= to && it.advanceCodePoints(to - from));
        const auto distance = it.bytesDistanceFrom(start);
        return StringView(start.getStart(), distance, from + distance == sv.sizeInBytesIncludingTerminator());
    }

    [[nodiscard]] StringView offsetLength(size_t offset, size_t length) const
    {
        return fromTo(offset, offset + length);
    }

    template <typename Lambda, typename CharacterType>
    [[nodiscard]] uint32_t split(CharacterType separator, Lambda lambda,
                                 SplitOptions options = {SplitOptions::SkipEmpty, SplitOptions::SkipSeparator})
    {
        if (sv.isEmpty())
            return 0;
        StringIterator it            = sv.getIterator<StringIterator>();
        StringIterator itBackup      = it;
        uint32_t       numSplits     = 0;
        bool           continueSplit = true;
        while (continueSplit)
        {
            continueSplit        = it.advanceUntilMatches(separator);
            StringView component = itBackup.viewUntil(it);
            if (options.has(SplitOptions::SkipSeparator))
            {
                (void)it.skipNext(); // No need to check return result, we already checked in advanceUntilMatches
                continueSplit = !it.isEmpty();
            }
            // directory
            if (!component.isEmpty() || !options.has(SplitOptions::SkipEmpty))
            {
                numSplits++;
                lambda(component);
            }
            itBackup = it;
        }
        return numSplits;
    }
};
