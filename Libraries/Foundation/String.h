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
    String(Vector<char_t>&& otherData) : data(forward<Vector<char_t>>(otherData)) {}
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
            return StringView();
        else
            return StringView(data.items, data.size() - 1, true);
    }

    [[nodiscard]] bool operator==(const String& other) const { return view() == (other.view()); }
    [[nodiscard]] bool operator!=(const String& other) const { return not operator==(other); }
    [[nodiscard]] bool operator==(const StringView other) const { return view() == (other); }
    [[nodiscard]] bool operator!=(const StringView other) const { return not operator==(other); }
    [[nodiscard]] bool operator<(const StringView other) const { return view() < other; }

    Vector<char_t> data;

  private:
    [[nodiscard]] bool assignStringView(StringView sv);
    // Invariants:
    //  - if string is empty        --> data.size() == 0
    //  - if string is not empty    --> data.size() > 2
};

template <>
struct SC::StringFormatterFor<SC::String>
{
    static bool format(Vector<char_t>& data, const StringIteratorASCII specifier, const String& value);
};
