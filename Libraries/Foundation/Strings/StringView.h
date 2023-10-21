// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Language/Span.h"
#include "StringIterator.h"

namespace SC
{
struct SC_COMPILER_EXPORT StringView;
struct SC_COMPILER_EXPORT StringViewTokenizer;
struct SC_COMPILER_EXPORT StringAlgorithms;

} // namespace SC

struct SC::StringView
{
    constexpr StringView();
    constexpr StringView(Span<char> textSpan, bool nullTerm, StringEncoding encoding);
    constexpr StringView(Span<const char> textSpan, bool nullTerm, StringEncoding encoding);
    constexpr StringView(const char* text, size_t numBytes, bool nullTerm, StringEncoding encoding);

    static StringView fromNullTerminated(const char* text, StringEncoding encoding);

    template <size_t N>
    constexpr StringView(const char (&text)[N]);

#if SC_PLATFORM_WINDOWS
    template <size_t N>
    constexpr StringView(const wchar_t (&text)[N]);
    constexpr StringView(const wchar_t* text, size_t numBytes, bool nullTerm);
    constexpr StringView(Span<const wchar_t> textSpan, bool nullTerm);
#endif

    [[nodiscard]] constexpr StringEncoding getEncoding() const { return static_cast<StringEncoding>(encoding); }

    [[nodiscard]] constexpr const char* bytesWithoutTerminator() const { return text; }

    [[nodiscard]] constexpr const char* bytesIncludingTerminator() const;

    auto getNullTerminatedNative() const;

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

    template <typename Func>
    [[nodiscard]] constexpr auto withIterator(Func&& func) const;

    template <typename Func>
    [[nodiscard]] static constexpr auto withIterators(StringView s1, StringView s2, Func&& func);

    template <typename StringIterator>
    constexpr StringIterator getIterator() const;

    [[nodiscard]] constexpr bool operator!=(StringView other) const { return not operator==(other); }

    [[nodiscard]] constexpr bool operator==(StringView other) const;

    [[nodiscard]] constexpr bool fullyOverlaps(StringView other, size_t& commonOverlappingPoints) const;

    [[nodiscard]] constexpr bool isEmpty() const { return text == nullptr or textSizeInBytes == 0; }

    [[nodiscard]] constexpr bool isNullTerminated() const { return hasNullTerm; }

    [[nodiscard]] constexpr size_t sizeInBytes() const { return textSizeInBytes; }

    [[nodiscard]] constexpr size_t sizeInBytesIncludingTerminator() const;

    /// Returns true if this StringView ends with c
    [[nodiscard]] bool endsWithChar(StringCodePoint c) const;

    /// Returns true if this StringView starts with c
    [[nodiscard]] bool startsWithChar(StringCodePoint c) const;

    /// Returns true if this StringView starts with str
    [[nodiscard]] bool startsWith(const StringView str) const;

    /// Returns true if this StringView ends with str
    [[nodiscard]] bool endsWith(const StringView str) const;

    /// Returns true if this StringView contains str
    // TODO: containsString will assert if strings have non compatible encoding
    [[nodiscard]] bool containsString(const StringView str) const;

    /// Returns true if this StringView contains c
    [[nodiscard]] bool containsChar(StringCodePoint c) const;

    [[nodiscard]] constexpr bool hasCompatibleEncoding(StringView str) const;

    /// Returns a StringView from two iterators. The from iterator will be shortened until the start of to
    template <typename StringIterator>
    static StringView fromIterators(StringIterator from, StringIterator to);

    /// Returns a section of a string.
    /// @param it The index to the beginning of the specified portion of StringView. The substring includes the
    /// characters up to iterator end.
    template <typename StringIterator>
    static StringView fromIteratorUntilEnd(StringIterator it);

    /// Returns a section of a string.
    /// @param it The index to the beginning of the specified portion of StringView. The substring includes the
    /// characters from iterator start up to its current position
    template <typename StringIterator>
    static constexpr StringView fromIteratorFromStart(StringIterator it);

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

    /// Trims (removes) all code points equal to c at the end of the string
    [[nodiscard]] StringView trimEndingChar(StringCodePoint c) const;

    /// Trims (removes) all code points equal to c at the start of the string
    [[nodiscard]] StringView trimStartingChar(StringCodePoint c) const;

    [[nodiscard]] constexpr StringView sliceStartBytes(size_t start) const;

    [[nodiscard]] constexpr StringView sliceStartEndBytes(size_t start, size_t end) const;

    [[nodiscard]] constexpr StringView sliceStartLengthBytes(size_t start, size_t length) const;

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

struct SC::StringAlgorithms
{
    [[nodiscard]] static bool matchWildcard(StringView s1, StringView s2);

  private:
    template <typename StringIterator1, typename StringIterator2>
    [[nodiscard]] static bool matchWildcardIterator(StringIterator1 pattern, StringIterator2 text);
};

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
