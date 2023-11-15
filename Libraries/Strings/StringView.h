// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Strings/StringIterator.h"

namespace SC
{
struct SC_COMPILER_EXPORT StringView;
struct SC_COMPILER_EXPORT StringViewTokenizer;
struct SC_COMPILER_EXPORT StringAlgorithms;

} // namespace SC

//! @defgroup group_strings Strings
//! @copybrief library_strings
//!
//! See @ref library_strings library page for more details.<br>

//! @addtogroup group_strings
//! @{

/// @brief Non-owning view over a range of characers with UTF Encoding.
struct SC::StringView
{
    /// @brief Construct an emtpy StringView
    constexpr StringView();

    /// @brief Construct a StringView from a Span of bytes.
    /// @param textSpan The span containing the text _EXCLUDING_ eventual null terminator
    /// @param nullTerm `true` if a null terminator code point is expected to be found after Span.
    ///                 On ASCII and UTF8 this is 1 byte, on UTF16 it must be 2 bytes.
    /// @param encoding The encoding of the text contained in this StringView
    constexpr StringView(Span<char> textSpan, bool nullTerm, StringEncoding encoding);

    /// @brief Construct a StringView from a Span of bytes.
    /// @param textSpan The span containing the text _EXCLUDING_ eventual null terminator
    /// @param nullTerm `true` if a null terminator code point is expected to be found after Span.
    ///                 On ASCII and UTF8 this is 1 byte, on UTF16 it must be 2 bytes.
    /// @param encoding The encoding of the text contained in this StringView
    constexpr StringView(Span<const char> textSpan, bool nullTerm, StringEncoding encoding);

    /// @brief Construct a StringView from a C string
    /// @param text The C-String
    /// @param numBytes Number of bytes in the string EXCLUDING null terminator
    /// @param nullTerm `true` if a null terminator code point is expected to be found after Span.
    ///                 On ASCII and UTF8 this is 1 byte, on UTF16 it must be 2 bytes.
    /// @param encoding The encoding of the text contained in this StringView
    constexpr StringView(const char* text, size_t numBytes, bool nullTerm, StringEncoding encoding);

    /// @brief Constructs a StringView from a null-terminated C-String
    /// @param text The null-terminated C-String
    /// @param encoding The encoding of the text contained in this StringView
    /// @return The StringView containing text with given encoding
    static StringView fromNullTerminated(const char* text, StringEncoding encoding);

    /// @brief Constructs a StringView with a null terminated string terminal
    /// @tparam N Number of characters in text string literal
    /// @param text The Null terminated string literal
    template <size_t N>
    constexpr StringView(const char (&text)[N]);

#if SC_PLATFORM_WINDOWS || DOXYGEN
    /// @brief Constructs an UTF16 StringView with a null terminated wide-string terminal
    /// @tparam N
    /// @param text The Null terminated wide-string literal
    template <size_t N>
    constexpr StringView(const wchar_t (&text)[N]);
    /// @brief Constructs an UTF16 StringView from a null-terminated wide C-String
    /// @param text The null-terminated wide C-String
    /// @param numBytes Number of bytes in the string EXCLUDING null terminator (that must be 2 bytes)
    /// @param nullTerm `true` if a null terminator code point is expected to be found after Span.
    ///                 On ASCII and UTF8 this is 1 byte, on UTF16 it must be 2 bytes.
    constexpr StringView(const wchar_t* text, size_t numBytes, bool nullTerm);

    /// @brief Construct an UTF16 StringView from a Span of bytes.
    /// @param textSpan The span containing the text _EXCLUDING_ eventual null terminator
    /// @param nullTerm `true` if a null terminator code point is expected to be found after Span (two bytes on UTF16)
    constexpr StringView(Span<const wchar_t> textSpan, bool nullTerm);
#endif

    /// @brief Get encoding of this StringView
    /// @return This StringView encoding
    [[nodiscard]] constexpr StringEncoding getEncoding() const { return static_cast<StringEncoding>(encoding); }

    /// @brief Directly access the memory of this StringView
    /// @return Pointer to start of StringView memory
    [[nodiscard]] constexpr const char* bytesWithoutTerminator() const { return text; }

    /// @brief Directly access the memory of this null terminated-StringView.
    /// @return Pointer to start of StringView memory
    /// @warning This method will assert if string is not null terminated.
    [[nodiscard]] constexpr const char* bytesIncludingTerminator() const;

    /// @brief Directly access the memory of this null terminated-StringView.
    /// @return Pointer to start of StringView memory.
    /// On Windows return type will be `wchar_t`.
    /// On other platforms return type will be `char`.
    /// @warning This method will assert that the string is null terminated.
    ///          On Windows this will assert that encoding is UTF16.
    ///          On other platforms this will assert that encoding is UTF8 or ASCII.
    auto getNullTerminatedNative() const;

    /// @brief Obtain a `const char` Span from this StringView
    /// @return Span representing this StringView
    constexpr Span<const char> toCharSpan() const { return {text, textSizeInBytes}; }

    /// @brief Obtain a `const uint8_t` Span from this StringView
    /// @return Span representing this StringView
    Span<const uint8_t> toBytesSpan() const { return Span<const uint8_t>::reinterpret_bytes(text, textSizeInBytes); }

    /// @brief Result of ordering comparison done by StringView::compare
    enum class Comparison
    {
        Smaller = -1, ///< Current string is smaller than the other
        Equals  = 0,  ///< Current string is equal to the other
        Bigger  = 1   ///< Current string is bigger than the other
    };

    /// @brief Ordering comparison between non-normalized StringView (operates on code points, not on utf graphemes)
    /// @param other The string being compared to current one
    /// @return Result of the comparison (smaller, equals or bigger)
    [[nodiscard]] Comparison compare(StringView other) const;

    /// @brief Ordering operator for StringView using StringView::compare
    /// @param other The string being compared to current one
    /// @return `true` if current string is Comparison::Smaller than other
    [[nodiscard]] bool operator<(StringView other) const { return compare(other) == Comparison::Smaller; }

    /// @brief Call given lambda with one of StringIteratorASCII, StringIteratorUTF8, StringIteratorUTF16 depending on
    /// encoding.
    /// @tparam Func A function/lambda with `auto operator()({StringIteratorASCII | StringIteratorUTF8 |
    /// StringIteratorUTF16})`
    /// @param func lambda / functor to be called
    /// @return whatever value is returned by invoking func
    template <typename Func>
    [[nodiscard]] constexpr auto withIterator(Func&& func) const;

    /// @brief Call given lambda with one of StringIteratorASCII, StringIteratorUTF8, StringIteratorUTF16 depending on
    /// encoding.
    /// @tparam Func Func A function/lambda with `auto operator()({StringIteratorASCII | StringIteratorUTF8 |
    /// StringIteratorUTF16}, {StringIteratorASCII | StringIteratorUTF8 | StringIteratorUTF16})`
    /// @param s1 StringView that will be passed as first iterator
    /// @param s2 StringView that will be passed as second iterator
    /// @param func the lambda / to be invoked
    /// @return whatever value is returned by invoking func
    template <typename Func>
    [[nodiscard]] static constexpr auto withIterators(StringView s1, StringView s2, Func&& func);

    /// @brief Returns a StringIterator from current StringView
    /// @tparam StringIterator can be StringIteratorASCII, StringIteratorUTF8 or StringIteratorUTF16
    /// @return StringIterator representing the StringView
    template <typename StringIterator>
    constexpr StringIterator getIterator() const;

    /// @brief Compare this StringView with another StringView for inequality
    /// @param other StringView to be compared with current one
    /// @return 'true' if the two StringView are different
    [[nodiscard]] constexpr bool operator!=(StringView other) const { return not operator==(other); }

    /// @brief Compare this StringView with another StringView for equality
    /// @param other StringView to be compared with current one
    /// @return 'true' if the two StringView are the same
    [[nodiscard]] constexpr bool operator==(StringView other) const;

    /// @brief Check if this StringView is equal to other StringView (operates on code points, not on utf graphemes).
    /// Returns the number of code points that are the same in both StringViews.
    /// @param other The StringView to be compared to
    /// @param commonOverlappingPoints number of equal code points in both StringView
    /// @return `true` if the two StringViews are equal
    [[nodiscard]] constexpr bool fullyOverlaps(StringView other, size_t& commonOverlappingPoints) const;

    /// @brief Check if StringView is empty
    /// @return `true` if string is empty
    [[nodiscard]] constexpr bool isEmpty() const { return text == nullptr or textSizeInBytes == 0; }

    /// @brief Check if StringView is immediately followed by a null termination character
    /// @return `true` if StringView is immediately followed by a null termination character
    [[nodiscard]] constexpr bool isNullTerminated() const { return hasNullTerm; }

    /// @brief Get size of the StringView in bytes
    /// @return Size in bytes of StringView
    [[nodiscard]] constexpr size_t sizeInBytes() const { return textSizeInBytes; }

    /// @brief Get size of the StringView in bytes, including null terminator
    /// @return Size in bytes of StringView including null terminator (2 bytes on UTF16)
    /// @warning This method can be called only on StringView that are null terminated.
    ///         This means that it will Assert if this StringView::isNullTerminated returns `false`
    [[nodiscard]] constexpr size_t sizeInBytesIncludingTerminator() const;

    /// @brief Check if StringView ends with given utf code point
    /// @param c The utf code point to check against
    /// @return  Returns `true` if this StringView ends with code point c
    [[nodiscard]] bool endsWithChar(StringCodePoint c) const;

    /// @brief Check if StringView starts with given utf code point
    /// @param c The utf code point to check against
    /// @return  Returns `true` if this StringView starts with code point c
    [[nodiscard]] bool startsWithChar(StringCodePoint c) const;

    /// @brief Check if StringView starts with another StringView
    /// @param str The other StringView to check with current
    /// @return  Returns `true` if this StringView starts with str
    [[nodiscard]] bool startsWith(const StringView str) const;

    /// @brief Check if StringView ends with another StringView
    /// @param str The other StringView to check with current
    /// @return  Returns `true` if this StringView ends with str
    [[nodiscard]] bool endsWith(const StringView str) const;

    /// @brief Check if StringView contains another StringView with compatible encoding.
    /// @param str The other StringView to check with current
    /// @return  Returns `true` if this StringView contains str
    /// @warning This method will assert if strings have non compatible encoding.
    ///          It can be checked with StringView::hasCompatibleEncoding (str) == `true`
    [[nodiscard]] bool containsString(const StringView str) const;

    /// @brief Check if StringView contains given utf code point
    /// @param c The utf code point to check against
    /// @return  Returns `true` if this StringView contains code point c
    [[nodiscard]] bool containsChar(StringCodePoint c) const;

    /// @brief Check if current StringView has compatible encoding with str.
    ///        This means checking if two encodings have the same utf unit size.
    /// @param str The other StringView to check with current
    /// @return Returns `true` if this StringView has compatible encoding with str
    [[nodiscard]] constexpr bool hasCompatibleEncoding(StringView str) const;

    /// Returns a StringView from two iterators. The from iterator will be shortened until the start of to

    /// @brief  Returns a StringView starting at `from` and ending at `to`.
    /// @tparam StringIterator One among StringIteratorASCII, StringIteratorUTF8 and StringIteratorUTF16
    /// @param from Indicates where the StringView will start.
    ///             The from iterator will be shortened until the start of to.
    /// @param to   Indicates where the StringView will end.
    /// @return A StringView  starting at `from` and ending at `to`.
    /// @note If from is `>` to an empty StringView will be returned
    template <typename StringIterator>
    static StringView fromIterators(StringIterator from, StringIterator to);

    /// @brief Returns a section of a string, from `it` to end of StringView.
    /// @tparam StringIterator One among StringIteratorASCII, StringIteratorUTF8 and StringIteratorUTF16
    /// @param it The iterator pointing at the start of the specified portion of StringView.
    /// @return Another StringView pointing at characters from `it` up to StringView end
    template <typename StringIterator>
    static StringView fromIteratorUntilEnd(StringIterator it);

    /// @brief Returns a section of a string, from start of StringView to `it`.
    /// @tparam StringIterator One among StringIteratorASCII, StringIteratorUTF8 and StringIteratorUTF16
    /// @param it The iterator pointing at the start of the specified portion of StringView.
    /// @return Another StringView pointing at characters from start of StringView until `it`
    template <typename StringIterator>
    static constexpr StringView fromIteratorFromStart(StringIterator it);

    /// @brief Get slice `[start, end)` starting at offset `start` and ending at `end` (measured in utf code points)
    /// @param start The initial code point where the slice starts
    /// @param end One after the final code point where the slice ends
    /// @return The `[start, end)` StringView slice
    [[nodiscard]] StringView sliceStartEnd(size_t start, size_t end) const;

    /// @brief Get slice `[start, start+length]` starting at offset `start` and of `length` code points
    /// @param start The initial code point where the slice starts
    /// @param length One after the final code point where the slice ends
    /// @return The `[start, start+length]` StringView slice
    [[nodiscard]] StringView sliceStartLength(size_t start, size_t length) const;

    /// @brief Get slice `[offset, end]` measured in utf code points
    /// @param offset The initial code point where the slice starts
    /// @return The sliced StringView `[offset, end]`
    [[nodiscard]] StringView sliceStart(size_t offset) const;

    /// @brief Get slice `[end-offset, end]` measured in utf code points
    /// @param offset The initial code point where the slice starts
    /// @return The sliced StringView `[end-offset, end]`
    [[nodiscard]] StringView sliceEnd(size_t offset) const;

    /// @brief Returns a shortened StringView without utf code points matching `c` at end.
    /// @param c The utf code point to look for
    /// @return The trimmed StringView
    [[nodiscard]] StringView trimEndingChar(StringCodePoint c) const;

    /// @brief Returns a shortened StringView without utf code points matching `c` at start.
    /// @param c The utf code point to look for
    /// @return The trimmed StringView
    [[nodiscard]] StringView trimStartingChar(StringCodePoint c) const;

    /// @brief Returns a shortened StringView from current cutting the first `start` bytes.
    /// @param start Offset in bytes where the slice starts.
    /// @return A sliced StringView starting at `start` bytes offset
    [[nodiscard]] constexpr StringView sliceStartBytes(size_t start) const;

    /// @brief Returns a shortened StringView taking a slice from `start` to `end` expressed in bytes.
    /// @param start Offset in bytes where the slice will start.
    /// @param end Offset in bytes where the slice will end.
    /// @return A sliced StringView starting at `start` bytes offset and ending at `end` bytes offset.
    [[nodiscard]] constexpr StringView sliceStartEndBytes(size_t start, size_t end) const;

    /// @brief Returns a shortened StringView taking a slice from `start` ending at `start`+`length` bytes.
    /// @param start Offset in bytes where the slice will start.
    /// @param length Length in bytes from `start` where the slice will end.
    /// @return A sliced StringView starting at `start` bytes offset and ending at `start`+`length` bytes offset.
    [[nodiscard]] constexpr StringView sliceStartLengthBytes(size_t start, size_t length) const;

    /// If the current view is an integer number, returns true

    /// @brief Check if StringView can be parsed as an integer number.
    /// @return `true` if StringView is an integer number.
    [[nodiscard]] bool isIntegerNumber() const;

    /// @brief Check if StringView can be parsed as an floating point number.
    /// @return `true` if StringView is a floating point number.
    [[nodiscard]] bool isFloatingNumber() const;

    /// Parses int32, returning false if it fails

    /// @brief Try parsing current StringView as a 32 bit integer.
    /// @param value Will receive the parsed 32 bit integer, if function returns `true`.
    /// @return `true` if the StringView has been successfully parsed as a 32 bit integer.
    [[nodiscard]] bool parseInt32(int32_t& value) const;

    /// @brief Try parsing current StringView as a floating point number.
    /// @param value Will receive the parsed floating point number, if function returns `true`.
    /// @return `true` if the StringView has been successfully parsed as a floating point number.
    [[nodiscard]] bool parseFloat(float& value) const;

    /// @brief Try parsing current StringView as a double precision floating point number.
    /// @param value Will receive the parsed double precision floating point number, if function returns `true`.
    /// @return `true` if the StringView has been successfully parsed as a double precision floating point number.
    [[nodiscard]] bool parseDouble(double& value) const;

  private:
    union
    {
        const char*    text;
        const wchar_t* textWide;
    };
    using SizeType = size_t;

    static constexpr SizeType NumOptionBits = 3;
    static constexpr SizeType MaxLength     = (~static_cast<SizeType>(0)) >> NumOptionBits;

    SizeType textSizeInBytes : sizeof(SizeType) * 8 - NumOptionBits;
    SizeType encoding    : 2;
    SizeType hasNullTerm : 1;

    template <typename T>
    struct identity
    {
    };
    template <typename Type>
    constexpr StringIteratorASCII getIterator(identity<Type>) const;
    constexpr StringIteratorUTF8  getIterator(identity<StringIteratorUTF8>) const;
    constexpr StringIteratorUTF16 getIterator(identity<StringIteratorUTF16>) const;
    template <typename StringIterator1, typename StringIterator2>
    static constexpr bool equalsIterator(StringIterator1 t1, StringIterator2 t2, size_t& points);

    template <typename StringIterator>
    constexpr bool equalsIterator(StringView other, size_t& points) const;
};

/// @brief Splits a StringView in tokens according to separators
struct SC::StringViewTokenizer
{
    StringCodePoint splittingCharacter = 0;
    size_t          numSplitsNonEmpty  = 0;
    size_t          numSplitsTotal     = 0;
    StringView      component;
    StringView      processed;

    enum Options
    {
        IncludeEmpty,
        SkipEmpty
    };
    StringViewTokenizer(StringView text) : originalText(text), current(text) {}
    [[nodiscard]] bool   tokenizeNext(Span<const StringCodePoint> separators, Options options);
    StringViewTokenizer& countTokens(Span<const StringCodePoint> separators);

    [[nodiscard]] bool isFinished() const;

  private:
    StringView originalText;
    StringView current;
};

/// @brief Algorithms operating on strings (glob / wildcard)
struct SC::StringAlgorithms
{
    [[nodiscard]] static bool matchWildcard(StringView s1, StringView s2);

  private:
    template <typename StringIterator1, typename StringIterator2>
    [[nodiscard]] static bool matchWildcardIterator(StringIterator1 pattern, StringIterator2 text);
};

//! @}

//-----------------------------------------------------------------------------------------------------------------------
// Implementations Details
//-----------------------------------------------------------------------------------------------------------------------
namespace SC
{
constexpr SC::StringView operator""_a8(const char* txt, size_t sz)
{
    return SC::StringView(txt, sz, true, SC::StringEncoding::Ascii);
}
constexpr SC::StringView operator""_u8(const char* txt, size_t sz)
{
    return SC::StringView(txt, sz, true, SC::StringEncoding::Utf8);
}
constexpr SC::StringView operator""_u16(const char* txt, size_t sz)
{
    const bool isNullTerminated = sz > 0 and sz % 2 == 1 and txt[sz - 1] == 0;
    return SC::StringView(txt, isNullTerminated ? sz - 1 : sz, isNullTerminated, SC::StringEncoding::Utf16);
}
} // namespace SC

#if SC_PLATFORM_WINDOWS
#define SC_STR_NATIVE(str) L##str
#else
#define SC_STR_NATIVE(str) str
#endif

constexpr SC::StringView::StringView()
    : text(nullptr), textSizeInBytes(0), encoding(static_cast<SizeType>(StringEncoding::Ascii)), hasNullTerm(false)
{}

constexpr SC::StringView::StringView(Span<char> textSpan, bool nullTerm, StringEncoding encoding)
    : text(textSpan.data()), textSizeInBytes(static_cast<SizeType>(textSpan.sizeInBytes())),
      encoding(static_cast<SizeType>(encoding)), hasNullTerm(nullTerm)
{
    static_assert(sizeof(StringView) == sizeof(void*) + sizeof(SizeType), "StringView wrong size");
    SC_ASSERT_DEBUG(textSpan.sizeInBytes() <= MaxLength);
}

constexpr SC::StringView::StringView(Span<const char> textSpan, bool nullTerm, StringEncoding encoding)
    : text(textSpan.data()), textSizeInBytes(static_cast<SizeType>(textSpan.sizeInBytes())),
      encoding(static_cast<SizeType>(encoding)), hasNullTerm(nullTerm)
{
    SC_ASSERT_DEBUG(textSpan.sizeInBytes() <= MaxLength);
}

constexpr SC::StringView::StringView(const char* text, size_t numBytes, bool nullTerm, StringEncoding encoding)
    : text(text), textSizeInBytes(static_cast<SizeType>(numBytes)), encoding(static_cast<SizeType>(encoding)),
      hasNullTerm(nullTerm)
{
    SC_ASSERT_DEBUG(numBytes <= MaxLength);
}

template <SC::size_t N>
constexpr SC::StringView::StringView(const char (&text)[N])
    : text(text), textSizeInBytes(N - 1), encoding(static_cast<SizeType>(StringEncoding::Ascii)), hasNullTerm(true)
{}

#if SC_PLATFORM_WINDOWS
template <size_t N>
constexpr SC::StringView::StringView(const wchar_t (&text)[N])
    : textWide(text), textSizeInBytes((N - 1) * sizeof(wchar_t)), encoding(static_cast<SizeType>(StringEncoding::Wide)),
      hasNullTerm(true)
{}

constexpr SC::StringView::StringView(const wchar_t* text, size_t numBytes, bool nullTerm)
    : textWide(text), textSizeInBytes(static_cast<SizeType>(numBytes)),
      encoding(static_cast<SizeType>(StringEncoding::Wide)), hasNullTerm(nullTerm)
{
    SC_ASSERT_DEBUG(numBytes <= MaxLength);
}

constexpr SC::StringView::StringView(Span<const wchar_t> textSpan, bool nullTerm)
    : textWide(textSpan.data()), textSizeInBytes(static_cast<SizeType>(textSpan.sizeInBytes())),
      encoding(static_cast<SizeType>(StringEncoding::Wide)), hasNullTerm(nullTerm)
{
    SC_ASSERT_DEBUG(textSpan.sizeInBytes() <= MaxLength);
}
#endif

[[nodiscard]] constexpr const char* SC::StringView::bytesIncludingTerminator() const
{
    SC_ASSERT_RELEASE(hasNullTerm);
    return text;
}

template <typename StringIterator>
constexpr StringIterator SC::StringView::getIterator() const
{
    // For GCC complaining about specialization in non-namespace scope
    return getIterator(identity<StringIterator>());
}

template <typename Type>
constexpr SC::StringIteratorASCII SC::StringView::getIterator(identity<Type>) const
{
    return StringIteratorASCII(text, text + textSizeInBytes);
}
constexpr SC::StringIteratorUTF8 SC::StringView::getIterator(identity<StringIteratorUTF8>) const
{
    return StringIteratorUTF8(text, text + textSizeInBytes);
}
constexpr SC::StringIteratorUTF16 SC::StringView::getIterator(identity<StringIteratorUTF16>) const
{
    return StringIteratorUTF16(text, text + textSizeInBytes);
}

template <typename StringIterator1, typename StringIterator2>
constexpr bool SC::StringView::equalsIterator(StringIterator1 t1, StringIterator2 t2, size_t& points)
{
    StringCodePoint c1 = 0;
    StringCodePoint c2 = 0;
    while (t1.advanceRead(c1) and t2.advanceRead(c2))
    {
        if (c1 != c2)
        {
            return false;
        }
        points++;
    }
    return t1.isAtEnd() and t2.isAtEnd();
}

template <typename StringIterator>
constexpr bool SC::StringView::equalsIterator(StringView other, size_t& points) const
{
    auto it = getIterator<StringIterator>();
    switch (other.getEncoding())
    {
    case StringEncoding::Ascii: return equalsIterator(it, other.getIterator<StringIteratorASCII>(), points);
    case StringEncoding::Utf8: return equalsIterator(it, other.getIterator<StringIteratorUTF8>(), points);
    case StringEncoding::Utf16: return equalsIterator(it, other.getIterator<StringIteratorUTF16>(), points);
    }
    Assert::unreachable();
}

[[nodiscard]] inline auto SC::StringView::getNullTerminatedNative() const
{
#if SC_PLATFORM_WINDOWS
    SC_ASSERT_RELEASE(hasNullTerm && (getEncoding() == StringEncoding::Utf16));
    return reinterpret_cast<const wchar_t*>(text);
#else
    SC_ASSERT_RELEASE(hasNullTerm && (getEncoding() == StringEncoding::Utf8 || getEncoding() == StringEncoding::Ascii));
    return text;
#endif
}

[[nodiscard]] constexpr bool SC::StringView::operator==(StringView other) const
{
    if (hasCompatibleEncoding(other))
    {
        if (textSizeInBytes != other.textSizeInBytes)
            return false;
        if (__builtin_is_constant_evaluated())
        {
            auto it1 = text;
            auto it2 = other.text;
            auto sz  = textSizeInBytes;
            for (size_t idx = 0; idx < sz; ++idx)
                if (it1[idx] != it2[idx])
                    return false;
        }
        else
        {
            return memcmp(text, other.text, textSizeInBytes) == 0;
        }
    }
    size_t commonOverlappingPoints = 0;
    return fullyOverlaps(other, commonOverlappingPoints);
}

constexpr bool SC::StringView::fullyOverlaps(StringView other, size_t& commonOverlappingPoints) const
{
    commonOverlappingPoints = 0;
    switch (getEncoding())
    {
    case StringEncoding::Ascii: return equalsIterator<StringIteratorASCII>(other, commonOverlappingPoints);
    case StringEncoding::Utf8: return equalsIterator<StringIteratorUTF8>(other, commonOverlappingPoints);
    case StringEncoding::Utf16: return equalsIterator<StringIteratorUTF16>(other, commonOverlappingPoints);
    }
    Assert::unreachable();
}

constexpr SC::size_t SC::StringView::sizeInBytesIncludingTerminator() const
{
    SC_ASSERT_RELEASE(hasNullTerm);
    return textSizeInBytes > 0 ? textSizeInBytes + StringEncodingGetSize(getEncoding()) : 0;
}

template <typename Func>
constexpr auto SC::StringView::withIterator(Func&& func) const
{
    switch (getEncoding())
    {
    case StringEncoding::Ascii: return func(getIterator<StringIteratorASCII>());
    case StringEncoding::Utf8: return func(getIterator<StringIteratorUTF8>());
    case StringEncoding::Utf16: return func(getIterator<StringIteratorUTF16>());
    }
    Assert::unreachable();
}

template <typename Func>
constexpr auto SC::StringView::withIterators(StringView s1, StringView s2, Func&& func)
{
    return s1.withIterator([&s2, &func](auto it1)
                           { return s2.withIterator([&it1, &func](auto it2) { return func(it1, it2); }); });
}

constexpr bool SC::StringView::hasCompatibleEncoding(StringView str) const
{
    return StringEncodingAreBinaryCompatible(getEncoding(), str.getEncoding());
}

template <typename StringIterator>
inline SC::StringView SC::StringView::fromIterators(StringIterator from, StringIterator to)
{
    const ssize_t numBytes = to.bytesDistanceFrom(from);
    if (numBytes >= 0)
    {
        StringIterator fromEnd = from;
        fromEnd.setToEnd();
        if (fromEnd.bytesDistanceFrom(to) >= 0) // If current iterator of to is inside from range
            return StringView(from.getCurrentIt(), static_cast<size_t>(numBytes), false, StringIterator::getEncoding());
    }
    return StringView(); // TODO: Make StringView::fromIterators return bool to make it fallible
}

template <typename StringIterator>
inline SC::StringView SC::StringView::fromIteratorUntilEnd(StringIterator it)
{
    StringIterator endIt = it;
    endIt.setToEnd();
    const size_t numBytes = static_cast<size_t>(endIt.bytesDistanceFrom(it));
    return StringView(it.getCurrentIt(), numBytes, false, StringIterator::getEncoding());
}

template <typename StringIterator>
constexpr SC::StringView SC::StringView::fromIteratorFromStart(StringIterator it)
{
    StringIterator start = it;
    start.setToStart();
    const size_t numBytes = static_cast<size_t>(it.bytesDistanceFrom(start));
    return StringView(start.getCurrentIt(), numBytes, false, StringIterator::getEncoding());
}

constexpr SC::StringView SC::StringView::sliceStartBytes(size_t start) const
{
    if (start < sizeInBytes())
        return sliceStartLengthBytes(start, sizeInBytes() - start);
    SC_ASSERT_RELEASE(start < sizeInBytes());
    return StringView(text, 0, false, getEncoding());
}

constexpr SC::StringView SC::StringView::sliceStartEndBytes(size_t start, size_t end) const
{
    if (end >= start)
        return sliceStartLengthBytes(start, end - start);
    SC_ASSERT_RELEASE(end >= start);
    return StringView(text, 0, false, getEncoding());
}

constexpr SC::StringView SC::StringView::sliceStartLengthBytes(size_t start, size_t length) const
{
    if (start + length > sizeInBytes())
    {
        SC_ASSERT_RELEASE(start + length > sizeInBytes());
        return StringView(text, 0, false, getEncoding());
    }
    return StringView(text + start, length, hasNullTerm and (start + length == sizeInBytes()), getEncoding());
}
