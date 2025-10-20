// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/StringSpan.h"
#include "../Memory/Buffer.h"

namespace SC
{
struct SC_COMPILER_EXPORT String;
template <int N>
struct SmallString;
namespace Reflection
{
template <typename T>
struct Reflect;
}
} // namespace SC

//! @addtogroup group_memory
//! @{

/// @brief A non-modifiable owning string with associated encoding.
///
/// SC::String is (currently) implemented as a SC::Vector with the associated string encoding.
/// A SC::StringSpan can be obtained from it calling SC::String::view method but it's up to the user making sure that
/// the usage of such SC::StringSpan doesn't exceed lifetime of the SC::String it originated from (but thankfully
/// Address Sanitizer will catch the issue if it goes un-noticed).
struct SC::String
{
    /// @brief Builds an empty String with a given Encoding
    /// @param encoding The encoding of the String
    String(StringEncoding encoding = StringEncoding::Utf8) : encoding(encoding) {}

    /// @brief Builds String from a StringSpan
    /// @param sv StringSpan to be assigned to this String
    /// @warning This function will assert if StringSpan::assign fails
    String(StringSpan sv) { SC_ASSERT_RELEASE(assign(sv)); }

    /// @brief Builds a String from a buffer ensuring zero termination
    /// @warning This function will assert if StringSpan::assign fails
    String(Buffer&& otherData, StringEncoding encoding);

    /// @brief Builds String with a null terminated char string literal
    /// @tparam N Length of the string literal (including null terminator)
    /// @param text Pointer to string literal
    /// @warning This function will assert if StringSpan::assign fails
    template <size_t N>
    String(const char (&text)[N])
    {
        SC_ASSERT_RELEASE(assign(StringSpan({text, N - 1}, true, StringEncoding::Ascii)));
    }

    /// @brief Checks if the memory pointed by the StringSpan is owned by this String
    /// @param view StringSpan to be checked
    /// @return `true` if StringSpan memory belongs to this String
    [[nodiscard]] bool owns(StringSpan view) const;

    /// @brief Assigns a StringSpan to this String, replacing existing contents
    /// @param sv StringSpan to be assigned to this string
    /// @return `true` if StringSpan is assigned successfully
    /// @note This method will invalidate any `StringSpan::view` previously obtained
    [[nodiscard]] bool assign(StringSpan sv);

    /// @brief Get StringSpan encoding
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

    /// @brief Obtain a null-terminated StringSpan from current String
    /// @return a null-terminated StringSpan from current String
    [[nodiscard]] StringSpan view() const SC_LANGUAGE_LIFETIME_BOUND;

    /// @brief Check if current String is same as other String
    /// @param other String to be checked
    /// @return `true` if the two strings are equal
    [[nodiscard]] bool operator==(const String& other) const { return view() == (other.view()); }

    /// @brief Check if current String is different from other String
    /// @param other String to be checked
    /// @return `true` if the two strings are different
    [[nodiscard]] bool operator!=(const String& other) const { return not operator==(other); }

    /// @brief Check if current String is same as other StringSpan
    /// @param other StringSpan to be checked
    /// @return `true` if the String and StringSpan are equal
    [[nodiscard]] bool operator==(const StringSpan other) const { return view() == (other); }

    /// @brief Check if current String is different from other StringSpan
    /// @param other StringSpan to be checked
    /// @return `true` if the String and StringSpan are different
    [[nodiscard]] bool operator!=(const StringSpan other) const { return not operator==(other); }

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

    /// @brief Check if current String is smaller to another StringView (using StringView::compare)
    /// @param other StringView to be checked
    /// @return `true` if the String is smaller than StringView (using StringView::compare)
    [[nodiscard]] bool operator<(const StringSpan other) const { return view() < other; }

    /// @brief Assigns an ascii string literal to current String
    /// @tparam N Length of string literal, including null terminator
    /// @param text  The string literal
    /// @return Reference to current String
    /// @warning Assignment operator will assert if String::assign fails
    template <size_t N>
    String& operator=(const char (&text)[N])
    {
        SC_ASSERT_RELEASE(assign(StringSpan({text, N - 1}, true, StringEncoding::Ascii)));
        return *this;
    }

    /// @brief Assigns (copy) contents of given StringSpan in current String
    /// @warning Assignment operator will assert if String::assign fails
    String& operator=(StringSpan view);

  protected:
    struct Internal;

    friend struct StringTest;
    template <typename T>
    friend struct Reflection::Reflect;

    template <typename T>
    friend struct GrowableBuffer;
    struct SC_COMPILER_EXPORT GrowableImplementation
    {
        String& string;

        IGrowableBuffer::DirectAccess& da;

        GrowableImplementation(String& string, IGrowableBuffer::DirectAccess& da) noexcept;
        ~GrowableImplementation() noexcept;
        void finalize() noexcept;
        bool tryGrowTo(size_t newSize) noexcept;
    };

    StringEncoding encoding;
    Buffer         data;

    String(StringEncoding encoding, uint32_t inlineCapacity);
    String(Buffer&& otherData, StringEncoding encoding, uint32_t inlineCapacity);
};

/// @brief String with compile time configurable inline storage (small string optimization)
/// @tparam N number of chars to reserve in inline storage
template <int N>
struct SC::SmallString : public String
{
    // Unfortunately we have to repeat all these overloads to set inline capacity and hasInlineData flag
    SmallString(StringEncoding encoding = StringEncoding::Utf8) : String(encoding, N) {}
    SmallString(const SmallString& other) : SmallString(other.getEncoding()) { String::operator=(other); }
    SmallString(SmallString&& other) : SmallString(other.getEncoding()) { String::operator=(move(other)); }
    String& operator=(const SmallString& other) { return String::operator=(other); }
    String& operator=(SmallString&& other) { return String::operator=(move(other)); }

    SmallString(const String& other) : SmallString(other.getEncoding()) { String::operator=(other); }
    SmallString(String&& other) : SmallString(other.getEncoding()) { String::operator=(move(other)); }
    SmallString(StringSpan other) : SmallString(other.getEncoding()) { String::operator=(move(other)); }
    SmallString(Buffer&& otherData, StringEncoding encoding) : String(move(otherData), encoding, N) {}
    template <size_t Q>
    SmallString(const char (&text)[Q]) : SmallString(StringSpan({text, Q - 1}, true, StringEncoding::Ascii))
    {}

  private:
    uint64_t inlineCapacity = N;
    char     buffer[N];
};
//! @}

namespace SC
{
template <int N>
using SmallStringNative = SmallString<N * sizeof(native_char_t)>;

// Allows using this type across Plugin boundaries
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT SmallString<64>;
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT SmallString<128 * sizeof(native_char_t)>;
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT SmallString<255 * sizeof(native_char_t)>;
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT SmallString<512 * sizeof(native_char_t)>;
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT SmallString<1024 * sizeof(native_char_t)>;

// Enables File library from reading data from file descriptor into a String
template <>
struct SC_COMPILER_EXPORT GrowableBuffer<String> : public IGrowableBuffer
{
    String::GrowableImplementation gi;
    GrowableBuffer(String& string)
        : IGrowableBuffer(&GrowableBuffer::tryGrowTo), gi(string, IGrowableBuffer::directAccess)
    {}
    static bool tryGrowTo(IGrowableBuffer& gb, size_t newSize) noexcept
    {
        return static_cast<GrowableBuffer&>(gb).gi.tryGrowTo(newSize);
    }
    static auto getEncodingFor(const String& str) noexcept { return str.getEncoding(); }
    void        finalize() noexcept { gi.finalize(); }
};

template <int N>
struct SC_COMPILER_EXPORT GrowableBuffer<SmallString<N>> : public IGrowableBuffer
{
    String::GrowableImplementation gi;
    GrowableBuffer(String& string)
        : IGrowableBuffer(&GrowableBuffer::tryGrowTo), gi(string, IGrowableBuffer::directAccess)
    {}
    static bool tryGrowTo(IGrowableBuffer& gb, size_t newSize) noexcept
    {
        return static_cast<GrowableBuffer&>(gb).gi.tryGrowTo(newSize);
    }
    static auto getEncodingFor(const SmallString<N>& str) noexcept { return str.getEncoding(); }
    void        finalize() noexcept { gi.finalize(); }
};
} // namespace SC
