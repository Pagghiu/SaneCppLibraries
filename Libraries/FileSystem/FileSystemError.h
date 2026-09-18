// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/Result.h"

namespace SC
{
//! @addtogroup group_file_system
//! @{

/// @brief Stable portable failures returned by the FileSystem library.
enum class FileSystemError : uint32_t
{
    NotInitialized = 1,
    UnsupportedPathEncoding,
    InvalidPath,
    PathMustBeAbsolute,
    PathCapacityExceeded,
    InvalidState,
    EntryNotFound,
    EntryAlreadyExists,
    EntryTypeMismatch,
    AccessDenied,
    ReadOnlyFileSystem,
    StorageCapacityExceeded,
    DirectoryNotEmpty,
    TooManyLinks,
    SymbolicLinkLoop,
    CrossDeviceOperation,
    InvalidArgument,
    OperationUnsupported,
    OutOfMemory,
    BufferCapacityExceeded,
    FileTooLarge,
    IncompleteRead,
    IncompleteWrite,
    UnsupportedEntryType,
    IoFailure,
    OperationFailed,
};

/// @brief Stable logical operation or backend stage retained for a FileSystem failure.
enum class FileSystemErrorDetail : uint16_t
{
    None = 0,
    ValidatePathEncoding,
    NormalizePath,
    ValidateAbsoluteBasePath,
    BuildTransportPath,
    ChangeDirectory,
    CheckAccess,
    QueryEntryMetadata,
    OpenFileForRead,
    OpenFileForWrite,
    QueryFileSize,
    GrowReadBuffer,
    ReadFileContent,
    WriteFileContent,
    AppendFileContent,
    CreateDirectoryEntry,
    CreateParentDirectory,
    RemoveFileEntry,
    RemoveDirectoryEntry,
    RenameEntry,
    CreateSymbolicLinkEntry,
    CreateHardLinkEntry,
    ReadSymbolicLinkTarget,
    StoreSymbolicLinkTarget,
    ChangePermissions,
    ChangeOwnership,
    ChangeLinkPermissions,
    ChangeLinkOwnership,
    ChangeModifiedTime,
    CopyFileEntry,
    CopyDirectoryTree,
    CloneEntry,
    OpenSourceFile,
    OpenDestinationFile,
    OpenSourceDirectory,
    EnumerateDirectory,
    BuildChildPath,
    QueryChildMetadata,
    CopyChildEntry,
    RemoveChildEntry,
    WindowsQueryReparsePoint,
    QueryFileTimes,
    SetFileTimes,
};

/// @brief Kind of scalar diagnostic context stored in ResultFileSystem.
enum class FileSystemErrorContextKind : uint16_t
{
    None = 0,
    NativeError,
    RequiredBytes,
    ActualBytes,
};

/// @brief Typed scalar payload for a FileSystem error.
union FileSystemErrorContext
{
    uint32_t nativeError;
    uint32_t requiredBytes;
    uint32_t actualBytes;

    constexpr FileSystemErrorContext(uint32_t value = 0) : nativeError(value) {}
};

/// @brief Stable category assigned to errors owned by FileSystem.
static constexpr ResultCategory FileSystemResultCategory = ResultCategory(11);

/// @brief FileSystem result retaining an operation detail and one scalar diagnostic context.
/// @details The composed Result is authoritative. Plain and foreign conversions clear FileSystem-specific context,
/// while same-domain copies retain it. Conversion to Result preserves only the category/error identity.
struct [[nodiscard]] ResultFileSystem
{
    Result                     result;
    FileSystemErrorDetail      detail      = FileSystemErrorDetail::None;
    FileSystemErrorContextKind contextKind = FileSystemErrorContextKind::None;
    FileSystemErrorContext     context     = {};

    constexpr ResultFileSystem() : result(true) {}
    explicit constexpr ResultFileSystem(bool valid) : result(valid) {}
    constexpr ResultFileSystem(FileSystemError error, FileSystemErrorDetail detail = FileSystemErrorDetail::None)
        : result(Result::Error(FileSystemResultCategory, error)), detail(detail)
    {}
    constexpr ResultFileSystem(FileSystemError error, FileSystemErrorDetail detail,
                               FileSystemErrorContextKind contextKind, FileSystemErrorContext context)
        : result(Result::Error(FileSystemResultCategory, error)), detail(detail), contextKind(contextKind),
          context(context)
    {}
    constexpr ResultFileSystem(Result result) : result(result) {}

    template <typename ResultLike>
    constexpr ResultFileSystem(const ResultLike& other) : result(other.toResult())
    {}

    static constexpr ResultFileSystem withNativeError(FileSystemError error, FileSystemErrorDetail detail,
                                                      uint32_t nativeError)
    {
        return {error, detail, FileSystemErrorContextKind::NativeError, FileSystemErrorContext(nativeError)};
    }

    static constexpr ResultFileSystem withRequiredBytes(FileSystemError error, FileSystemErrorDetail detail,
                                                        uint32_t requiredBytes)
    {
        return {error, detail, FileSystemErrorContextKind::RequiredBytes, FileSystemErrorContext(requiredBytes)};
    }

    static constexpr ResultFileSystem withActualBytes(FileSystemError error, FileSystemErrorDetail detail,
                                                      uint32_t actualBytes)
    {
        return {error, detail, FileSystemErrorContextKind::ActualBytes, FileSystemErrorContext(actualBytes)};
    }

    explicit constexpr operator bool() const { return static_cast<bool>(result); }
    constexpr          operator Result() const { return result; }
    constexpr Result   toResult() const { return result; }
    constexpr bool     isError(FileSystemError error) const { return result.isError(FileSystemResultCategory, error); }
};

//! @}
} // namespace SC
