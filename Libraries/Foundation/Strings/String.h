// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Containers/Array.h"
#include "../Containers/Vector.h"
#include "StringFormat.h"
#include "StringView.h"

namespace SC
{
struct String;

template <int N>
struct SmallString;

namespace Reflection
{
template <typename T>
struct MetaClass;
}
} // namespace SC
struct SC::String
{
    String(StringEncoding encoding = StringEncoding::Utf8) : encoding(encoding) {}

    // TODO: Figure out if removing this constructor in favour of the fallible assign makes the api ugly
    String(const StringView& sv) { SC_ASSERT_RELEASE(assign(sv)); }

    String(Vector<char>&& data, StringEncoding encoding) : encoding(encoding), data(move(data)) {}

    [[nodiscard]] bool assign(const StringView& sv);

    // const methods
    [[nodiscard]] StringEncoding getEncoding() const { return encoding; }
    [[nodiscard]] size_t         sizeInBytesIncludingTerminator() const { return data.size(); }
    //  - if string is empty        --> data.size() == 0
    //  - if string is not empty    --> data.size() > 2
    [[nodiscard]] const char* bytesIncludingTerminator() const { return data.data(); }
#if SC_PLATFORM_WINDOWS
    [[nodiscard]] wchar_t* nativeWritableBytesIncludingTerminator()
    {
        SC_ASSERT_RELEASE(encoding == StringEncoding::Utf16);
        return reinterpret_cast<wchar_t*>(data.data());
    }
#else
    [[nodiscard]] char* nativeWritableBytesIncludingTerminator()
    {
        SC_ASSERT_RELEASE(encoding < StringEncoding::Utf16);
        return data.data();
    }
#endif
    [[nodiscard]] bool isEmpty() const { return data.isEmpty(); }

    [[nodiscard]] StringView view() const;

    // Operators
    [[nodiscard]] bool operator==(const String& other) const { return view() == (other.view()); }
    [[nodiscard]] bool operator!=(const String& other) const { return not operator==(other); }
    [[nodiscard]] bool operator==(const StringView other) const { return view() == (other); }
    [[nodiscard]] bool operator!=(const StringView other) const { return not operator==(other); }
    [[nodiscard]] bool operator<(const StringView other) const { return view() < other; }

    Span<const char> toSpanConst() const { return data.toSpanConst(); }
    Span<char>       toSpan() { return data.toSpan(); }

  protected:
    friend struct StringTest;
    friend struct StringBuilder;
    friend struct StringConverter;
    friend struct FileDescriptor;
    friend struct FileSystem;
    template <typename T>
    friend struct Reflection::MetaClass;
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
    SmallString(StringView view) : String(view.getEncoding())
    {
        SegmentHeader* header         = SegmentHeader::getSegmentHeader(buffer.items);
        header->options.isSmallVector = true;
        String::data.items            = buffer.items;
        SC_ASSERT_RELEASE(assign(view));
    }
    SmallString(Vector<char>&& data, StringEncoding encoding) : String(forward<Vector<char>>(data), encoding) {}

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

    SmallString& operator=(StringView other)
    {
        SC_ASSERT_RELEASE(assign(other));
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
    static bool format(StringFormatOutput& data, const StringView specifier, const String& value);
};

namespace SC
{
template <int N>
using StringNative = SmallString<N * sizeof(native_char_t)>;
} // namespace SC
