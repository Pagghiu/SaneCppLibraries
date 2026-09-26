// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../../Libraries/Common/ResultErrorFormatter.h"
#include "BuildError.h"

namespace SC
{
/// @brief Formats an optional canonical English build-tool diagnostic into caller-owned storage.
inline ResultErrorFormat formatBuildError(BuildError error, Span<char> output)
{
    ResultErrorFormatter formatter(output);
    switch (error)
    {
    case BuildError::HelpOutputFailed: formatter.append("Build help could not be written"); break;
    case BuildError::ParseErrorOutputFailed: formatter.append("Build parse error could not be written"); break;
    case BuildError::InvalidOptionValue: formatter.append("Build option value is invalid"); break;
    case BuildError::AmbiguousOptionValue: formatter.append("Build option value is ambiguous"); break;
    case BuildError::InvalidOptionCombination: formatter.append("Build option combination is invalid"); break;
    case BuildError::InvalidShortOptionGroup: formatter.append("Build short-option group is invalid"); break;
    case BuildError::UnsupportedPlatform: formatter.append("Build operation is unsupported on this platform"); break;
    case BuildError::InvalidArguments: formatter.append("Build arguments are invalid"); break;
    case BuildError::UnexpectedForwardedArguments:
        formatter.append("Forwarded arguments are only supported by build run");
        break;
    case BuildError::MissingLongPathPolicyValue: formatter.append("Long-path policy value is missing"); break;
    case BuildError::UnknownConfigureOption: formatter.append("Build configure option is unknown"); break;
    case BuildError::DocumentationCommandFailed: formatter.append("Documentation command exited unsuccessfully"); break;
    case BuildError::UnsupportedAction: formatter.append("Build action is unsupported"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

inline ResultErrorFormat formatBuildError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != BuildResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatBuildError(static_cast<BuildError>(result.errorValue()), output);
}
} // namespace SC
