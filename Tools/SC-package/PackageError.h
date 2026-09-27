// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../../Libraries/Common/Result.h"

namespace SC
{
namespace Tools
{
/// @brief Stable failures owned by the SC-package tool. Values are append-only.
enum class PackageError : uint32_t
{
    UnknownPackage = 1,
    InstallHandlerMissing,
    ReceiptNotFound,
    PackageNameRequired,
    UnsupportedAction,
    DuplicateRegistryEntry,
    RegistryCapacityExceeded,
    CopySourceDirectoryMissing,
    InstallDirectoryMissing,
    RecipePhaseUnknown,
    RecipePhaseHandlerMissing,
    ExportPathMissing,
    ExportPathAbsolute,
    ExportPathEscapesRoot,
    ExportPathTooDeep,
    SourceHashMalformed,
    SourceHashAlgorithmUnsupported,
    SourceHashDigestMissing,
    ReceiptEncodingFailed,
    ReceiptMalformed,
    ReceiptSchemaUnsupported,
    ReceiptNameMissing,
    DuplicateExport,
    ExportNotFound,
    RepairUnsupportedOnHost,
    RepairRootMissing,
    PackageLayoutIncomplete,
    RepairUnsupported,
    ReceiptVersionMissing,
    ReceiptSourceMissing,
    ReceiptInstallRootMissing,
    ReceiptValidationFailed,
    ExportKindMissing,
    ExportNameMissing,
    ExportFileMissing,
    RegistryExportMissing,
    ReceiptIdentityMismatch,
    LockEncodingFailed,
    ArchiveListingFailed,
    ArchiveEmpty,
    ArchiveRootMissing,
    ArchiveExtractionFailed,
    ExtractedRootMissing,
};

static constexpr ResultCategory PackageResultCategory = ResultCategory(22);
} // namespace Tools
} // namespace SC
