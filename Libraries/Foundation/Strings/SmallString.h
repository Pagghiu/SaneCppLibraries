// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Containers/Array.h"
#include "../Strings/String.h"
#include "../Strings/StringConverter.h" // ensureZeroTermination

namespace SC
{

template <int N>
struct SmallString;

} // namespace SC

template <int N>
struct SC::SmallString : public String
{
    Array<char, N> buffer;

    SmallString(StringEncoding encoding = StringEncoding::Utf8) : String(encoding) { init(); }

    SmallString(StringView view) : String(view.getEncoding())
    {
        init();
        SC_COMPILER_WARNING_PUSH_OFFSETOF;
        static_assert(SC_COMPILER_OFFSETOF(SmallString<1>, buffer) - SC_COMPILER_OFFSETOF(String, data) ==
                          alignof(SegmentHeader),
                      "Wrong alignment");
        SC_COMPILER_WARNING_POP;
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
        SC_ASSERT_RELEASE(StringConverter::ensureZeroTermination(data, encoding));
        String::data = move(data);
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
        SegmentHeader* header = SegmentHeader::getSegmentHeader(buffer.items);
        header->isSmallVector = true;
        String::data.items    = buffer.items;
    }
};

namespace SC
{
template <int N>
using StringNative = SmallString<N * sizeof(native_char_t)>;
} // namespace SC
