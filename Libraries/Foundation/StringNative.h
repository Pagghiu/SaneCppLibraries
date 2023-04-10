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
struct SC::StringNative : public StringConverter
{
    SmallString<N * sizeof(utf_char_t)> bufferText;
    StringNative() : bufferText(StringEncoding::Native), StringConverter(bufferText) {}

    // TODO: Refactor to use String/SmallString(s) and use StringConverter at need to avoid its dangerous reference.
    StringNative(StringNative&& other) : bufferText(move(other.bufferText)), StringConverter(bufferText) {}
    StringNative(const StringNative& other) : bufferText(other.bufferText), StringConverter(bufferText) {}
    StringNative& operator=(StringNative&& other)
    {
        bufferText = move(other.bufferText);
        return *this;
    }
    StringNative& operator=(const StringNative& other)
    {
        bufferText = other.bufferText;
        return *this;
    }
};
