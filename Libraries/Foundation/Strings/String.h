// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Containers/Vector.h"
#include "StringView.h"

namespace SC
{
struct String;
namespace Reflection
{
template <typename T>
struct MetaClass;
}
} // namespace SC

struct SC::String
{
    String(StringEncoding encoding = StringEncoding::Utf8) : encoding(encoding) {}

    String(StringView sv) { SC_ASSERT_RELEASE(assign(sv)); }

    template <size_t N>
    String(const char (&text)[N])
    {
        SC_ASSERT_RELEASE(assign(StringView(text, N - 1, true, StringEncoding::Ascii)));
    }

    [[nodiscard]] bool assign(StringView sv);

    // const methods
    [[nodiscard]] StringEncoding getEncoding() const { return encoding; }

    [[nodiscard]] size_t sizeInBytesIncludingTerminator() const { return data.size(); }

    [[nodiscard]] const char* bytesIncludingTerminator() const { return data.data(); }

    [[nodiscard]] auto nativeWritableBytesIncludingTerminator();

    [[nodiscard]] bool isEmpty() const { return data.isEmpty(); }

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

    template <size_t N>
    String& operator=(const char (&text)[N])
    {
        SC_ASSERT_RELEASE(assign(StringView(text, N - 1, true, StringEncoding::Ascii)));
        return *this;
    }

  protected:
    // All these friendships are made to leverage writing directly to the Vector<char>
    // but while still keeping it an implementation detail
    friend struct SmallStringTest;
    friend struct StringBuilder;
    friend struct StringConverter;
    friend struct FileDescriptor;
    friend struct FileSystem;
    template <int>
    friend struct SmallString;
    template <typename T>
    friend struct Reflection::MetaClass;
    StringEncoding encoding;
#if SC_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4324) // useless warning on 32 bit... (structure was padded due to __declspec(align()))
#endif
    // alignas(alignof(SegmentHeader)) is needed on 32 bit to fix distance with SmallString::buffer
    alignas(alignof(SegmentHeader)) Vector<char> data;
#if SC_COMPILER_MSVC
#pragma warning(pop)
#endif
};

//-----------------------------------------------------------------------------------------------------------------------
// Implementations Details
//-----------------------------------------------------------------------------------------------------------------------

inline bool SC::String::assign(StringView sv)
{
    encoding             = sv.getEncoding();
    const size_t length  = sv.sizeInBytes();
    const size_t numZero = StringEncodingGetSize(encoding);
    if (not data.resizeWithoutInitializing(length + numZero))
        return false;
    if (sv.isNullTerminated())
    {
        memcpy(data.items, sv.bytesWithoutTerminator(), length + numZero);
    }
    else
    {
        memcpy(data.items, sv.bytesWithoutTerminator(), length);
        for (size_t idx = 0; idx < numZero; ++idx)
        {
            data.items[length + idx] = 0;
        }
    }
    return true;
}

inline SC::StringView SC::String::view() const
{
    const bool  isEmpty = data.isEmpty();
    const char* items   = isEmpty ? nullptr : data.items;
    return StringView(items, isEmpty ? 0 : data.size() - StringEncodingGetSize(encoding), not isEmpty, encoding);
}

inline auto SC::String::nativeWritableBytesIncludingTerminator()
{
#if SC_PLATFORM_WINDOWS
    SC_ASSERT_RELEASE(encoding == StringEncoding::Utf16);
    return reinterpret_cast<wchar_t*>(data.data());
#else
    SC_ASSERT_RELEASE(encoding < StringEncoding::Utf16);
    return data.data();
#endif
}
