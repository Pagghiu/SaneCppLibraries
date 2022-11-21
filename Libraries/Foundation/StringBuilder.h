// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "String.h"
#include "StringFormat.h"
#include "Vector.h"

namespace SC
{
struct StringBuilder
{
    [[nodiscard]] size_t        sizeInBytesIncludingTerminator() const { return data.size(); }
    [[nodiscard]] const char_t* bytesIncludingTerminator() const { return data.data(); }

    template <typename... Types>
    [[nodiscard]] bool appendFormatASCII(StringView fmt, Types... args)
    {
        if (not data.isEmpty())
            SC_TRY_IF(data.pop_back());
        return StringFormat<text::StringIteratorASCII>::format(data, fmt, args...);
    }

    [[nodiscard]] bool append(StringView str)
    {
        if (not data.isEmpty())
            SC_TRY_IF(data.pop_back());
        if (str.isNullTerminated())
        {
            return data.appendCopy(str.bytesIncludingTerminator(), str.sizeInBytesIncludingTerminator());
        }
        else
        {
            SC_TRY_IF(data.appendCopy(str.bytesWithoutTerminator(), str.sizeInBytesWithoutTerminator()));
            SC_TRY_IF(data.push_back(0));
            return true;
        }
    }
    [[nodiscard]] bool append(const String& str)
    {
        if (not data.isEmpty())
            SC_TRY_IF(data.pop_back());
        return data.appendCopy(str.bytesIncludingTerminator(), str.sizeInBytesIncludingTerminator());
    }
    [[nodiscard]] String         toString() { return move(data); }
    [[nodiscard]] Vector<char_t> toVectorOfChars() { return move(data); }

  private:
    Vector<char_t> data;

    [[nodiscard]] bool assignStringView(StringView sv);
    [[nodiscard]] bool snprintf(bool append, const char_t* fmt, ...) SC_ATTRIBUTE_PRINTF(3, 4);
};
} // namespace SC
