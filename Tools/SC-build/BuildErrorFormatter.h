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
    case BuildError::RuntimeShimLinkConflict:
        formatter.append("C++ runtime shims conflict with standard C++ runtime linking");
        break;
    case BuildError::ProjectRootMissing: formatter.append("Project root directory is missing"); break;
    case BuildError::LibraryDirectoryMissing: formatter.append("Sane C++ library directory is missing"); break;
    case BuildError::ProjectNameMissing: formatter.append("Project name is missing"); break;
    case BuildError::ProjectTargetNameMissing: formatter.append("Project target name is missing"); break;
    case BuildError::ProjectDirectoryMissing: formatter.append("Project directory is missing"); break;
    case BuildError::ProjectConfigurationMissing: formatter.append("Project has no configuration"); break;
    case BuildError::LongPathPolicyUnsupportedTarget:
        formatter.append("Long-path policy requires a runtime target");
        break;
    case BuildError::ConfigurationNameMissing: formatter.append("Configuration name is missing"); break;
    case BuildError::ConfigurationOutputPathMissing: formatter.append("Configuration output path is missing"); break;
    case BuildError::ConfigurationIntermediatePathMissing:
        formatter.append("Configuration intermediates path is missing");
        break;
    case BuildError::AbsoluteFileMaskUnsupported: formatter.append("File selection mask cannot be absolute"); break;
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
