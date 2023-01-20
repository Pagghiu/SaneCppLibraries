// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Span.h"
#include "Types.h"
#include "Vector.h"

namespace SC
{
[[nodiscard]] constexpr bool isDigit(char_t c) { return c >= '0' && c <= '9'; }
[[nodiscard]] constexpr bool isSign(char_t c) { return c == '+' || c == '-'; }
[[nodiscard]] constexpr bool isDigitOrSign(char_t c) { return isDigit(c) || isSign(c); }
[[nodiscard]] bool           isIntegerNumber(Span<const char_t> text);

} // namespace SC
