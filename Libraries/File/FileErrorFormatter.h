// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/ResultErrorFormatter.h"
#include "FileError.h"

namespace SC
{
namespace detail
{
inline bool appendFileError(ResultErrorFormatter& formatter, FileError error)
{
    switch (error)
    {
    case FileError::InvalidHandle: formatter.append("File descriptor is invalid"); break;
    case FileError::UnsupportedPathEncoding: formatter.append("File path encoding is not supported"); break;
    case FileError::InvalidPath: formatter.append("File path is invalid"); break;
    case FileError::PathMustBeAbsolute: formatter.append("File path must be absolute"); break;
    case FileError::PathCapacityExceeded: formatter.append("File path capacity exceeded"); break;
    case FileError::InvalidNamedPipeName: formatter.append("Named pipe name is invalid"); break;
    case FileError::InvalidState: formatter.append("File object state is invalid"); break;
    case FileError::OperationUnsupported: formatter.append("File operation is unsupported"); break;
    case FileError::OpenFailed: formatter.append("Failed to open file descriptor"); break;
    case FileError::CloseFailed: formatter.append("Failed to close file descriptor"); break;
    case FileError::ReadFailed: formatter.append("Failed to read file descriptor"); break;
    case FileError::WriteFailed: formatter.append("Failed to write file descriptor"); break;
    case FileError::IncompleteWrite: formatter.append("File descriptor write was incomplete"); break;
    case FileError::SeekFailed: formatter.append("Failed to seek file descriptor"); break;
    case FileError::MetadataQueryFailed: formatter.append("Failed to query file descriptor metadata"); break;
    case FileError::PermissionsChangeFailed: formatter.append("Failed to change file descriptor permissions"); break;
    case FileError::OwnershipChangeFailed: formatter.append("Failed to change file descriptor ownership"); break;
    case FileError::SyncFailed: formatter.append("Failed to synchronize file descriptor"); break;
    case FileError::TruncateFailed: formatter.append("Failed to truncate file descriptor"); break;
    case FileError::DuplicateFailed: formatter.append("Failed to duplicate file descriptor"); break;
    case FileError::DescriptorConfigurationFailed: formatter.append("Failed to configure file descriptor"); break;
    case FileError::PipeCreationFailed: formatter.append("Failed to create pipe"); break;
    case FileError::NamedPipeCreationFailed: formatter.append("Failed to create named pipe server"); break;
    case FileError::NamedPipeConnectionFailed: formatter.append("Failed to connect named pipe"); break;
    case FileError::NamedPipeAcceptFailed: formatter.append("Failed to accept named pipe connection"); break;
    case FileError::EndpointCleanupFailed: formatter.append("Failed to remove named pipe endpoint"); break;
    case FileError::BufferCapacityExceeded: formatter.append("Buffer capacity exceeded"); break;
    case FileError::WouldBlock: formatter.append("File operation would block"); break;
    case FileError::TimedOut: formatter.append("File operation timed out"); break;
    case FileError::OperationCancelled: formatter.append("File operation was cancelled"); break;
    case FileError::PipeDisconnected: formatter.append("Pipe is disconnected"); break;
    default: return false;
    }
    return true;
}

inline bool appendFileErrorDetail(ResultErrorFormatter& formatter, FileErrorDetail detail)
{
    switch (detail)
    {
    case FileErrorDetail::None: return true;
    case FileErrorDetail::ValidatePathEncoding: formatter.append("validate path encoding"); break;
    case FileErrorDetail::NormalizePath: formatter.append("normalize path"); break;
    case FileErrorDetail::ValidateAbsolutePath: formatter.append("validate absolute path"); break;
    case FileErrorDetail::BuildTransportPath: formatter.append("build native transport path"); break;
    case FileErrorDetail::OpenFile: formatter.append("open file"); break;
    case FileErrorDetail::CloseDescriptor: formatter.append("close descriptor"); break;
    case FileErrorDetail::ReadDescriptor: formatter.append("read descriptor"); break;
    case FileErrorDetail::ReadDescriptorAtOffset: formatter.append("read descriptor at offset"); break;
    case FileErrorDetail::GrowReadBuffer: formatter.append("grow read buffer"); break;
    case FileErrorDetail::ResetReadBuffer: formatter.append("reset read buffer"); break;
    case FileErrorDetail::WriteDescriptor: formatter.append("write descriptor"); break;
    case FileErrorDetail::WriteDescriptorAtOffset: formatter.append("write descriptor at offset"); break;
    case FileErrorDetail::SeekDescriptor: formatter.append("seek descriptor"); break;
    case FileErrorDetail::QueryDescriptorPosition: formatter.append("query descriptor position"); break;
    case FileErrorDetail::QueryDescriptorSize: formatter.append("query descriptor size"); break;
    case FileErrorDetail::QueryDescriptorMetadata: formatter.append("query descriptor metadata"); break;
    case FileErrorDetail::ChangeDescriptorPermissions: formatter.append("change descriptor permissions"); break;
    case FileErrorDetail::ChangeDescriptorOwnership: formatter.append("change descriptor ownership"); break;
    case FileErrorDetail::SynchronizeDescriptor: formatter.append("synchronize descriptor"); break;
    case FileErrorDetail::SynchronizeDescriptorData: formatter.append("synchronize descriptor data"); break;
    case FileErrorDetail::TruncateDescriptor: formatter.append("truncate descriptor"); break;
    case FileErrorDetail::GetStandardHandle: formatter.append("get standard handle"); break;
    case FileErrorDetail::DuplicateStandardHandle: formatter.append("duplicate standard handle"); break;
    case FileErrorDetail::CreateAnonymousPipe: formatter.append("create anonymous pipe"); break;
    case FileErrorDetail::DuplicatePipeDescriptor: formatter.append("duplicate pipe descriptor"); break;
    case FileErrorDetail::AssignPipeDescriptor: formatter.append("assign pipe descriptor"); break;
    case FileErrorDetail::QueryDescriptorFlags: formatter.append("query descriptor flags"); break;
    case FileErrorDetail::ConfigureDescriptorFlags: formatter.append("configure descriptor flags"); break;
    case FileErrorDetail::ValidateNamedPipeLogicalName: formatter.append("validate named pipe logical name"); break;
    case FileErrorDetail::BuildNamedPipePath: formatter.append("build named pipe path"); break;
    case FileErrorDetail::ValidateNamedPipePath: formatter.append("validate named pipe path"); break;
    case FileErrorDetail::CreateNamedPipeServer: formatter.append("create named pipe server"); break;
    case FileErrorDetail::RemoveNamedPipeEndpoint: formatter.append("remove named pipe endpoint"); break;
    case FileErrorDetail::BindNamedPipeServer: formatter.append("bind named pipe server"); break;
    case FileErrorDetail::ListenNamedPipeServer: formatter.append("listen on named pipe server"); break;
    case FileErrorDetail::AcceptNamedPipeConnection: formatter.append("accept named pipe connection"); break;
    case FileErrorDetail::WaitNamedPipeClient: formatter.append("wait for named pipe server"); break;
    case FileErrorDetail::ConnectNamedPipeClient: formatter.append("connect named pipe client"); break;
    case FileErrorDetail::DuplicateNamedPipeConnection: formatter.append("duplicate named pipe connection"); break;
    default: return false;
    }
    return true;
}

inline ResultErrorFormat formatFileErrorWithContext(FileError error, FileErrorDetail detail,
                                                    FileErrorContextKind contextKind, FileErrorContext context,
                                                    Span<char> output)
{
    ResultErrorFormatter formatter(output);
    if (not appendFileError(formatter, error))
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    if (detail != FileErrorDetail::None)
    {
        formatter.append(" (detail: ");
        if (not appendFileErrorDetail(formatter, detail))
            return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
        formatter.append(")");
    }
    switch (contextKind)
    {
    case FileErrorContextKind::None: break;
    case FileErrorContextKind::NativeError:
        formatter.append(" (native error: ");
        formatter.append(static_cast<uint64_t>(context.nativeError));
        formatter.append(")");
        break;
    case FileErrorContextKind::RequiredBytes:
        formatter.append(" (required capacity: ");
        formatter.append(static_cast<uint64_t>(context.requiredBytes));
        formatter.append(" bytes)");
        break;
    case FileErrorContextKind::ActualBytes:
        formatter.append(" (actual bytes: ");
        formatter.append(static_cast<uint64_t>(context.actualBytes));
        formatter.append(")");
        break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}
} // namespace detail

inline ResultErrorFormat formatFileError(FileError error, Span<char> output)
{
    return detail::formatFileErrorWithContext(error, FileErrorDetail::None, FileErrorContextKind::None, {}, output);
}

inline ResultErrorFormat formatFileError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != FileResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatFileError(static_cast<FileError>(result.errorValue()), output);
}

inline ResultErrorFormat formatFileError(ResultFile result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.result.category() != FileResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return detail::formatFileErrorWithContext(static_cast<FileError>(result.result.errorValue()), result.detail,
                                              result.contextKind, result.context, output);
}
} // namespace SC
