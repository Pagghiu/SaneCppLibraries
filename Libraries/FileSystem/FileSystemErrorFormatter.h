// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/ResultErrorFormatter.h"
#include "FileSystemError.h"

namespace SC
{
namespace detail
{
inline bool appendFileSystemError(ResultErrorFormatter& formatter, FileSystemError error)
{
    switch (error)
    {
    case FileSystemError::NotInitialized: formatter.append("Filesystem is not initialized"); break;
    case FileSystemError::UnsupportedPathEncoding: formatter.append("Filesystem path encoding is not supported"); break;
    case FileSystemError::InvalidPath: formatter.append("Filesystem path is invalid"); break;
    case FileSystemError::PathMustBeAbsolute: formatter.append("Filesystem path must be absolute"); break;
    case FileSystemError::PathCapacityExceeded: formatter.append("Filesystem path capacity exceeded"); break;
    case FileSystemError::InvalidState: formatter.append("Filesystem state is invalid"); break;
    case FileSystemError::EntryNotFound: formatter.append("Filesystem entry was not found"); break;
    case FileSystemError::EntryAlreadyExists: formatter.append("Filesystem entry already exists"); break;
    case FileSystemError::EntryTypeMismatch: formatter.append("Filesystem entry has an unexpected type"); break;
    case FileSystemError::AccessDenied: formatter.append("Filesystem access was denied"); break;
    case FileSystemError::ReadOnlyFileSystem: formatter.append("Filesystem is read-only"); break;
    case FileSystemError::StorageCapacityExceeded: formatter.append("Filesystem storage capacity was exceeded"); break;
    case FileSystemError::DirectoryNotEmpty: formatter.append("Filesystem directory is not empty"); break;
    case FileSystemError::TooManyLinks: formatter.append("Filesystem link limit was exceeded"); break;
    case FileSystemError::SymbolicLinkLoop: formatter.append("Filesystem symbolic link loop was detected"); break;
    case FileSystemError::CrossDeviceOperation:
        formatter.append("Filesystem operation crosses device boundaries");
        break;
    case FileSystemError::InvalidArgument: formatter.append("Filesystem operation argument is invalid"); break;
    case FileSystemError::OperationUnsupported: formatter.append("Filesystem operation is unsupported"); break;
    case FileSystemError::OutOfMemory: formatter.append("Filesystem operation ran out of memory"); break;
    case FileSystemError::BufferCapacityExceeded: formatter.append("Filesystem buffer capacity exceeded"); break;
    case FileSystemError::FileTooLarge: formatter.append("Filesystem file is too large"); break;
    case FileSystemError::IncompleteRead: formatter.append("Filesystem read was incomplete"); break;
    case FileSystemError::IncompleteWrite: formatter.append("Filesystem write was incomplete"); break;
    case FileSystemError::UnsupportedEntryType: formatter.append("Filesystem entry type is unsupported"); break;
    case FileSystemError::IoFailure: formatter.append("Filesystem input/output operation failed"); break;
    case FileSystemError::OperationFailed: formatter.append("Filesystem operation failed"); break;
    default: return false;
    }
    return true;
}

inline bool appendFileSystemErrorDetail(ResultErrorFormatter& formatter, FileSystemErrorDetail detail)
{
    switch (detail)
    {
    case FileSystemErrorDetail::None: return true;
    case FileSystemErrorDetail::ValidatePathEncoding: formatter.append("validate path encoding"); break;
    case FileSystemErrorDetail::NormalizePath: formatter.append("normalize path"); break;
    case FileSystemErrorDetail::ValidateAbsoluteBasePath: formatter.append("validate absolute base path"); break;
    case FileSystemErrorDetail::BuildTransportPath: formatter.append("build native transport path"); break;
    case FileSystemErrorDetail::ChangeDirectory: formatter.append("change filesystem directory"); break;
    case FileSystemErrorDetail::CheckAccess: formatter.append("check entry access"); break;
    case FileSystemErrorDetail::QueryEntryMetadata: formatter.append("query entry metadata"); break;
    case FileSystemErrorDetail::OpenFileForRead: formatter.append("open file for reading"); break;
    case FileSystemErrorDetail::OpenFileForWrite: formatter.append("open file for writing"); break;
    case FileSystemErrorDetail::QueryFileSize: formatter.append("query file size"); break;
    case FileSystemErrorDetail::GrowReadBuffer: formatter.append("grow read buffer"); break;
    case FileSystemErrorDetail::ReadFileContent: formatter.append("read file content"); break;
    case FileSystemErrorDetail::WriteFileContent: formatter.append("write file content"); break;
    case FileSystemErrorDetail::AppendFileContent: formatter.append("append file content"); break;
    case FileSystemErrorDetail::CreateDirectoryEntry: formatter.append("create directory"); break;
    case FileSystemErrorDetail::CreateParentDirectory: formatter.append("create parent directory"); break;
    case FileSystemErrorDetail::RemoveFileEntry: formatter.append("remove file"); break;
    case FileSystemErrorDetail::RemoveDirectoryEntry: formatter.append("remove directory"); break;
    case FileSystemErrorDetail::RenameEntry: formatter.append("rename entry"); break;
    case FileSystemErrorDetail::CreateSymbolicLinkEntry: formatter.append("create symbolic link"); break;
    case FileSystemErrorDetail::CreateHardLinkEntry: formatter.append("create hard link"); break;
    case FileSystemErrorDetail::ReadSymbolicLinkTarget: formatter.append("read symbolic link target"); break;
    case FileSystemErrorDetail::StoreSymbolicLinkTarget: formatter.append("store symbolic link target"); break;
    case FileSystemErrorDetail::ChangePermissions: formatter.append("change entry permissions"); break;
    case FileSystemErrorDetail::ChangeOwnership: formatter.append("change entry ownership"); break;
    case FileSystemErrorDetail::ChangeLinkPermissions: formatter.append("change link permissions"); break;
    case FileSystemErrorDetail::ChangeLinkOwnership: formatter.append("change link ownership"); break;
    case FileSystemErrorDetail::ChangeModifiedTime: formatter.append("change modified time"); break;
    case FileSystemErrorDetail::CopyFileEntry: formatter.append("copy file"); break;
    case FileSystemErrorDetail::CopyDirectoryTree: formatter.append("copy directory"); break;
    case FileSystemErrorDetail::CloneEntry: formatter.append("clone entry"); break;
    case FileSystemErrorDetail::OpenSourceFile: formatter.append("open source file"); break;
    case FileSystemErrorDetail::OpenDestinationFile: formatter.append("open destination file"); break;
    case FileSystemErrorDetail::OpenSourceDirectory: formatter.append("open source directory"); break;
    case FileSystemErrorDetail::EnumerateDirectory: formatter.append("enumerate directory"); break;
    case FileSystemErrorDetail::BuildChildPath: formatter.append("build child path"); break;
    case FileSystemErrorDetail::QueryChildMetadata: formatter.append("query child metadata"); break;
    case FileSystemErrorDetail::CopyChildEntry: formatter.append("copy child entry"); break;
    case FileSystemErrorDetail::RemoveChildEntry: formatter.append("remove child entry"); break;
    case FileSystemErrorDetail::WindowsQueryReparsePoint: formatter.append("query Windows reparse point"); break;
    case FileSystemErrorDetail::QueryFileTimes: formatter.append("query file times"); break;
    case FileSystemErrorDetail::SetFileTimes: formatter.append("set file times"); break;
    default: return false;
    }
    return true;
}

inline ResultErrorFormat formatFileSystemErrorWithContext(FileSystemError error, FileSystemErrorDetail detail,
                                                          FileSystemErrorContextKind contextKind,
                                                          FileSystemErrorContext context, Span<char> output)
{
    ResultErrorFormatter formatter(output);
    if (not appendFileSystemError(formatter, error))
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    if (detail != FileSystemErrorDetail::None)
    {
        formatter.append(" (detail: ");
        if (not appendFileSystemErrorDetail(formatter, detail))
            return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
        formatter.append(")");
    }
    switch (contextKind)
    {
    case FileSystemErrorContextKind::None: break;
    case FileSystemErrorContextKind::NativeError:
        formatter.append(" (native error: ");
        formatter.append(static_cast<uint64_t>(context.nativeError));
        formatter.append(")");
        break;
    case FileSystemErrorContextKind::RequiredBytes:
        formatter.append(" (required capacity: ");
        formatter.append(static_cast<uint64_t>(context.requiredBytes));
        formatter.append(" bytes)");
        break;
    case FileSystemErrorContextKind::ActualBytes:
        formatter.append(" (actual bytes: ");
        formatter.append(static_cast<uint64_t>(context.actualBytes));
        formatter.append(")");
        break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}
} // namespace detail

inline ResultErrorFormat formatFileSystemError(FileSystemError error, Span<char> output)
{
    return detail::formatFileSystemErrorWithContext(error, FileSystemErrorDetail::None,
                                                    FileSystemErrorContextKind::None, {}, output);
}

inline ResultErrorFormat formatFileSystemError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != FileSystemResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatFileSystemError(static_cast<FileSystemError>(result.errorValue()), output);
}

inline ResultErrorFormat formatFileSystemError(ResultFileSystem result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.result.category() != FileSystemResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return detail::formatFileSystemErrorWithContext(static_cast<FileSystemError>(result.result.errorValue()),
                                                    result.detail, result.contextKind, result.context, output);
}
} // namespace SC
