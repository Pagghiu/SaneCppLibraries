// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Span.h"
#include "StringIterator.h"

namespace SC
{
struct StringView;
struct SplitOptions
{
    enum Value
    {
        None          = 0,
        SkipEmpty     = 1,
        SkipSeparator = 2
    };
    Value value;
    SplitOptions(std::initializer_list<Value> ilist)
    {
        value = None;
        for (auto v : ilist)
        {
            value = static_cast<Value>(static_cast<uint32_t>(value) | static_cast<uint32_t>(v));
        }
    }
    bool has(Value v) const { return (value & v) != None; }
};
enum class StringComparison
{
    Smaller = -1,
    Equals  = 0,
    Bigger  = 1
};

} // namespace SC

struct SC::StringView
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

    constexpr SpanVoid<const void> toVoidSpan() const { return {textUtf8, textSizeInBytes}; }

    constexpr Span<const char> toCharSpan() const { return {textUtf8, textSizeInBytes}; }

    template <size_t N>
    constexpr StringView(const char (&text)[N])
        : textUtf8(text), textSizeInBytes(N - 1), encoding(StringEncoding::Ascii), hasNullTerm(true)
    {}
    template <size_t N>
    StringView(const wchar_t (&text)[N])
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

    [[nodiscard]] StringComparison compareASCII(StringView other) const;

    [[nodiscard]] bool operator<(StringView other) const { return compareASCII(other) == StringComparison::Smaller; }

  private:
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
        return equals(other);
    }

  private:
    template <typename StringIterator1, typename StringIterator2>
    static constexpr bool equalsIterator(StringIterator1 t1, StringIterator2 t2)
    {
        typename StringIterator1::CodePoint c1 = 0;
        typename StringIterator2::CodePoint c2 = 0;
        while (t1.advanceRead(c1) and t2.advanceRead(c2))
        {
            if (static_cast<uint32_t>(c1) != static_cast<uint32_t>(c2))
            {
                return false;
            }
        }
        return t1.isAtEnd();
    }

    template <typename StringIterator>
    constexpr bool equalsIterator(StringView other) const
    {
        auto it = getIterator<StringIterator>();
        switch (other.getEncoding())
        {
        case StringEncoding::Ascii: return equalsIterator(it, other.getIterator<StringIteratorASCII>()); break;
        case StringEncoding::Utf8: return equalsIterator(it, other.getIterator<StringIteratorUTF8>()); break;
        case StringEncoding::Utf16: return equalsIterator(it, other.getIterator<StringIteratorUTF16>()); break;
        }
        SC_UNREACHABLE();
    }

    constexpr bool equals(StringView other) const
    {
        switch (getEncoding())
        {
        case StringEncoding::Ascii: return equalsIterator<StringIteratorASCII>(other);
        case StringEncoding::Utf8: return equalsIterator<StringIteratorUTF8>(other);
        case StringEncoding::Utf16: return equalsIterator<StringIteratorUTF16>(other);
        }
        SC_UNREACHABLE();
    }

  public:
    [[nodiscard]] bool           operator!=(StringView other) const { return not operator==(other); }
    [[nodiscard]] constexpr bool isEmpty() const { return textUtf8 == nullptr || textSizeInBytes == 0; }
    [[nodiscard]] constexpr bool isNullTerminated() const { return hasNullTerm; }

    size_t sizeASCII() const { return sizeInBytes(); }

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

    template <typename CharType>
    [[nodiscard]] constexpr bool endsWithChar(CharType c) const
    {
        return withIterator([c](auto it) { return it.endsWithChar(c); });
    }

    template <typename CharType>
    [[nodiscard]] constexpr bool startsWithChar(CharType c) const
    {
        return withIterator([c](auto it) { return it.startsWithChar(c); });
    }

    [[nodiscard]] bool startsWith(const StringView str) const;
    [[nodiscard]] bool endsWith(const StringView str) const;
    // TODO: containsString will assert if strings have non compatible encoding
    [[nodiscard]] bool containsString(const StringView str) const;

    template <typename CharType>
    [[nodiscard]] constexpr bool containsChar(CharType c) const
    {
        return withIterator([c](auto it) { return it.advanceUntilMatches(it.castCodePoint(c)); });
    }

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
    template <typename StringIterator = StringIteratorASCII>
    [[nodiscard]] constexpr StringView sliceStartEnd(size_t start, size_t end) const
    {
        auto it = getIterator<StringIterator>();
        SC_RELEASE_ASSERT(it.advanceCodePoints(start));
        auto startIt = it;
        SC_RELEASE_ASSERT(start <= end && it.advanceCodePoints(end - start));
        const size_t distance = static_cast<size_t>(it.bytesDistanceFrom(startIt));
        return StringView(startIt.getCurrentIt(), distance,
                          hasNullTerm and (start + distance == sizeInBytesIncludingTerminator()), encoding);
    }
    /// Returns a section of a string.
    /// @param start The index to the beginning of the specified portion of StringView.
    /// @param length The number of characters to include. The substring includes the characters up to, but not
    /// including, the character indicated by end.
    template <typename StringIterator = StringIteratorASCII>
    [[nodiscard]] constexpr StringView sliceStartLength(size_t start, size_t length) const
    {
        return sliceStartEnd<StringIterator>(start, start + length);
    }

    /// Returns a section of a string.
    /// @param start The index to the beginning of the specified portion of StringView.
    template <typename StringIterator = StringIteratorASCII>
    [[nodiscard]] constexpr StringView sliceStart(size_t start) const
    {
        auto it = getIterator<StringIterator>();
        SC_RELEASE_ASSERT(it.advanceCodePoints(start));
        auto startIt = it;
        it.setToEnd();
        const size_t distance = static_cast<size_t>(it.bytesDistanceFrom(startIt));
        return StringView(startIt.getCurrentIt(), distance,
                          hasNullTerm and (start + distance == sizeInBytesIncludingTerminator()), encoding);
    }

    template <typename Lambda>
    [[nodiscard]] size_t splitASCII(char_t separator, Lambda&& lambda,
                                    SplitOptions options = {SplitOptions::SkipEmpty, SplitOptions::SkipSeparator})
    {
        return split<StringIteratorASCII>(separator, forward<Lambda>(lambda), options);
    }

    // TODO: make split using iterator pattern instead of lambda so we can move it to cpp
    template <typename StringIterator, typename Lambda, typename CharacterType>
    [[nodiscard]] size_t split(CharacterType separator, Lambda&& lambda, SplitOptions options)
    {
        if (isEmpty())
            return 0;
        StringIterator it            = getIterator<StringIterator>();
        StringIterator itBackup      = it;
        size_t         numSplits     = 0;
        bool           continueSplit = true;
        while (continueSplit)
        {
            continueSplit        = it.advanceUntilMatches(separator);
            StringView component = StringView::fromIterators(itBackup, it);
            if (options.has(SplitOptions::SkipSeparator))
            {
                (void)it.stepForward(); // we already checked in advanceUntilMatches
                continueSplit = !it.isAtEnd();
            }
            // directory
            if (!component.isEmpty() || !options.has(SplitOptions::SkipEmpty))
            {
                numSplits++;
                lambda(component);
            }
            itBackup = it;
        }
        return numSplits;
    }

    /// If the current view is an integer number, returns true
    template <typename StringIterator>
    [[nodiscard]] constexpr bool isIntegerNumber() const
    {
        auto it = getIterator<StringIterator>();
        (void)it.advanceIfMatchesAny({'-', '+'}); // optional
        bool matchedAtLeastOneDigit = false;
        while (it.advanceIfMatchesRange('0', '9'))
            matchedAtLeastOneDigit = true;
        return matchedAtLeastOneDigit and it.isAtEnd();
    }

    /// If the current view is a floating number, returns true
    template <typename StringIterator>
    [[nodiscard]] constexpr bool isFloatingNumber() const
    {
        // TODO: Floating point exponential notation
        auto it = getIterator<StringIterator>();
        (void)it.advanceIfMatchesAny({'-', '+'}); // optional
        bool matchedAtLeastOneDigit = false;
        while (it.advanceIfMatchesRange('0', '9'))
            matchedAtLeastOneDigit = true;
        if (it.advanceIfMatches('.')) // optional
            while (it.advanceIfMatchesRange('0', '9'))
                matchedAtLeastOneDigit = true;
        return matchedAtLeastOneDigit and it.isAtEnd();
    }

    /// Parses int32, returning false if it fails
    [[nodiscard]] bool parseInt32(int32_t& value) const;

    /// Parses float, returning false if it fails
    [[nodiscard]] bool parseFloat(float& value) const;

    /// Parses double, returning false if it fails
    [[nodiscard]] bool parseDouble(double& value) const;
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
