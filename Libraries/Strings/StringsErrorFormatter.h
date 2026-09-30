// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Common/ResultErrorFormatter.h"
#include "StringsError.h"

namespace SC
{
//! @addtogroup group_strings
//! @{

/// @brief Formats a canonical English Strings diagnostic into caller-owned storage.
/// @details This opt-in header owns the canonical strings. Applications can use ResultErrorFormatter with translated
/// text instead.
inline ResultErrorFormat formatStringsError(StringsError error, Span<char> output)
{
    switch (error)
    {
    case StringsError::InvalidArgumentCount:
        return ResultErrorFormatter::formatMessage("Argument count is not valid", output);
    case StringsError::InsufficientArgumentStorage:
        return ResultErrorFormatter::formatMessage("Argument storage capacity is insufficient", output);
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
}

/// @brief Formats a plain Result when it contains a Strings error.
inline ResultErrorFormat formatStringsError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != StringsResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatStringsError(static_cast<StringsError>(result.errorValue()), output);
}

//! @}
} // namespace SC
