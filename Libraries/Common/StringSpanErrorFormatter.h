// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_STRING_SPAN_ERROR_FORMATTER_DEFINITION_H
#if SC_FOUNDATION_STRING_SPAN_ERROR_FORMATTER_DEFINITION_H != 1
#error "StringSpanErrorFormatter.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_STRING_SPAN_ERROR_FORMATTER_DEFINITION_H 1

#include "ResultErrorFormatter.h"
#include "StringSpanError.h"

namespace SC
{
/// @brief Format a foundational StringSpan failure into caller-provided UTF-8 storage.
inline ResultErrorFormat formatStringSpanError(StringSpanError error, Span<char> output)
{
    ResultErrorFormatter formatter(output);
    switch (error)
    {
    case StringSpanError::DestinationOffsetInvalid:
        formatter.append("String span destination offset is invalid");
        break;
    case StringSpanError::DestinationTooSmall: formatter.append("String span destination is too small"); break;
    case StringSpanError::NativeConversionFailed:
        formatter.append("String span native conversion failed or destination is too small");
        break;
    case StringSpanError::EncodingUnsupported: formatter.append("String span encoding is unsupported"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

inline ResultErrorFormat formatStringSpanError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != StringSpanResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatStringSpanError(static_cast<StringSpanError>(result.errorValue()), output);
}
} // namespace SC

#endif // SC_FOUNDATION_STRING_SPAN_ERROR_FORMATTER_DEFINITION_H
