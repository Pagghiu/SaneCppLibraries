#pragma once
#include "span.h"
#include "types.h"
namespace sanecpp
{
[[nodiscard]] constexpr bool isDigit(char_t c) { return c >= '0' && c <= '9'; }
[[nodiscard]] constexpr bool isSign(char_t c) { return c == '+' || c == '-'; }
[[nodiscard]] constexpr bool isDigitOrSign(char_t c) { return isDigit(c) || isSign(c); }
[[nodiscard]] bool           isIntegerNumber(span<const char_t> text);
} // namespace sanecpp
