// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Assert.h"
#include "Compiler.h"
#include "InitializerList.h"
#include "Limits.h"
#include "Memory.h"
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
template <typename StringIterator>
struct StringFunctions;
} // namespace SC

struct SC::StringView
{
  private:
    Span<const char_t> text;
    bool               hasNullTerm;

  public:
    constexpr StringView() : text(nullptr, 0), hasNullTerm(false) {}
    constexpr StringView(const char_t* text, size_t bytes, bool nullTerm) : text{text, bytes}, hasNullTerm(nullTerm) {}
    template <size_t N>
    constexpr StringView(const char (&text)[N]) : text{text, N - 1}, hasNullTerm(true)
    {}

    [[nodiscard]] constexpr const char_t* bytesWithoutTerminator() const { return text.data; }
    [[nodiscard]] constexpr const char_t* bytesIncludingTerminator() const { return text.data; }

    [[nodiscard]] Comparison compareASCII(StringView other) const { return text.compare(other.text); }

    [[nodiscard]] bool operator<(StringView other) const { return text.compare(other.text) == Comparison::Smaller; }

    template <typename StringIterator>
    StringIterator getIterator() const
    {
        return StringIterator(bytesWithoutTerminator(), bytesWithoutTerminator() + sizeInBytes());
    }

    [[nodiscard]] bool             operator==(StringView other) const { return text.equalsContent(other.text); }
    [[nodiscard]] bool             operator!=(StringView other) const { return not operator==(other); }
    [[nodiscard]] constexpr bool   isEmpty() const { return text.data == nullptr || text.size == 0; }
    [[nodiscard]] constexpr bool   isNullTerminated() const { return hasNullTerm; }
    [[nodiscard]] constexpr size_t sizeInBytes() const { return text.size; }
    [[nodiscard]] constexpr size_t sizeInBytesIncludingTerminator() const { return text.size > 0 ? text.size + 1 : 0; }
    [[nodiscard]] bool             parseInt32(int32_t* value) const;
    [[nodiscard]] bool endsWith(char_t c) const { return isEmpty() ? false : text.data[text.size - 1] == c; }
    [[nodiscard]] bool startsWith(char_t c) const { return isEmpty() ? false : text.data[0] == c; }
    [[nodiscard]] bool startsWith(const StringView str) const
    {
        if (str.text.size <= text.size)
        {
            const StringView ours(text.data, str.text.size, false);
            return str == ours;
        }
        return false;
    }
    [[nodiscard]] bool endsWith(const StringView str) const
    {
        if (str.sizeInBytes() <= sizeInBytes())
        {
            const StringView ours(text.data + text.size - str.text.size, str.text.size, false);
            return str == ours;
        }
        return false;
    }

    [[nodiscard]] bool setSizeInBytesWithoutTerminator(size_t newSize)
    {
        if (newSize <= text.size)
        {
            text.size = newSize;
            return true;
        }
        return false;
    }

    /// Returns a StringView from two iterators. The from iterator will be shortened until the start of to
    template <typename StringIterator>
    static StringView fromIterators(StringIterator from, StringIterator to)
    {
        if (to.getStart() <= from.getEnd() && to.getStart() >= from.getStart())
        {
            return StringView(from.getStart(), to.getStart() - from.getStart(), false);
        }
        return StringView();
    }

    /// Returns a section of a string.
    /// @param from The index to the beginning of the specified portion of StringView. The substring includes the
    /// characters up to the end.
    template <typename StringIterator>
    static StringView fromIterator(StringIterator from)
    {
        return StringView(from.getStart(), from.getEnd() - from.getStart(), false);
    }
    template <typename StringIterator = StringIteratorASCII>
    size_t sizeCodePoints() const;

    template <>
    size_t sizeCodePoints<StringIteratorASCII>() const
    {
        return sizeInBytes();
    }
    /// Returns a section of a string.
    /// @param start The index to the beginning of the specified portion of StringView.
    /// @param end The index to the end of the specified portion of StringView. The substring includes the characters up
    /// to, but not including, the character indicated by end.
    template <typename StringIterator = StringIteratorASCII>
    [[nodiscard]] StringView sliceStartEnd(size_t start, size_t end) const
    {
        StringIterator it = getIterator<StringIterator>();
        SC_RELEASE_ASSERT(it.advanceCodePoints(start));
        StringIterator startIt = it;
        SC_RELEASE_ASSERT(start <= end && it.advanceCodePoints(end - start));
        const auto distance = it.bytesDistanceFrom(startIt);
        return StringView(startIt.getStart(), distance, start + distance == sizeInBytesIncludingTerminator());
    }

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
    [[nodiscard]] StringView sliceStart(size_t start) const
    {
        return sliceStartEnd<StringIterator>(start, start + sizeInBytes());
    }
    template <typename Lambda>
    [[nodiscard]] size_t splitASCII(char_t separator, Lambda&& lambda,
                                    SplitOptions options = {SplitOptions::SkipEmpty, SplitOptions::SkipSeparator})
    {
        return split<StringIteratorASCII>(separator, forward(lambda), options);
    }

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
};

#if SC_MSVC
inline SC::StringView operator"" _sv(const char* txt, size_t sz) { return SC::StringView(txt, sz, true); }
#else
inline SC::StringView operator"" _sv(const SC::char_t* txt, SC::size_t sz) { return SC::StringView(txt, sz, true); }
#endif
