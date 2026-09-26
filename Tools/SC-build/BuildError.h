// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../../Libraries/Common/Result.h"

namespace SC
{
/// @brief Stable failures owned by the SC-build tool. Values are append-only.
enum class BuildError : uint32_t
{
    HelpOutputFailed = 1,
    ParseErrorOutputFailed,
    InvalidOptionValue,
    AmbiguousOptionValue,
    InvalidOptionCombination,
    InvalidShortOptionGroup,
    UnsupportedPlatform,
    InvalidArguments,
    UnexpectedForwardedArguments,
    MissingLongPathPolicyValue,
    UnknownConfigureOption,
    DocumentationCommandFailed,
    UnsupportedAction,
    RuntimeShimLinkConflict,
    ProjectRootMissing,
    LibraryDirectoryMissing,
    ProjectNameMissing,
    ProjectTargetNameMissing,
    ProjectDirectoryMissing,
    ProjectConfigurationMissing,
    LongPathPolicyUnsupportedTarget,
    ConfigurationNameMissing,
    ConfigurationOutputPathMissing,
    ConfigurationIntermediatePathMissing,
    AbsoluteFileMaskUnsupported,
    WorkspaceNotFound,
    GeneratorArchitectureUnsupported,
    NoWorkspacesDefined,
    CoverageExecutableLaunchFailed,
    CoverageExecutableExitedUnsuccessfully,
    CoverageCompilerVersionUnavailable,
    CoverageProfileMergeLaunchFailed,
    CoverageProfileMergeExitedUnsuccessfully,
    CoverageHtmlGenerationLaunchFailed,
    CoverageHtmlGenerationExitedUnsuccessfully,
    CoverageReportLaunchFailed,
    CoverageReportExitedUnsuccessfully,
    CoveragePercentageInvalid,
    ExecutableExitedUnsuccessfully,
    ExecutablePathOutputMissing,
    ExecutablePathUnavailable,
    BuildCommandExitedUnsuccessfully,
    ExecutablePathQueryFailed,
    RunRequiresSingleProject,
    RunRequiresExecutableTarget,
    CommandArgumentLimitExceeded,
    ResolvedDependencyNotFound,
    StripUnsupported,
    ExportedSymbolsUnsupported,
    PrintRequiresSingleProject,
    RunnerUnavailableForTarget,
    RunnerCommandMissing,
    CoverageUnsupportedForToolchain,
    ConfigurationNotFound,
    ProjectDependencyCycle,
    ProjectNotFound,
};

static constexpr ResultCategory BuildResultCategory = ResultCategory(21);
} // namespace SC
