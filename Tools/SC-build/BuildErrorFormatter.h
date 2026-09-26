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
    case BuildError::WorkspaceNotFound: formatter.append("Build workspace was not found"); break;
    case BuildError::GeneratorArchitectureUnsupported:
        formatter.append("Build generator does not support the requested architecture");
        break;
    case BuildError::NoWorkspacesDefined: formatter.append("Build definition has no workspaces"); break;
    case BuildError::CoverageExecutableLaunchFailed:
        formatter.append("Coverage executable could not be launched");
        break;
    case BuildError::CoverageExecutableExitedUnsuccessfully:
        formatter.append("Coverage executable exited unsuccessfully");
        break;
    case BuildError::CoverageCompilerVersionUnavailable:
        formatter.append("Coverage compiler version could not be read");
        break;
    case BuildError::CoverageProfileMergeLaunchFailed:
        formatter.append("Coverage profile merger could not be launched");
        break;
    case BuildError::CoverageProfileMergeExitedUnsuccessfully:
        formatter.append("Coverage profile merger exited unsuccessfully");
        break;
    case BuildError::CoverageHtmlGenerationLaunchFailed:
        formatter.append("Coverage HTML generator could not be launched");
        break;
    case BuildError::CoverageHtmlGenerationExitedUnsuccessfully:
        formatter.append("Coverage HTML generator exited unsuccessfully");
        break;
    case BuildError::CoverageReportLaunchFailed:
        formatter.append("Coverage report generator could not be launched");
        break;
    case BuildError::CoverageReportExitedUnsuccessfully:
        formatter.append("Coverage report generator exited unsuccessfully");
        break;
    case BuildError::CoveragePercentageInvalid: formatter.append("Coverage percentage is invalid"); break;
    case BuildError::ExecutableExitedUnsuccessfully: formatter.append("Executable exited unsuccessfully"); break;
    case BuildError::ExecutablePathOutputMissing: formatter.append("Executable path output is missing"); break;
    case BuildError::ExecutablePathUnavailable: formatter.append("Build did not provide an executable path"); break;
    case BuildError::BuildCommandExitedUnsuccessfully: formatter.append("Build command exited unsuccessfully"); break;
    case BuildError::ExecutablePathQueryFailed: formatter.append("Executable path query failed"); break;
    case BuildError::RunRequiresSingleProject: formatter.append("Run requires one project"); break;
    case BuildError::RunRequiresExecutableTarget: formatter.append("Run requires an executable target"); break;
    case BuildError::CommandArgumentLimitExceeded: formatter.append("Native command has too many arguments"); break;
    case BuildError::ResolvedDependencyNotFound: formatter.append("Resolved build dependency was not found"); break;
    case BuildError::StripUnsupported: formatter.append("Stripping is unsupported for this target"); break;
    case BuildError::ExportedSymbolsUnsupported:
        formatter.append("Preserving exported symbols is unsupported for this target");
        break;
    case BuildError::PrintRequiresSingleProject: formatter.append("Print requires one project"); break;
    case BuildError::RunnerUnavailableForTarget: formatter.append("Runner cannot execute the selected target"); break;
    case BuildError::RunnerCommandMissing: formatter.append("Wrapped runner command is missing"); break;
    case BuildError::CoverageUnsupportedForToolchain:
        formatter.append("Coverage is unsupported by the selected toolchain");
        break;
    case BuildError::ConfigurationNotFound: formatter.append("Build configuration was not found"); break;
    case BuildError::ProjectDependencyCycle: formatter.append("Build project dependencies contain a cycle"); break;
    case BuildError::ProjectNotFound: formatter.append("Build project was not found"); break;
    case BuildError::SysrootTargetUnsupported: formatter.append("Sysroot does not support the selected target"); break;
    case BuildError::ToolchainTargetUnsupported:
        formatter.append("Toolchain does not support the selected target");
        break;
    case BuildError::ToolchainHostUnsupported: formatter.append("Toolchain does not support this host"); break;
    case BuildError::SysrootUnsupportedForToolchain:
        formatter.append("Toolchain does not support the selected sysroot");
        break;
    case BuildError::TargetTripleUnsupportedForToolchain:
        formatter.append("Toolchain does not support an explicit target triple");
        break;
    case BuildError::CustomCompilerCMissing: formatter.append("Custom C compiler is missing"); break;
    case BuildError::CustomCompilerCppMissing: formatter.append("Custom C++ compiler is missing"); break;
    case BuildError::UnresolvedToolchain: formatter.append("Build toolchain was not resolved"); break;
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
