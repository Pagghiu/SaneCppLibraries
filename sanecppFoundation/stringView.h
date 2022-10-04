#pragma once
#include "assert.h"
#include "compiler.h"
#include "limits.h"
#include "memory.h"
#include "span.h"
#include "stringUtility.h"

namespace sanecpp
{
class stringView
{
    span<const char_t> text;
    bool               hasNullTerm;

  public:
    constexpr stringView() : text(nullptr, 0), hasNullTerm(false) {}
    constexpr stringView(const char_t* text, size_t bytes, bool nullTerm) : text{text, bytes}, hasNullTerm(nullTerm) {}
    template <size_t N>
    constexpr stringView(const char (&text)[N]) : text{text, N - 1}, hasNullTerm(true)
    {}

    [[nodiscard]] constexpr const char_t* getText() const { return text.data; }

    [[nodiscard]] constexpr bool equals(stringView other) const { return text.equalsContent(other.text); }
    [[nodiscard]] constexpr bool operator==(stringView other) const { return equals(other); }
    [[nodiscard]] constexpr bool operator!=(stringView other) const { return not operator==(other); }
    [[nodiscard]] bool           operator==(const char_t* str) const { return text.equalsContent({str, strlen(str)}); }
    [[nodiscard]] bool           operator!=(const char_t* str) const { return not operator==(str); }
    [[nodiscard]] constexpr bool isEmpty() const { return text.data == nullptr; }
    [[nodiscard]] constexpr bool isNullTerminated() const { return hasNullTerm; }
    [[nodiscard]] constexpr size_t getLengthInBytes() const { return text.size; }
    [[nodiscard]] bool             parseInt32(int32_t* value) const;
};

} // namespace sanecpp
