// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Libraries/Common/ResultErrorFormatter.h"
#include "ToolsError.h"

namespace SC
{
namespace Tools
{
/// @brief Formats an optional canonical English tool diagnostic into caller-owned storage.
inline ResultErrorFormat formatToolsError(ToolsError error, Span<char> output)
{
    ResultErrorFormatter formatter(output);
    switch (error)
    {
    case ToolsError::UnsupportedFormatAction: formatter.append("Format action must be execute or check"); break;
    case ToolsError::ChildProcessExitedNonzero:
        formatter.append("Formatting process exited with a non-zero status");
        break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

inline ResultErrorFormat formatToolsError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != ToolsResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatToolsError(static_cast<ToolsError>(result.errorValue()), output);
}
} // namespace Tools
} // namespace SC
