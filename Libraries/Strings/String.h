// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Containers/Vector.h"
#include "../Strings/StringView.h"

namespace SC
{
struct String;
namespace Reflection
{
template <typename T>
struct Reflect;
}
} // namespace SC

//! @addtogroup group_strings
//! @{

/// @brief A non-modifiable owning string with associated encoding
struct SC::String
{
    /// @brief Builds an empty String with a given Encoding
    /// @param encoding The encoding of the String
    String(StringEncoding encoding = StringEncoding::Utf8) : encoding(encoding) {}

    /// @brief Builds String from a StringView
    /// @param sv StringView to be assigned to this String
    /// @warning This function will assert if StringView::assign fails
    String(StringView sv) { SC_ASSERT_RELEASE(assign(sv)); }

    /// @brief Builds String with a null terminated char string literal
    /// @tparam N Length of the string literal (including null terminator)
    /// @param text Pointer to string literal
    /// @warning This function will assert if StringView::assign fails
    template <size_t N>
    String(const char (&text)[N])
    {
        SC_ASSERT_RELEASE(assign(StringView(text, N - 1, true, StringEncoding::Ascii)));
    }

    /// @brief Assigns a StringView to this String, replacing existing contents
    /// @param sv StringView to be assigned to this string
    /// @return `true` if StringView is assigned successfully
    /// @note This method will invalidate any `StringView::view` previously obtained
    [[nodiscard]] bool assign(StringView sv);

    /// @brief Get StringView encoding
    /// @return Current encoding for this String
    [[nodiscard]] StringEncoding getEncoding() const { return encoding; }

    /// @brief Get length of the string in bytes (including null terminator bytes)
    /// @return Size in bytes including null terminator
    [[nodiscard]] size_t sizeInBytesIncludingTerminator() const { return data.size(); }

    /// @brief Access current string content as read-only null-terminated `const char*`
    /// @return A null terminated `const char*`
    [[nodiscard]] const char* bytesIncludingTerminator() const { return data.data(); }

    /// @brief Check if String is empty
    /// @return `true` if String is empty
    [[nodiscard]] bool isEmpty() const { return data.isEmpty(); }

    /// @brief Obtain a null-terminated StringView from current String
    /// @return a null-terminated StringView from current String
    [[nodiscard]] StringView view() const;

    /// @brief Check if current String is same as other String
    /// @param other String to be checked
    /// @return `true` if the two strings are equal
    [[nodiscard]] bool operator==(const String& other) const { return view() == (other.view()); }

    /// @brief Check if current String is different from other String
    /// @param other String to be checked
    /// @return `true` if the two strings are different
    [[nodiscard]] bool operator!=(const String& other) const { return not operator==(other); }

    /// @brief Check if current String is same as other StringView
    /// @param other StringView to be checked
    /// @return `true` if the String and StringView are equal
    [[nodiscard]] bool operator==(const StringView other) const { return view() == (other); }

    /// @brief Check if current String is different from other StringView
    /// @param other StringView to be checked
    /// @return `true` if the String and StringView are different
    [[nodiscard]] bool operator!=(const StringView other) const { return not operator==(other); }

    /// @brief Check if current String is smaller to another StringView (using StringView::compare)
    /// @param other StringView to be checked
    /// @return `true` if the String is smaller than StringView (using StringView::compare)
    [[nodiscard]] bool operator<(const StringView other) const { return view() < other; }

    /// @brief Check if current String is equal to the ascii string literal
    /// @tparam N Length of string literal, including null terminator
    /// @param other The string literal
    /// @return `true` if the String is the same as other
    template <size_t N>
    [[nodiscard]] bool operator==(const char (&other)[N]) const
    {
        return view() == other;
    }
    /// @brief Check if current String is different from the ascii string literal
    /// @tparam N Length of string literal, including null terminator
    /// @param other The string literal
    /// @return `true` if the String is different from other
    template <size_t N>
    [[nodiscard]] bool operator!=(const char (&other)[N]) const
    {
        return view() != other;
    }

    /// @brief Assigns an ascii string literal to current String
    /// @tparam N Length of string literal, including null terminator
    /// @param text  The string literal
    /// @return Reference to current String
    /// @warning Assignment operator will assert if String::assign fails
    template <size_t N>
    String& operator=(const char (&text)[N])
    {
        SC_ASSERT_RELEASE(assign(StringView(text, N - 1, true, StringEncoding::Ascii)));
        return *this;
    }

  protected:
    // TODO: nativeWritableBytesIncludingTerminator should be removed
    [[nodiscard]] auto nativeWritableBytesIncludingTerminator();

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
    friend struct Reflection::Reflect;
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
//! @}

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
