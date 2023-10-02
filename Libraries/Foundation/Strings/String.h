// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Containers/Array.h"
#include "../Containers/Vector.h"
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
    String(StringEncoding encoding = StringEncoding::Utf8);

    String(StringView sv);

    template <size_t N>
    String(const char (&text)[N])
    {
        SC_ASSERT_RELEASE(assign(StringView(text, N - 1, true, StringEncoding::Ascii)));
    }

    [[nodiscard]] bool assign(StringView sv);

    // const methods
    [[nodiscard]] StringEncoding getEncoding() const { return encoding; }
    [[nodiscard]] size_t         sizeInBytesIncludingTerminator() const { return data.size(); }
    //  - if string is empty        --> data.size() == 0
    //  - if string is not empty    --> data.size() > 2
    [[nodiscard]] const char* bytesIncludingTerminator() const { return data.data(); }
    [[nodiscard]] bool        isEmpty() const { return data.isEmpty(); }

    [[nodiscard]] StringView view() const;

    // Operators
    [[nodiscard]] bool operator==(const String& other) const { return view() == (other.view()); }
    [[nodiscard]] bool operator!=(const String& other) const { return not operator==(other); }
    [[nodiscard]] bool operator==(const StringView other) const { return view() == (other); }
    [[nodiscard]] bool operator!=(const StringView other) const { return not operator==(other); }
    [[nodiscard]] bool operator<(const StringView other) const { return view() < other; }
    template <size_t N>
    [[nodiscard]] bool operator==(const char (&other)[N]) const
    {
        return view() == other;
    }
    template <size_t N>
    [[nodiscard]] bool operator!=(const char (&other)[N]) const
    {
        return view() != other;
    }
    template <size_t Q>
    String& operator=(const char (&text)[Q])
    {
        SC_ASSERT_RELEASE(assign(StringView(text, Q - 1, true, StringEncoding::Ascii)));
        return *this;
    }

  protected:
    // All these friendships are made to leverage writing directly to the Vector<char>
    // but while still keeping it an implementation detail
    friend struct StringTest;
    friend struct StringBuilder;
    friend struct StringConverter;
    friend struct FileDescriptor;
    friend struct FileSystem;
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
#if SC_PLATFORM_WINDOWS
    [[nodiscard]] wchar_t* nativeWritableBytesIncludingTerminator();
#else
    [[nodiscard]] char* nativeWritableBytesIncludingTerminator();
#endif
};

template <int N>
struct SC::SmallString : public String
{
    Array<char, N> buffer;

    SmallString(StringEncoding encoding = StringEncoding::Utf8) : String(encoding) { init(); }

    SmallString(StringView view) : String(view.getEncoding())
    {
        init();
        SC_ASSERT_RELEASE(assign(view));
    }

    template <size_t Q>
    SmallString(const char (&text)[Q])
    {
        init();
        SC_ASSERT_RELEASE(assign(StringView(text, Q - 1, true, StringEncoding::Ascii)));
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
    template <size_t Q>
    SmallString& operator=(const char (&text)[Q])
    {
        SC_ASSERT_RELEASE(assign(StringView(text, Q - 1, true, StringEncoding::Ascii)));
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

namespace SC
{
template <int N>
using StringNative = SmallString<N * sizeof(native_char_t)>;
} // namespace SC
