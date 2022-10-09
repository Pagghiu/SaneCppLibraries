#pragma once
#include "Assert.h"
#include "Compiler.h"
#include "Limits.h"
#include "Memory.h"
#include "Span.h"
#include "StringUtility.h"

namespace SC
{
class StringView
{
    Span<const char_t> text;
    bool               hasNullTerm;

  public:
    constexpr StringView() : text(nullptr, 0), hasNullTerm(false) {}
    constexpr StringView(const char_t* text, size_t bytes, bool nullTerm) : text{text, bytes}, hasNullTerm(nullTerm) {}
    template <size_t N>
    constexpr StringView(const char (&text)[N]) : text{text, N - 1}, hasNullTerm(true)
    {}

    [[nodiscard]] constexpr const char_t* getText() const { return text.data; }

    [[nodiscard]] constexpr Comparison compareASCII(StringView other) const { return text.compare(other.text); }

    [[nodiscard]] constexpr bool operator<(StringView other) const
    {
        return text.compare(other.text) == Comparison::Smaller;
    }
    [[nodiscard]] constexpr bool   operator==(StringView other) const { return text.equalsContent(other.text); }
    [[nodiscard]] constexpr bool   operator!=(StringView other) const { return not operator==(other); }
    [[nodiscard]] constexpr bool   isEmpty() const { return text.data == nullptr; }
    [[nodiscard]] constexpr bool   isNullTerminated() const { return hasNullTerm; }
    [[nodiscard]] constexpr size_t getLengthInBytes() const { return text.size; }
    [[nodiscard]] bool             parseInt32(int32_t* value) const;
};

} // namespace SC
