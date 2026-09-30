// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_STRING_SPAN_ERROR_DEFINITION_H
#if SC_FOUNDATION_STRING_SPAN_ERROR_DEFINITION_H != 1
#error "StringSpanError.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_STRING_SPAN_ERROR_DEFINITION_H 1

#include "Result.h"

namespace SC
{
/// @brief Stable failures of the shared StringSpan native-output helper.
enum class StringSpanError : uint32_t
{
    DestinationOffsetInvalid = 1,
    DestinationTooSmall,
    NativeConversionFailed,
    EncodingUnsupported,
};

/// @brief Category owned by the foundational StringSpan type, not by a higher-level string library.
static constexpr ResultCategory StringSpanResultCategory = ResultCategory(19);
} // namespace SC

#endif // SC_FOUNDATION_STRING_SPAN_ERROR_DEFINITION_H
