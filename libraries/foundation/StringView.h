#pragma once
#include "Assert.h"
#include "Compiler.h"
#include "Limits.h"
#include "Memory.h"
#include "Span.h"
#include "StringIterator.h"

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

    [[nodiscard]] constexpr Comparison compareASCII(StringView other) const { return text.compare(other.text); }

    [[nodiscard]] constexpr bool operator<(StringView other) const
    {
        return text.compare(other.text) == Comparison::Smaller;
    }

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

    [[nodiscard]] constexpr bool   operator==(StringView other) const { return text.equalsContent(other.text); }
    [[nodiscard]] constexpr bool   operator!=(StringView other) const { return not operator==(other); }
    [[nodiscard]] constexpr bool   isEmpty() const { return text.data == nullptr; }
    [[nodiscard]] constexpr bool   isNullTerminated() const { return hasNullTerm; }
    [[nodiscard]] constexpr size_t sizeInBytesWithoutTerminator() const { return text.size; }
    [[nodiscard]] constexpr size_t sizeInBytesIncludingTerminator() const { return text.size > 0 ? text.size + 1 : 0; }
    [[nodiscard]] bool             parseInt32(int32_t* value) const;
};

inline SC::StringView operator"" _sv(const SC::char_t* txt, SC::size_t sz) { return SC::StringView(txt, sz, true); }
