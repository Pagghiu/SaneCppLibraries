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

template <int N>
struct SmallString;
} // namespace SC
struct SC::String
{
  private:
    [[nodiscard]] bool assignStringView(StringView sv);

  public:
    String(StringEncoding encoding = StringEncoding::Utf8) : encoding(encoding) {}
    String(StringView sv) { SC_RELEASE_ASSERT(assignStringView(sv)); }

    String& operator=(StringView sv)
    {
        data.clear();
        SC_RELEASE_ASSERT(assignStringView(sv));
        return *this;
    }

    // const methods
    [[nodiscard]] StringEncoding getEncoding() const { return encoding; }
    [[nodiscard]] size_t         sizeInBytesIncludingTerminator() const { return data.size(); }
    //  - if string is empty        --> data.size() == 0
    //  - if string is not empty    --> data.size() > 2
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
            return StringView(data.items, data.size() - StringEncodingGetSize(encoding), true, encoding);
        }
    }

    bool popNulltermIfExists()
    {
        const auto sizeOfZero = StringEncodingGetSize(encoding);
        const auto dataSize   = data.size();
        if (dataSize >= sizeOfZero)
        {
            return data.resizeWithoutInitializing(dataSize - sizeOfZero);
        }
        return true;
    }
    // Operators
    [[nodiscard]] bool operator==(const String& other) const { return view() == (other.view()); }
    [[nodiscard]] bool operator!=(const String& other) const { return not operator==(other); }
    [[nodiscard]] bool operator==(const StringView other) const { return view() == (other); }
    [[nodiscard]] bool operator!=(const StringView other) const { return not operator==(other); }
    [[nodiscard]] bool operator<(const StringView other) const { return view() < other; }

    // Data
    StringEncoding encoding;
    Vector<char>   data;
};

template <int N>
struct SC::SmallString : public String
{
    Array<char, N> buffer;
    SmallString(StringEncoding encoding = StringEncoding::Utf8) : String(encoding)
    {
        SegmentHeader* header         = SegmentHeader::getSegmentHeader(buffer.items);
        header->options.isSmallVector = true;
        String::data.items            = buffer.items;
    }
    SmallString(SmallString&& other) : String(forward<String>(other)) {}
    SmallString(const SmallString& other) : String(other) {}
    SmallString& operator=(SmallString&& other)
    {
        String::operator=(forward<String>(other));
        return *this;
    }
    SmallString& operator=(const SmallString& other)
    {
        String::operator=(other);
        return *this;
    }

    SmallString(String&& other) : String(forward<String>(other)) {}
    SmallString(const String& other) : String(other) {}
    SmallString& operator=(String&& other)
    {
        String::operator=(forward<String>(other));
        return *this;
    }
    SmallString& operator=(const String& other)
    {
        String::operator=(other);
        return *this;
    }
};

template <>
struct SC::StringFormatterFor<SC::String>
{
    static bool format(StringFormatOutput& data, const StringIteratorASCII specifier, const String& value);
};
