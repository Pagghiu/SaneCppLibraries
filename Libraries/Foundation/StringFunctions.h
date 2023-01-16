// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "StringView.h"

namespace SC
{
template <typename StringIterator>
struct StringFunctions;
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
};
