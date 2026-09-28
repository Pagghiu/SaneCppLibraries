// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../../Libraries/Common/ResultErrorFormatter.h"
#include "PackageError.h"

namespace SC
{
namespace Tools
{
/// @brief Formats an optional canonical English package-tool diagnostic into caller-owned storage.
inline ResultErrorFormat formatPackageError(PackageError error, Span<char> output)
{
    ResultErrorFormatter formatter(output);
    switch (error)
    {
    case PackageError::UnknownPackage: formatter.append("Package is not in the registry"); break;
    case PackageError::InstallHandlerMissing: formatter.append("Package install handler or recipe is missing"); break;
    case PackageError::ReceiptNotFound: formatter.append("Package receipt was not found"); break;
    case PackageError::PackageNameRequired: formatter.append("Package name is required"); break;
    case PackageError::UnsupportedAction: formatter.append("Package action is unsupported"); break;
    case PackageError::DuplicateRegistryEntry: formatter.append("Package registry entry already exists"); break;
    case PackageError::RegistryCapacityExceeded: formatter.append("Package registry capacity was exceeded"); break;
    case PackageError::CopySourceDirectoryMissing: formatter.append("Package copy source directory is missing"); break;
    case PackageError::InstallDirectoryMissing: formatter.append("Package install directory is missing"); break;
    case PackageError::RecipePhaseUnknown: formatter.append("Package recipe phase is unknown"); break;
    case PackageError::RecipePhaseHandlerMissing: formatter.append("Package recipe phase handler is missing"); break;
    case PackageError::ExportPathMissing: formatter.append("Package export path is missing"); break;
    case PackageError::ExportPathAbsolute: formatter.append("Package export path must be relative"); break;
    case PackageError::ExportPathEscapesRoot: formatter.append("Package export path escapes the package root"); break;
    case PackageError::ExportPathTooDeep: formatter.append("Package export path has too many components"); break;
    case PackageError::SourceHashMalformed: formatter.append("Package source hash is malformed"); break;
    case PackageError::SourceHashAlgorithmUnsupported:
        formatter.append("Package source hash algorithm is unsupported");
        break;
    case PackageError::SourceHashDigestMissing: formatter.append("Package source hash digest is missing"); break;
    case PackageError::ReceiptEncodingFailed: formatter.append("Package receipt could not be encoded"); break;
    case PackageError::ReceiptMalformed: formatter.append("Package receipt is malformed"); break;
    case PackageError::ReceiptSchemaUnsupported: formatter.append("Package receipt schema is unsupported"); break;
    case PackageError::ReceiptNameMissing: formatter.append("Package receipt name is missing"); break;
    case PackageError::DuplicateExport: formatter.append("Package receipt export is duplicated"); break;
    case PackageError::ExportNotFound: formatter.append("Package export was not found"); break;
    case PackageError::RepairUnsupportedOnHost: formatter.append("Package repair is unsupported on this host"); break;
    case PackageError::RepairRootMissing: formatter.append("Package repair root was not found"); break;
    case PackageError::PackageLayoutIncomplete: formatter.append("Package layout is incomplete"); break;
    case PackageError::RepairUnsupported: formatter.append("Package repair is unsupported"); break;
    case PackageError::ReceiptVersionMissing: formatter.append("Package receipt version is missing"); break;
    case PackageError::ReceiptSourceMissing: formatter.append("Package receipt source is missing"); break;
    case PackageError::ReceiptInstallRootMissing: formatter.append("Package receipt install root is missing"); break;
    case PackageError::ReceiptValidationFailed: formatter.append("Package receipt validation did not pass"); break;
    case PackageError::ExportKindMissing: formatter.append("Package receipt export kind is missing"); break;
    case PackageError::ExportNameMissing: formatter.append("Package receipt export name is missing"); break;
    case PackageError::ExportFileMissing: formatter.append("Package receipt export file is missing"); break;
    case PackageError::RegistryExportMissing: formatter.append("Package receipt is missing a registry export"); break;
    case PackageError::ReceiptIdentityMismatch:
        formatter.append("Package receipt identity does not match registry");
        break;
    case PackageError::LockEncodingFailed: formatter.append("Package lock could not be encoded"); break;
    case PackageError::ArchiveListingFailed: formatter.append("Package archive could not be listed"); break;
    case PackageError::ArchiveEmpty: formatter.append("Package archive is empty"); break;
    case PackageError::ArchiveRootMissing: formatter.append("Package archive root is missing"); break;
    case PackageError::ArchiveExtractionFailed: formatter.append("Package archive could not be extracted"); break;
    case PackageError::ExtractedRootMissing: formatter.append("Extracted package root was not found"); break;
    case PackageError::ImportDirectoryValueMissing: formatter.append("Import directory value is missing"); break;
    case PackageError::RunnerPathValueMissing: formatter.append("Runner path value is missing"); break;
    case PackageError::UnknownInstallOption: formatter.append("Package install option is unknown"); break;
    case PackageError::UnexpectedInstallArgument: formatter.append("Package install argument is unexpected"); break;
    case PackageError::PackageDirectoryMissing: formatter.append("Package directory was not found"); break;
    case PackageError::VersionDirectoryMissing: formatter.append("Package version directory was not found"); break;
    case PackageError::HostCommandUnavailable: formatter.append("Host command could not be resolved"); break;
    case PackageError::ArchiveContainerExtractionFailed:
        formatter.append("Package archive container could not be extracted");
        break;
    case PackageError::ArchivePayloadExtractionFailed:
        formatter.append("Package archive payload could not be extracted");
        break;
    case PackageError::ArchivePayloadUnsupported: formatter.append("Package archive payload is unsupported"); break;
    case PackageError::MetadataDownloadFailed: formatter.append("Package metadata could not be downloaded"); break;
    case PackageError::MetadataDecompressionFailed:
        formatter.append("Package metadata could not be decompressed");
        break;
    case PackageError::PackageIndexFieldMissing: formatter.append("Package index field is missing"); break;
    case PackageError::PackageMetadataNotFound: formatter.append("Package metadata was not found"); break;
    case PackageError::PackageMetadataHashMissing: formatter.append("Package metadata hash is missing"); break;
    case PackageError::RunnerUnavailable: formatter.append("Required package runner is unavailable"); break;
    case PackageError::EnvironmentVariableNameNotTerminated:
        formatter.append("Environment variable name is not null terminated");
        break;
    case PackageError::InstallerHostUnsupported:
        formatter.append("Package installer does not support this host");
        break;
    case PackageError::RunnerArchitectureUnsupported:
        formatter.append("Package runner does not support the requested architecture");
        break;
    case PackageError::RunnerExecutableMissing: formatter.append("Package runner executable is missing"); break;
    case PackageError::RunnerProbeFailed: formatter.append("Package runner probe failed"); break;
    case PackageError::RunnerIdentityMismatch: formatter.append("Package runner identity could not be verified"); break;
    case PackageError::RunnerSetMissing: formatter.append("Package runner set is missing"); break;
    case PackageError::Intel64RunnerMissing: formatter.append("Intel64 package runner is missing"); break;
    case PackageError::Arm64RunnerMissing: formatter.append("ARM64 package runner is missing"); break;
    case PackageError::ImportDirectoryMissing: formatter.append("Imported package directory does not exist"); break;
    case PackageError::ToolchainCompileFailed: formatter.append("Package toolchain smoke compile failed"); break;
    case PackageError::ToolchainObjectMissing: formatter.append("Package toolchain smoke object is missing"); break;
    case PackageError::ToolchainLinkFailed: formatter.append("Package toolchain smoke link failed"); break;
    case PackageError::ToolchainExecutableMissing:
        formatter.append("Package toolchain smoke executable is missing");
        break;
    case PackageError::InstallerArchitectureUnsupported:
        formatter.append("Package installer does not support this architecture");
        break;
    case PackageError::PackageArchiveUnavailable:
        formatter.append("Package archive is unavailable for this host");
        break;
    case PackageError::ToolVersionMalformed: formatter.append("Package tool version output is malformed"); break;
    case PackageError::ToolVersionMismatch: formatter.append("Package tool version does not match"); break;
    case PackageError::ToolIdentityMismatch: formatter.append("Package tool identity could not be verified"); break;
    case PackageError::HostToolUnavailable: formatter.append("Matching host tool is unavailable"); break;
    case PackageError::ToolProbeFailed: formatter.append("Package tool probe failed"); break;
    case PackageError::ArchiverProbeFailed: formatter.append("Package archiver probe failed"); break;
    case PackageError::ToolMetadataMissing: formatter.append("Package tool metadata is missing"); break;
    case PackageError::ToolTargetUnsupported: formatter.append("Package tool target is unsupported"); break;
    case PackageError::ToolTargetMismatch: formatter.append("Package tool targets do not match"); break;
    case PackageError::PackageSetupFailed: formatter.append("Package setup failed"); break;
    case PackageError::RequiredSourceMissing: formatter.append("Required package source file is missing"); break;
    case PackageError::PackageBuildFailed: formatter.append("Package build failed"); break;
    case PackageError::ToolchainRunFailed: formatter.append("Package toolchain smoke run failed"); break;
    case PackageError::PackageBuildArtifactMissing: formatter.append("Package build artifact is missing"); break;
    case PackageError::RunnerVersionMismatch: formatter.append("Package runner version does not match"); break;
    case PackageError::PackageCopyFailed: formatter.append("Package copy failed"); break;
    case PackageError::PackageDownloadFailed: formatter.append("Package download failed"); break;
    case PackageError::HostCommandFailed: formatter.append("Required host command failed"); break;
    case PackageError::SourceHashMismatch: formatter.append("Package source hash does not match"); break;
    case PackageError::DownloadVersionMissing: formatter.append("Package download version is missing"); break;
    case PackageError::DownloadPlatformMissing: formatter.append("Package download platform is missing"); break;
    case PackageError::DownloadURLMissing: formatter.append("Package download URL is missing"); break;
    case PackageError::DownloadHashMissing: formatter.append("Package download hash is missing"); break;
    case PackageError::SourceCloneFailed: formatter.append("Package source clone failed"); break;
    case PackageError::SourceRepositoryInitFailed: formatter.append("Package source repository setup failed"); break;
    case PackageError::SourceRemoteConfigurationFailed:
        formatter.append("Package source remote configuration failed");
        break;
    case PackageError::SourceFetchFailed: formatter.append("Package source fetch failed"); break;
    case PackageError::SourceRevisionCheckoutFailed: formatter.append("Package source revision checkout failed"); break;
    case PackageError::SourceRevisionProbeFailed: formatter.append("Package source revision probe failed"); break;
    case PackageError::SourceRevisionMismatch: formatter.append("Package source revision does not match"); break;
    case PackageError::ToolchainIncludesMissing: formatter.append("Toolchain include directory is missing"); break;
    case PackageError::SystemApiIncludesMissing: formatter.append("System API include directory is missing"); break;
    case PackageError::SystemSharedIncludesMissing:
        formatter.append("Shared system include directory is missing");
        break;
    case PackageError::SystemRuntimeIncludesMissing:
        formatter.append("System runtime include directory is missing");
        break;
    case PackageError::SystemProjectionIncludesMissing:
        formatter.append("System projection include directory is missing");
        break;
    case PackageError::SystemCppProjectionIncludesMissing:
        formatter.append("C++ system projection include directory is missing");
        break;
    case PackageError::Intel64ToolchainLibrariesMissing:
        formatter.append("Intel64 toolchain library directory is missing");
        break;
    case PackageError::Intel64SystemApiLibrariesMissing:
        formatter.append("Intel64 system API library directory is missing");
        break;
    case PackageError::Intel64SystemRuntimeLibrariesMissing:
        formatter.append("Intel64 system runtime library directory is missing");
        break;
    case PackageError::Intel64CompilerMissing: formatter.append("Intel64 compiler executable is missing"); break;
    case PackageError::Intel64LinkerMissing: formatter.append("Intel64 linker executable is missing"); break;
    case PackageError::Intel64ArchiverMissing: formatter.append("Intel64 archiver executable is missing"); break;
    case PackageError::Arm64ToolchainLibrariesMissing:
        formatter.append("ARM64 toolchain library directory is missing");
        break;
    case PackageError::Arm64SystemApiLibrariesMissing:
        formatter.append("ARM64 system API library directory is missing");
        break;
    case PackageError::Arm64SystemRuntimeLibrariesMissing:
        formatter.append("ARM64 system runtime library directory is missing");
        break;
    case PackageError::Arm64CompilerMissing: formatter.append("ARM64 compiler executable is missing"); break;
    case PackageError::Arm64LinkerMissing: formatter.append("ARM64 linker executable is missing"); break;
    case PackageError::Arm64ArchiverMissing: formatter.append("ARM64 archiver executable is missing"); break;
    case PackageError::RunnerCrashDialogConfigurationFailed:
        formatter.append("Runner crash-dialog configuration failed");
        break;
    case PackageError::RunnerFirstChanceConfigurationFailed:
        formatter.append("Runner first-chance exception configuration failed");
        break;
    case PackageError::RunnerMenuServiceConfigurationFailed:
        formatter.append("Runner menu-service configuration failed");
        break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

inline ResultErrorFormat formatPackageError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != PackageResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatPackageError(static_cast<PackageError>(result.errorValue()), output);
}
} // namespace Tools
} // namespace SC
