// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Result.h"
#include "../Foundation/Span.h"

namespace SC
{
#if SC_PLATFORM_WINDOWS
#define SC_NATIVE_STR(str) L##str
#else
#define SC_NATIVE_STR(str) str
#endif

/// @brief String Encoding (Ascii, Utf8, Utf16)
enum class StringEncoding : uint8_t
{
    Ascii = 0, ///< Encoding is ASCII
    Utf8  = 1, ///< Encoding is UTF8
    Utf16 = 2, ///< Encoding is UTF16-LE
#if SC_PLATFORM_WINDOWS
    Native = Utf16, ///< Encoding is UTF16-LE
#else
    Native = Utf8 ///< Encoding is UTF8
#endif
};

/// @brief Returns the number of bytes to represent an utf unit in the given encoding
/// @param encoding The encoding
/// @return number of bytes of the given encoding
constexpr uint32_t StringEncodingGetSize(StringEncoding encoding) { return encoding == StringEncoding::Utf16 ? 2 : 1; }

/// @brief An read-only view over a string (to avoid including @ref group_strings library when parsing is not needed).
/// The most common use case is to pass it in and out of OS API as is for file system paths.
/// Some libraries check the encoding and eventually convert the string to a different one when using specific OS api.
struct SC_COMPILER_EXPORT StringSpan
{
    // clang-format off
    
    /// @brief Construct an empty StringView
    constexpr StringSpan(StringEncoding encoding = StringEncoding::Ascii) : text(nullptr), textSizeInBytes(0), encoding(static_cast<uint8_t>(encoding)), hasNullTerm(0) {}

    /// @brief Construct a StringView from a Span of bytes.
    /// @param text The span containing the text _EXCLUDING_ eventual null terminator
    /// @param nullTerm `true` if a null terminator code point is expected to be found after Span. On ASCII and UTF8 this is 1 byte, on UTF16 it must be 2 bytes.
    /// @param encoding The encoding of the text contained in this StringView
    constexpr StringSpan(Span<const char> text, bool nullTerm, StringEncoding encoding) : text(text.data()), textSizeInBytes(text.sizeInBytes()), encoding(static_cast<uint8_t>(encoding)), hasNullTerm(nullTerm ? 1 : 0) {}

    /// @brief Constructs a StringView with a null terminated string terminal
    template <size_t N>
    constexpr StringSpan(const char (&str)[N]) : text(str), textSizeInBytes(N - 1), encoding(static_cast<uint8_t>(StringEncoding::Ascii)), hasNullTerm(true) {}

    /// @brief Constructs a StringView from a null terminated string
    static constexpr StringSpan fromNullTerminated(const char* text, StringEncoding encoding) { return text == nullptr ? StringSpan(encoding) : StringSpan({text, ::strlen(text)}, true, encoding); }

#if SC_PLATFORM_WINDOWS
    constexpr StringSpan(Span<const wchar_t> textSpan, bool nullTerm, StringEncoding encoding = StringEncoding::Native) : textWide(textSpan.data()), textSizeInBytes(textSpan.sizeInBytes()), encoding(static_cast<uint8_t>(encoding)), hasNullTerm(nullTerm ? 1 : 0) {}

    template <size_t N>
    constexpr StringSpan(const wchar_t (&str)[N]) : textWide(str), textSizeInBytes((N - 1)* sizeof(wchar_t)), encoding(static_cast<uint8_t>(StringEncoding::Native)), hasNullTerm(true) {}
    static constexpr StringSpan fromNullTerminated(const wchar_t* text, StringEncoding encoding) { return text == nullptr ? StringSpan(encoding) : StringSpan({text, ::wcslen(text)}, true); }
#endif
    constexpr bool operator ==(const StringSpan other) const { return textSizeInBytes == other.textSizeInBytes and ::memcmp(text, other.text, textSizeInBytes) == 0; }
    // clang-format on

    /// @brief Obtain a `const char` Span from this StringView
    [[nodiscard]] Span<const char> toCharSpan() const { return {text, textSizeInBytes}; }

    /// @brief Return true if StringView is empty
    [[nodiscard]] constexpr bool isEmpty() const { return text == nullptr or textSizeInBytes == 0; }

    /// @brief Check if StringView is immediately followed by a null termination character
    [[nodiscard]] constexpr bool isNullTerminated() const { return hasNullTerm; }

    /// @brief Get size of the StringView in bytes
    [[nodiscard]] constexpr size_t sizeInBytes() const { return textSizeInBytes; }

    /// @brief Get size of the StringView in bytes, including null terminator (2 bytes on UTF16)
    /// @warning This method will Assert if this isNullTerminated returns `false`
    [[nodiscard]] size_t sizeInBytesIncludingTerminator() const;

    /// @brief Get encoding of this StringView
    [[nodiscard]] constexpr StringEncoding getEncoding() const { return static_cast<StringEncoding>(encoding); }

    /// @brief Directly access the memory of this StringView
    [[nodiscard]] constexpr const char* bytesWithoutTerminator() const { return text; }

    /// @brief Directly access the memory of this null terminated-StringView.
    /// @warning This method will assert if string is not null terminated.
    [[nodiscard]] const char* bytesIncludingTerminator() const;

    /// @brief Directly access the memory of this null terminated-StringView.
    /// @return Pointer to start of StringView memory.
    /// On Windows return type will be `const wchar_t*`.
    /// On other platforms return type will be `const char*`.
    [[nodiscard]] auto getNullTerminatedNative() const
    {
#if SC_PLATFORM_WINDOWS
        return textWide;
#else
        return text;
#endif
    }

    struct NativeWritable
    {
        Span<native_char_t> writableSpan;
        size_t              length = 0;

        StringSpan view() const { return {{writableSpan.data(), length}, true, StringEncoding::Native}; }
    };

    /// @brief Writes this Span to a destination Span, using native encoding and null-terminating it
    /// @param string The destination writable string to write to
    Result writeNullTerminatedTo(NativeWritable& string) const;

    /// @brief Appends this Span to a destination Span, using native encoding and null-terminating it
    /// @param string The destination writable string to write to
    /// @param removePreviousNullTerminator If true, the previous null terminator is removed
    Result appendNullTerminatedTo(NativeWritable& string, bool removePreviousNullTerminator = true) const;

  protected:
    friend struct StringView;
    union
    {
        const char* text;
#if SC_PLATFORM_WINDOWS
        const wchar_t* textWide;
#endif
    };

    static constexpr size_t NumOptionBits = 3;
    static constexpr size_t MaxLength     = (~static_cast<size_t>(0)) >> NumOptionBits;

    size_t textSizeInBytes : sizeof(size_t) * 8 - NumOptionBits;
    size_t encoding    : 2;
    size_t hasNullTerm : 1;
};

} // namespace SC
