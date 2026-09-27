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
