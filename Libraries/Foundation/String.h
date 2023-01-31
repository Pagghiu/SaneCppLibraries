// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "StringFormat.h"
#include "StringView.h"
#include "Vector.h"

namespace SC
{
struct String;
}
struct SC::String
{
    String() = default;
    String(Vector<char_t>&& otherData, StringEncoding encoding)
        : data(forward<Vector<char_t>>(otherData)), encoding(encoding)
    {}
    String(StringView sv) { SC_RELEASE_ASSERT(assignStringView(sv)); }

    String& operator=(StringView sv)
    {
        data.clear();
        SC_RELEASE_ASSERT(assignStringView(sv));
        return *this;
    }
    [[nodiscard]] size_t        sizeInBytesIncludingTerminator() const { return data.size(); }
    [[nodiscard]] const char_t* bytesIncludingTerminator() const { return data.data(); }
    [[nodiscard]] bool          isEmpty() const { return data.isEmpty(); }

    [[nodiscard]] StringView view() const
    {
        if (data.isEmpty())
        {
            return StringView(nullptr, 0, false, encoding);
        }
        else
        {
            int sizeOfZero = 1;
            if (encoding == StringEncoding::Utf16)
                sizeOfZero = 2;
            else if (encoding == StringEncoding::Utf32)
                sizeOfZero = 4;
            return StringView(data.items, data.size() - sizeOfZero, true, encoding);
        }
    }

    [[nodiscard]] bool operator==(const String& other) const { return view() == (other.view()); }
    [[nodiscard]] bool operator!=(const String& other) const { return not operator==(other); }
    [[nodiscard]] bool operator==(const StringView other) const { return view() == (other); }
    [[nodiscard]] bool operator!=(const StringView other) const { return not operator==(other); }
    [[nodiscard]] bool operator<(const StringView other) const { return view() < other; }

    Vector<char_t> data;

  private:
    StringEncoding     encoding = StringEncoding::Utf8;
    [[nodiscard]] bool assignStringView(StringView sv);
    // Invariants:
    //  - if string is empty        --> data.size() == 0
    //  - if string is not empty    --> data.size() > 2
};

template <>
struct SC::StringFormatterFor<SC::String>
{
    static bool format(StringFormatOutput& data, const StringIteratorASCII specifier, const String& value);
};
