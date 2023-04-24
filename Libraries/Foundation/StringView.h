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
    Span<const char> text;
    StringEncoding   encoding;
    bool             hasNullTerm;

  public:
    constexpr StringView() : text(nullptr, 0), encoding(StringEncoding::Ascii), hasNullTerm(false) {}
    constexpr StringView(Span<const char> text, bool nullTerm, StringEncoding encoding)
        : text(text), encoding(encoding), hasNullTerm(nullTerm)
    {}
    constexpr StringView(const char_t* text, size_t bytes, bool nullTerm, StringEncoding encoding)
        : text{text, bytes}, encoding(encoding), hasNullTerm(nullTerm)
    {}
    StringView(Span<const wchar_t> text, bool nullTerm, StringEncoding encoding)
        : text{reinterpret_cast<const char_t*>(text.data()), text.sizeInBytes()}, encoding(encoding),
          hasNullTerm(nullTerm)
    {}

    SpanVoid<const void> toVoidSpan() const { return text; }

    template <size_t N>
    constexpr StringView(const char (&text)[N]) : text{text, N - 1}, encoding(StringEncoding::Ascii), hasNullTerm(true)
    {}
#if SC_PLATFORM_WINDOWS
    template <size_t N>
    StringView(const wchar_t (&text)[N])
        : text{reinterpret_cast<const char*>(text), (N - 1) * sizeof(wchar_t)}, encoding(StringEncoding::Utf16),
          hasNullTerm(true)
    {}
#endif

    [[nodiscard]] constexpr StringEncoding getEncoding() const { return encoding; }
    [[nodiscard]] constexpr const char_t*  bytesWithoutTerminator() const { return text.data(); }
    [[nodiscard]] constexpr const char_t*  bytesIncludingTerminator() const
    {
        SC_RELEASE_ASSERT(hasNullTerm);
        return text.data();
    }
#if SC_PLATFORM_WINDOWS
    [[nodiscard]] const wchar_t* getNullTerminatedNative() const
    {
        SC_RELEASE_ASSERT(hasNullTerm && (encoding == StringEncoding::Utf16));
        return reinterpret_cast<const wchar_t*>(text.data());
    }
#else
    [[nodiscard]] const char_t* getNullTerminatedNative() const
    {
        SC_RELEASE_ASSERT(hasNullTerm && (encoding == StringEncoding::Utf8 || encoding == StringEncoding::Ascii));
        return text.data();
    }
#endif

    [[nodiscard]] StringComparison compareASCII(StringView other) const;

    [[nodiscard]] bool operator<(StringView other) const { return compareASCII(other) == StringComparison::Smaller; }

    template <typename StringIterator>
    StringIterator getIterator() const
    {
        return StringIterator(bytesWithoutTerminator(), bytesWithoutTerminator() + sizeInBytes());
    }

    [[nodiscard]] bool operator==(StringView other) const
    {
        return hasCompatibleEncoding(other) && text.sizeInBytes() == other.text.sizeInBytes()
                   ? memcmp(text.data(), other.text.data(), text.sizeInBytes()) == 0
                   : false;
    }
    [[nodiscard]] bool             operator!=(StringView other) const { return not operator==(other); }
    [[nodiscard]] constexpr bool   isEmpty() const { return text.data() == nullptr || text.sizeInBytes() == 0; }
    [[nodiscard]] constexpr bool   isNullTerminated() const { return hasNullTerm; }
    [[nodiscard]] constexpr size_t sizeInBytes() const { return text.sizeInBytes(); }

    [[nodiscard]] constexpr size_t sizeInBytesIncludingTerminator() const
    {
        SC_RELEASE_ASSERT(hasNullTerm);
        return text.sizeInBytes() > 0 ? text.sizeInBytes() + StringEncodingGetSize(encoding) : 0;
    }
    [[nodiscard]] bool endsWith(char_t c) const { return isEmpty() ? false : text.data()[text.sizeInBytes() - 1] == c; }
    [[nodiscard]] bool startsWith(char_t c) const { return isEmpty() ? false : text.data()[0] == c; }
    [[nodiscard]] bool startsWith(const StringView str) const;
    [[nodiscard]] bool endsWith(const StringView str) const;

    [[nodiscard]] bool containsASCIICharacter(char c) const
    {
        return getIterator<StringIteratorASCII>().advanceUntilMatches(c);
    }
    [[nodiscard]] bool hasCompatibleEncoding(StringView str) const
    {
        return StringEncodingAreBinaryCompatible(encoding, str.encoding);
    }

    /// Returns a StringView from two iterators. The from iterator will be shortened until the start of to
    template <typename StringIterator>
    static StringView fromIterators(StringIterator from, StringIterator to)
    {
        // TODO: Make StringView::fromIterators return bool to make it fallible
        if (to.getIt() <= from.getEnd() && to.getIt() >= from.getIt())
        {
            SC_DEBUG_ASSERT(to.getIt() >= from.getIt());
            return StringView(from.getIt(), static_cast<size_t>(to.getIt() - from.getIt()), false,
                              StringIterator::getEncoding());
        }
        return StringView();
    }

    /// Returns a section of a string.
    /// @param it The index to the beginning of the specified portion of StringView. The substring includes the
    /// characters up to iterator end.
    template <typename StringIterator>
    static StringView fromIteratorUntilEnd(StringIterator it)
    {
        SC_DEBUG_ASSERT(it.getEnd() >= it.getIt());
        return StringView(it.getIt(), static_cast<size_t>(it.getEnd() - it.getIt()), false,
                          StringIterator::getEncoding());
    }

    /// Returns a section of a string.
    /// @param it The index to the beginning of the specified portion of StringView. The substring includes the
    /// characters from iterator start up to its current position
    template <typename StringIterator>
    static StringView fromIteratorFromStart(StringIterator it)
    {
        SC_DEBUG_ASSERT(it.getIt() >= it.getStart());
        return StringView(it.getStart(), static_cast<size_t>(it.getIt() - it.getStart()), false,
                          StringIterator::getEncoding());
    }

    size_t sizeASCII() const { return sizeInBytes(); }

    /// Returns a section of a string.
    /// @param start The index to the beginning of the specified portion of StringView.
    /// @param end The index to the end of the specified portion of StringView. The substring includes the characters up
    /// to, but not including, the character indicated by end.
    template <typename StringIterator = StringIteratorASCII>
    [[nodiscard]] StringView sliceStartEnd(size_t start, size_t end) const;

    /// Returns a section of a string.
    /// @param start The index to the beginning of the specified portion of StringView.
    /// @param length The number of characters to include. The substring includes the characters up to, but not
    /// including, the character indicated by end.
    template <typename StringIterator = StringIteratorASCII>
    [[nodiscard]] StringView sliceStartLength(size_t start, size_t length) const
    {
        return sliceStartEnd<StringIterator>(start, start + length);
    }

    /// Returns a section of a string.
    /// @param start The index to the beginning of the specified portion of StringView.
    template <typename StringIterator = StringIteratorASCII>
    [[nodiscard]] StringView sliceStart(size_t start) const;

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
                (void)it.skipNext(); // No need to check return result, we already checked in advanceUntilMatches
                continueSplit = !it.isEmpty();
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
    [[nodiscard]] bool isIntegerNumber() const;

    /// Parses int32, returning false if it fails
    [[nodiscard]] bool parseInt32(int32_t* value) const;

  private:
    struct Internal;
};

#if SC_PLATFORM_WINDOWS
#define SC_STR_L(str) L##str
#else
#define SC_STR_L(str) str
#endif
namespace SC
{
#if SC_MSVC
constexpr inline SC::StringView operator"" _a8(const char* txt, size_t sz)
{
    return SC::StringView(txt, sz, true, SC::StringEncoding::Ascii);
}
constexpr inline SC::StringView operator"" _u8(const char* txt, size_t sz)
{
    return SC::StringView(txt, sz, true, SC::StringEncoding::Utf8);
}
#else
constexpr inline SC::StringView operator"" _a8(const SC::char_t* txt, SC::size_t sz)
{
    return SC::StringView(txt, sz, true, SC::StringEncoding::Ascii);
}
constexpr inline SC::StringView operator"" _u8(const SC::char_t* txt, SC::size_t sz)
{
    return SC::StringView(txt, sz, true, SC::StringEncoding::Utf8);
}
#endif
} // namespace SC
