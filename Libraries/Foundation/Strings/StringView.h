// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Language/Span.h"
#include "StringIterator.h"

namespace SC
{
struct SC_COMPILER_EXPORT StringView;
struct StringViewTokenizer;

} // namespace SC

struct SC::StringView
{
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

    template <typename StringIterator1, typename StringIterator2>
    static constexpr bool equalsIterator(StringIterator1 t1, StringIterator2 t2, size_t& points)
    {
        typename StringIterator1::CodePoint c1 = 0;
        typename StringIterator2::CodePoint c2 = 0;
        while (t1.advanceRead(c1) and t2.advanceRead(c2))
        {
            if (static_cast<uint32_t>(c1) != static_cast<uint32_t>(c2))
            {
                return false;
            }
            points++;
        }
        return t1.isAtEnd() and t2.isAtEnd();
    }

    template <typename StringIterator>
    constexpr bool equalsIterator(StringView other, size_t& points) const
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

    template <typename T>
    struct identity
    {
    };

    template <typename Type>
    constexpr StringIteratorASCII getIterator(identity<Type>) const
    {
        return StringIteratorASCII(text, text + textSizeInBytes);
    }
    constexpr StringIteratorUTF8 getIterator(identity<StringIteratorUTF8>) const
    {
        return StringIteratorUTF8(text, text + textSizeInBytes);
    }
    constexpr StringIteratorUTF16 getIterator(identity<StringIteratorUTF16>) const
    {
        return StringIteratorUTF16(text, text + textSizeInBytes);
    }

  public:
    constexpr StringView()
        : text(nullptr), textSizeInBytes(0), encoding(static_cast<SizeType>(StringEncoding::Ascii)), hasNullTerm(false)
    {}
    constexpr StringView(Span<char> textSpan, bool nullTerm, StringEncoding encoding)
        : text(textSpan.data()), textSizeInBytes(static_cast<SizeType>(textSpan.sizeInBytes())),
          encoding(static_cast<SizeType>(encoding)), hasNullTerm(nullTerm)
    {
        static_assert(sizeof(StringView) == sizeof(void*) + sizeof(SizeType), "StringView wrong size");
        SC_ASSERT_DEBUG(textSpan.sizeInBytes() <= MaxLength);
    }
    constexpr StringView(Span<const char> textSpan, bool nullTerm, StringEncoding encoding)
        : text(textSpan.data()), textSizeInBytes(static_cast<SizeType>(textSpan.sizeInBytes())),
          encoding(static_cast<SizeType>(encoding)), hasNullTerm(nullTerm)
    {
        SC_ASSERT_DEBUG(textSpan.sizeInBytes() <= MaxLength);
    }
    constexpr StringView(const char* text, size_t numBytes, bool nullTerm, StringEncoding encoding)
        : text(text), textSizeInBytes(static_cast<SizeType>(numBytes)), encoding(static_cast<SizeType>(encoding)),
          hasNullTerm(nullTerm)
    {
        SC_ASSERT_DEBUG(numBytes <= MaxLength);
    }

    template <size_t N>
    constexpr StringView(const char (&text)[N])
        : text(text), textSizeInBytes(N - 1), encoding(static_cast<SizeType>(StringEncoding::Ascii)), hasNullTerm(true)
    {}

#if SC_PLATFORM_WINDOWS
    template <size_t N>
    constexpr StringView(const wchar_t (&text)[N])
        : textWide(text), textSizeInBytes((N - 1) * sizeof(wchar_t)),
          encoding(static_cast<SizeType>(StringEncoding::Wide)), hasNullTerm(true)
    {}

    constexpr StringView(const wchar_t* text, size_t numBytes, bool nullTerm)
        : textWide(text), textSizeInBytes(static_cast<SizeType>(numBytes)),
          encoding(static_cast<SizeType>(StringEncoding::Wide)), hasNullTerm(nullTerm)
    {
        SC_ASSERT_DEBUG(numBytes <= MaxLength);
    }
    constexpr StringView(Span<const wchar_t> textSpan, bool nullTerm)
        : textWide(textSpan.data()), textSizeInBytes(static_cast<SizeType>(textSpan.sizeInBytes())),
          encoding(static_cast<SizeType>(StringEncoding::Wide)), hasNullTerm(nullTerm)
    {
        SC_ASSERT_DEBUG(textSpan.sizeInBytes() <= MaxLength);
    }
#endif
    [[nodiscard]] constexpr StringEncoding getEncoding() const { return static_cast<StringEncoding>(encoding); }
    [[nodiscard]] constexpr const char*    bytesWithoutTerminator() const { return text; }
    [[nodiscard]] constexpr const char*    bytesIncludingTerminator() const
    {
        SC_ASSERT_RELEASE(hasNullTerm);
        return text;
    }
#if SC_PLATFORM_WINDOWS
    [[nodiscard]] const wchar_t* getNullTerminatedNative() const
    {
        SC_ASSERT_RELEASE(hasNullTerm && (getEncoding() == StringEncoding::Utf16));
        return reinterpret_cast<const wchar_t*>(text);
    }
#else
    [[nodiscard]] const char* getNullTerminatedNative() const
    {
        SC_ASSERT_RELEASE(hasNullTerm &&
                          (getEncoding() == StringEncoding::Utf8 || getEncoding() == StringEncoding::Ascii));
        return text;
    }
#endif

    constexpr Span<const char> toCharSpan() const { return {text, textSizeInBytes}; }
    Span<const uint8_t> toBytesSpan() const { return Span<const uint8_t>::reinterpret_bytes(text, textSizeInBytes); }

    enum class Comparison
    {
        Smaller = -1,
        Equals  = 0,
        Bigger  = 1
    };

    [[nodiscard]] Comparison compare(StringView other) const;

    [[nodiscard]] bool operator<(StringView other) const { return compare(other) == Comparison::Smaller; }

    template <typename StringIterator>
    constexpr StringIterator getIterator() const
    {
        // For GCC complaining about specialization in non-namespace scope
        return getIterator(identity<StringIterator>());
    }

    [[nodiscard]] constexpr bool operator!=(StringView other) const { return not operator==(other); }
    [[nodiscard]] constexpr bool operator==(StringView other) const
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

    constexpr bool fullyOverlaps(StringView other, size_t& commonOverlappingPoints) const
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

    [[nodiscard]] constexpr bool isEmpty() const { return text == nullptr || textSizeInBytes == 0; }
    [[nodiscard]] constexpr bool isNullTerminated() const { return hasNullTerm; }

    [[nodiscard]] constexpr size_t sizeInBytes() const { return textSizeInBytes; }

    [[nodiscard]] constexpr size_t sizeInBytesIncludingTerminator() const
    {
        SC_ASSERT_RELEASE(hasNullTerm);
        return textSizeInBytes > 0 ? textSizeInBytes + StringEncodingGetSize(getEncoding()) : 0;
    }

    template <typename Func>
    [[nodiscard]] constexpr auto withIterator(Func&& func) const
    {
        switch (getEncoding())
        {
        case StringEncoding::Ascii: return func(getIterator<StringIteratorASCII>());
        case StringEncoding::Utf8: return func(getIterator<StringIteratorUTF8>());
        case StringEncoding::Utf16: return func(getIterator<StringIteratorUTF16>());
        }
        Assert::unreachable();
    }

    [[nodiscard]] bool endsWithChar(StringCodePoint c) const;
    [[nodiscard]] bool startsWithChar(StringCodePoint c) const;
    [[nodiscard]] bool startsWith(const StringView str) const;
    [[nodiscard]] bool endsWith(const StringView str) const;
    // TODO: containsString will assert if strings have non compatible encoding
    [[nodiscard]] bool containsString(const StringView str) const;
    [[nodiscard]] bool containsChar(StringCodePoint c) const;

    [[nodiscard]] constexpr bool hasCompatibleEncoding(StringView str) const
    {
        return StringEncodingAreBinaryCompatible(getEncoding(), str.getEncoding());
    }

    /// Returns a StringView from two iterators. The from iterator will be shortened until the start of to
    template <typename StringIterator>
    static StringView fromIterators(StringIterator from, StringIterator to)
    {
        const ssize_t numBytes = to.bytesDistanceFrom(from);
        if (numBytes >= 0)
        {
            StringIterator fromEnd = from;
            fromEnd.setToEnd();
            if (fromEnd.bytesDistanceFrom(to) >= 0) // If current iterator of to is inside from range
                return StringView(from.getCurrentIt(), static_cast<size_t>(numBytes), false,
                                  StringIterator::getEncoding());
        }
        return StringView(); // TODO: Make StringView::fromIterators return bool to make it fallible
    }

    /// Returns a section of a string.
    /// @param it The index to the beginning of the specified portion of StringView. The substring includes the
    /// characters up to iterator end.
    template <typename StringIterator>
    static StringView fromIteratorUntilEnd(StringIterator it)
    {
        StringIterator endIt = it;
        endIt.setToEnd();
        const size_t numBytes = static_cast<size_t>(endIt.bytesDistanceFrom(it));
        return StringView(it.getCurrentIt(), numBytes, false, StringIterator::getEncoding());
    }

    /// Returns a section of a string.
    /// @param it The index to the beginning of the specified portion of StringView. The substring includes the
    /// characters from iterator start up to its current position
    template <typename StringIterator>
    static constexpr StringView fromIteratorFromStart(StringIterator it)
    {
        StringIterator start = it;
        start.setToStart();
        const size_t numBytes = static_cast<size_t>(it.bytesDistanceFrom(start));
        return StringView(start.getCurrentIt(), numBytes, false, StringIterator::getEncoding());
    }

    /// Returns a section of a string.
    /// @param start The index to the beginning of the specified portion of StringView.
    /// @param end The index to the end of the specified portion of StringView. The substring includes the characters up
    /// to, but not including, the character indicated by end.
    [[nodiscard]] StringView sliceStartEnd(size_t start, size_t end) const;

    /// Returns a section of a string.
    /// @param start The offset from the beginning of the specified portion of StringView.
    /// @param length The number of code points to include.
    [[nodiscard]] StringView sliceStartLength(size_t start, size_t length) const;

    /// Returns a section of a string.
    /// @param offset The offset from the beginning of the specified portion of StringView.
    [[nodiscard]] StringView sliceStart(size_t offset) const;

    /// Returns a section of a string.
    /// @param offset The index to the beginning of the specified portion of StringView.
    [[nodiscard]] StringView sliceEnd(size_t offset) const;

    [[nodiscard]] StringView trimEndingChar(StringCodePoint c) const;

    [[nodiscard]] StringView trimStartingChar(StringCodePoint c) const;

    [[nodiscard]] constexpr StringView sliceStartBytes(size_t start) const
    {
        if (start < sizeInBytes())
            return sliceStartLengthBytes(start, sizeInBytes() - start);
        SC_ASSERT_RELEASE(start < sizeInBytes());
        return StringView(text, 0, false, getEncoding());
    }

    [[nodiscard]] constexpr StringView sliceStartEndBytes(size_t start, size_t end) const
    {
        if (end >= start)
            return sliceStartLengthBytes(start, end - start);
        SC_ASSERT_RELEASE(end >= start);
        return StringView(text, 0, false, getEncoding());
    }

    [[nodiscard]] constexpr StringView sliceStartLengthBytes(size_t start, size_t length) const
    {
        if (start + length > sizeInBytes())
        {
            SC_ASSERT_RELEASE(start + length > sizeInBytes());
            return StringView(text, 0, false, getEncoding());
        }
        return StringView(text + start, length, hasNullTerm and (start + length == sizeInBytes()), getEncoding());
    }

    /// If the current view is an integer number, returns true
    [[nodiscard]] bool isIntegerNumber() const;

    /// If the current view is a floating number, returns true
    [[nodiscard]] bool isFloatingNumber() const;

    /// Parses int32, returning false if it fails
    [[nodiscard]] bool parseInt32(int32_t& value) const;

    /// Parses float, returning false if it fails
    [[nodiscard]] bool parseFloat(float& value) const;

    /// Parses double, returning false if it fails
    [[nodiscard]] bool parseDouble(double& value) const;
};

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

#if SC_PLATFORM_WINDOWS
#define SC_STR_NATIVE(str) L##str
#else
#define SC_STR_NATIVE(str) str
#endif
namespace SC
{
constexpr inline SC::StringView operator""_a8(const char* txt, size_t sz)
{
    return SC::StringView(txt, sz, true, SC::StringEncoding::Ascii);
}
constexpr inline SC::StringView operator""_u8(const char* txt, size_t sz)
{
    return SC::StringView(txt, sz, true, SC::StringEncoding::Utf8);
}
constexpr inline SC::StringView operator""_u16(const char* txt, size_t sz)
{
    const bool isNullTerminated = sz > 0 and sz % 2 == 1 and txt[sz - 1] == 0;
    return SC::StringView(txt, isNullTerminated ? sz - 1 : sz, isNullTerminated, SC::StringEncoding::Utf16);
}
} // namespace SC
