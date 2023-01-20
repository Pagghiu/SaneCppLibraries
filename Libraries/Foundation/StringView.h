// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Assert.h"
#include "Compiler.h"
#include "Limits.h"
#include "Memory.h"
#include "Span.h"

namespace SC
{
struct StringView;
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
        return StringIterator(bytesWithoutTerminator(), bytesWithoutTerminator() + sizeInBytesWithoutTerminator());
    }

    template <typename StringIterator>
    StringFunctions<StringIterator> functions() const
    {
        return *this;
    }

    [[nodiscard]] bool             operator==(StringView other) const { return text.equalsContent(other.text); }
    [[nodiscard]] bool             operator!=(StringView other) const { return not operator==(other); }
    [[nodiscard]] constexpr bool   isEmpty() const { return text.data == nullptr || text.size == 0; }
    [[nodiscard]] constexpr bool   isNullTerminated() const { return hasNullTerm; }
    [[nodiscard]] constexpr size_t sizeInBytesWithoutTerminator() const { return text.size; }
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
        if (str.sizeInBytesWithoutTerminator() <= sizeInBytesWithoutTerminator())
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
};

#if SC_MSVC
inline SC::StringView operator"" _sv(const char* txt, size_t sz) { return SC::StringView(txt, sz, true); }
#else
inline SC::StringView operator"" _sv(const SC::char_t* txt, SC::size_t sz) { return SC::StringView(txt, sz, true); }
#endif
