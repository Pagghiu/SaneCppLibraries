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

    String(Vector<char>&& data, StringEncoding encoding) : encoding(encoding), data(move(data))
    {
        SC_ASSERT_RELEASE(addZeroTerminatorIfNeeded());
    }

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
    template <int N>
    friend struct SmallString;
    template <typename T>
    friend struct Reflection::MetaClass;
    StringEncoding encoding;
#if SC_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4324) // useless warning... ('struct_name' : structure was padded due to __declspec(align()))
#endif
    // alignas(alignof(SegmentHeader)) is needed on 32 bit to fix distance with SmallString::buffer
    alignas(alignof(SegmentHeader)) Vector<char> data;
#if SC_COMPILER_MSVC
#pragma warning(pop)
#endif
    [[nodiscard]] bool addZeroTerminatorIfNeeded();
};

template <int N>
struct SC::SmallString : public String
{
    Array<char, N> buffer;

    SmallString(StringEncoding encoding = StringEncoding::Utf8) : String(encoding)
    {
        SC_COMPILER_WARNING_PUSH_OFFSETOF;
        static_assert(alignof(SegmentHeader) == alignof(uint64_t), "alignof(segmentheader)");
        static_assert(SC_COMPILER_OFFSETOF(SmallString, buffer) - SC_COMPILER_OFFSETOF(String, data) ==
                          alignof(SegmentHeader),
                      "Wrong alignment");
        SC_COMPILER_WARNING_POP;
        init();
    }
    SmallString(StringView view) : String(view.getEncoding())
    {
        init();
        SC_ASSERT_RELEASE(assign(view));
    }
    SmallString(Vector<char>&& data, StringEncoding encoding) : String(encoding)
    {
        init();
        String::data = move(data);
        SC_ASSERT_RELEASE(String::addZeroTerminatorIfNeeded());
    }

    SmallString(SmallString&& other) : String(other.encoding)
    {
        init();
        String::data = move(other.data);
    }

    SmallString(const SmallString& other) : String(other.encoding)
    {
        init();
        String::data = other.data;
    }

    SmallString& operator=(SmallString&& other) noexcept
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

  private:
    void init()
    {
        SegmentHeader* header         = SegmentHeader::getSegmentHeader(buffer.items);
        header->options.isSmallVector = true;
        String::data.items            = buffer.items;
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
