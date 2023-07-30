// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Span.h"
#include "StringIterator.h"

namespace SC
{
struct StringView;
struct StringViewTokenizer;

} // namespace SC

struct SC_EXPORT_SYMBOL SC::StringView
{
  private:
    union
    {
        const char*    textUtf8;
        const wchar_t* textUtf16;
    };
    size_t         textSizeInBytes;
    StringEncoding encoding;
    bool           hasNullTerm;

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
        SC_UNREACHABLE();
    }

    template <typename T>
    struct identity
    {
        typedef T type;
    };

    template <typename Type>
    constexpr StringIteratorASCII getIterator(identity<Type>) const
    {
        return StringIteratorASCII(textUtf8, textUtf8 + textSizeInBytes / sizeof(char));
    }
    constexpr StringIteratorUTF8 getIterator(identity<StringIteratorUTF8>) const
    {
        return StringIteratorUTF8(textUtf8, textUtf8 + textSizeInBytes / sizeof(char));
    }
    constexpr StringIteratorUTF16 getIterator(identity<StringIteratorUTF16>) const
    {
        return StringIteratorUTF16(textUtf16, textUtf16 + textSizeInBytes / sizeof(wchar_t));
    }

  public:
    constexpr StringView() : textUtf8(nullptr), textSizeInBytes(0), encoding(StringEncoding::Ascii), hasNullTerm(false)
    {}
    constexpr StringView(Span<char> textSpan, bool nullTerm, StringEncoding encoding)
        : textUtf8(textSpan.data()), textSizeInBytes(textSpan.sizeInBytes()), encoding(encoding), hasNullTerm(nullTerm)
    {}
    constexpr StringView(Span<const char> textSpan, bool nullTerm, StringEncoding encoding)
        : textUtf8(textSpan.data()), textSizeInBytes(textSpan.sizeInBytes()), encoding(encoding), hasNullTerm(nullTerm)
    {}
    constexpr StringView(const char_t* text, size_t bytes, bool nullTerm, StringEncoding encoding)
        : textUtf8(text), textSizeInBytes(bytes), encoding(encoding), hasNullTerm(nullTerm)
    {}
    constexpr StringView(const wchar_t* text, size_t bytes, bool nullTerm, StringEncoding encoding)
        : textUtf16(text), textSizeInBytes(bytes), encoding(encoding), hasNullTerm(nullTerm)
    {}
    constexpr StringView(Span<const wchar_t> textSpan, bool nullTerm, StringEncoding encoding)
        : textUtf16(textSpan.data()), textSizeInBytes(textSpan.sizeInBytes()), encoding(encoding), hasNullTerm(nullTerm)
    {}

    template <size_t N>
    constexpr StringView(const char (&text)[N])
        : textUtf8(text), textSizeInBytes(N - 1), encoding(StringEncoding::Ascii), hasNullTerm(true)
    {}
    template <size_t N>
    constexpr StringView(const wchar_t (&text)[N])
        : textUtf16(text), textSizeInBytes((N - 1) * sizeof(wchar_t)), encoding(StringEncoding::Utf16),
          hasNullTerm(true)
    {}

    [[nodiscard]] constexpr StringEncoding getEncoding() const { return encoding; }
    [[nodiscard]] constexpr const char_t*  bytesWithoutTerminator() const { return textUtf8; }
    [[nodiscard]] constexpr const char_t*  bytesIncludingTerminator() const
    {
        SC_RELEASE_ASSERT(hasNullTerm);
        return textUtf8;
    }
#if SC_PLATFORM_WINDOWS
    [[nodiscard]] const wchar_t* getNullTerminatedNative() const
    {
        SC_RELEASE_ASSERT(hasNullTerm && (encoding == StringEncoding::Utf16));
        return reinterpret_cast<const wchar_t*>(textUtf8);
    }
#else
    [[nodiscard]] const char_t* getNullTerminatedNative() const
    {
        SC_RELEASE_ASSERT(hasNullTerm && (encoding == StringEncoding::Utf8 || encoding == StringEncoding::Ascii));
        return textUtf8;
    }
#endif

    constexpr SpanVoid<const void> toVoidSpan() const { return {textUtf8, textSizeInBytes}; }
    constexpr Span<const char>     toCharSpan() const { return {textUtf8, textSizeInBytes}; }

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

    [[nodiscard]] constexpr bool operator==(StringView other) const
    {
        if (hasCompatibleEncoding(other))
        {
            if (textSizeInBytes != other.textSizeInBytes)
                return false;
            if (__builtin_is_constant_evaluated())
            {
                auto it1 = textUtf8;
                auto it2 = other.textUtf8;
                auto sz  = textSizeInBytes;
                for (size_t idx = 0; idx < sz; ++idx)
                    if (it1[idx] != it2[idx])
                        return false;
            }
            else
            {
                return memcmp(textUtf8, other.textUtf8, textSizeInBytes) == 0;
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
        SC_UNREACHABLE();
    }

    [[nodiscard]] bool           operator!=(StringView other) const { return not operator==(other); }
    [[nodiscard]] constexpr bool isEmpty() const { return textUtf8 == nullptr || textSizeInBytes == 0; }
    [[nodiscard]] constexpr bool isNullTerminated() const { return hasNullTerm; }

    [[nodiscard]] constexpr size_t sizeInBytes() const { return textSizeInBytes; }

    [[nodiscard]] constexpr size_t sizeInBytesIncludingTerminator() const
    {
        SC_RELEASE_ASSERT(hasNullTerm);
        return textSizeInBytes > 0 ? textSizeInBytes + StringEncodingGetSize(encoding) : 0;
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
        SC_UNREACHABLE();
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
        return StringEncodingAreBinaryCompatible(encoding, str.encoding);
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
        SC_RELEASE_ASSERT(start < sizeInBytes());
        return StringView(textUtf8, 0, false, encoding);
    }

    [[nodiscard]] constexpr StringView sliceStartEndBytes(size_t start, size_t end) const
    {
        if (end >= start)
            return sliceStartLengthBytes(start, end - start);
        SC_RELEASE_ASSERT(end >= start);
        return StringView(textUtf8, 0, false, encoding);
    }

    [[nodiscard]] constexpr StringView sliceStartLengthBytes(size_t start, size_t length) const
    {
        if (start + length > sizeInBytes())
        {
            SC_RELEASE_ASSERT(start + length > sizeInBytes());
            return StringView(textUtf8, 0, false, encoding);
        }
        return StringView(textUtf8 + start, length, hasNullTerm and (start + length == sizeInBytes()), encoding);
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

    enum Options
    {
        IncludeEmpty,
        SkipEmpty
    };
    StringViewTokenizer(StringView current) : current(current) {}
    [[nodiscard]] bool   tokenizeNext(Span<const StringCodePoint> separators, Options options);
    StringViewTokenizer& countTokens(Span<const StringCodePoint> separators);

  private:
    StringView current;
};

#if SC_PLATFORM_WINDOWS
#define SC_STR_NATIVE(str) L##str
#else
#define SC_STR_NATIVE(str) str
#endif
namespace SC
{
constexpr inline SC::StringView operator"" _a8(const char* txt, size_t sz)
{
    return SC::StringView(txt, sz, true, SC::StringEncoding::Ascii);
}
constexpr inline SC::StringView operator"" _u8(const char* txt, size_t sz)
{
    return SC::StringView(txt, sz, true, SC::StringEncoding::Utf8);
}
} // namespace SC
