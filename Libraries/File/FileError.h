// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/Result.h"

namespace SC
{
//! @addtogroup group_file
//! @{

/// @brief Stable portable failures returned by the File library.
enum class FileError : uint32_t
{
    InvalidHandle = 1,
    UnsupportedPathEncoding,
    InvalidPath,
    PathMustBeAbsolute,
    PathCapacityExceeded,
    InvalidNamedPipeName,
    InvalidState,
    OperationUnsupported,
    OpenFailed,
    CloseFailed,
    ReadFailed,
    WriteFailed,
    IncompleteWrite,
    SeekFailed,
    MetadataQueryFailed,
    PermissionsChangeFailed,
    OwnershipChangeFailed,
    SyncFailed,
    TruncateFailed,
    DuplicateFailed,
    DescriptorConfigurationFailed,
    PipeCreationFailed,
    NamedPipeCreationFailed,
    NamedPipeConnectionFailed,
    NamedPipeAcceptFailed,
    EndpointCleanupFailed,
    BufferCapacityExceeded,
    WouldBlock,
    TimedOut,
    OperationCancelled,
    PipeDisconnected,
};

/// @brief Stable logical operation or native backend stage retained for a File failure.
enum class FileErrorDetail : uint16_t
{
    None = 0,
    ValidatePathEncoding,
    NormalizePath,
    ValidateAbsolutePath,
    BuildTransportPath,
    OpenFile,
    CloseDescriptor,
    ReadDescriptor,
    ReadDescriptorAtOffset,
    GrowReadBuffer,
    ResetReadBuffer,
    WriteDescriptor,
    WriteDescriptorAtOffset,
    SeekDescriptor,
    QueryDescriptorPosition,
    QueryDescriptorSize,
    QueryDescriptorMetadata,
    ChangeDescriptorPermissions,
    ChangeDescriptorOwnership,
    SynchronizeDescriptor,
    SynchronizeDescriptorData,
    TruncateDescriptor,
    GetStandardHandle,
    DuplicateStandardHandle,
    CreateAnonymousPipe,
    DuplicatePipeDescriptor,
    AssignPipeDescriptor,
    QueryDescriptorFlags,
    ConfigureDescriptorFlags,
    ValidateNamedPipeLogicalName,
    BuildNamedPipePath,
    ValidateNamedPipePath,
    CreateNamedPipeServer,
    RemoveNamedPipeEndpoint,
    BindNamedPipeServer,
    ListenNamedPipeServer,
    AcceptNamedPipeConnection,
    WaitNamedPipeClient,
    ConnectNamedPipeClient,
    DuplicateNamedPipeConnection,
};

/// @brief Kind of scalar diagnostic context stored in ResultFile.
enum class FileErrorContextKind : uint16_t
{
    None = 0,
    NativeError,
    RequiredBytes,
    ActualBytes,
};

/// @brief Typed scalar payload for a File error.
union FileErrorContext
{
    struct RequiredBytes
    {
    };
    struct ActualBytes
    {
    };

    uint32_t nativeError;
    uint32_t requiredBytes;
    uint32_t actualBytes;

    constexpr FileErrorContext(uint32_t value = 0) : nativeError(value) {}
    constexpr FileErrorContext(RequiredBytes, uint32_t value) : requiredBytes(value) {}
    constexpr FileErrorContext(ActualBytes, uint32_t value) : actualBytes(value) {}
};

/// @brief Stable category assigned to errors owned by File.
static constexpr ResultCategory FileResultCategory = ResultCategory(10);

/// @brief File result retaining an operation detail and one scalar diagnostic context.
/// @details The composed Result is authoritative. Plain and foreign conversions clear File-specific context, while
/// same-domain copies retain it. Conversion to Result preserves only the category/error identity.
struct [[nodiscard]] ResultFile
{
    Result               result;
    FileErrorDetail      detail      = FileErrorDetail::None;
    FileErrorContextKind contextKind = FileErrorContextKind::None;
    FileErrorContext     context     = {};

    constexpr ResultFile() : result(true) {}
    explicit constexpr ResultFile(bool valid) : result(valid) {}
    constexpr ResultFile(FileError error, FileErrorDetail detail = FileErrorDetail::None)
        : result(Result::Error(FileResultCategory, error)), detail(detail)
    {}
    constexpr ResultFile(FileError error, FileErrorDetail detail, FileErrorContextKind contextKind,
                         FileErrorContext context)
        : result(Result::Error(FileResultCategory, error)), detail(detail), contextKind(contextKind), context(context)
    {}
    constexpr ResultFile(Result result) : result(result) {}

    template <typename ResultLike>
    constexpr ResultFile(const ResultLike& other) : result(other.toResult())
    {}

    static constexpr ResultFile withNativeError(FileError error, FileErrorDetail detail, uint32_t nativeError)
    {
        return {error, detail, FileErrorContextKind::NativeError, FileErrorContext(nativeError)};
    }

    static constexpr ResultFile withRequiredBytes(FileError error, FileErrorDetail detail, uint32_t requiredBytes)
    {
        return {error, detail, FileErrorContextKind::RequiredBytes,
                FileErrorContext(FileErrorContext::RequiredBytes{}, requiredBytes)};
    }

    static constexpr ResultFile withActualBytes(FileError error, FileErrorDetail detail, uint32_t actualBytes)
    {
        return {error, detail, FileErrorContextKind::ActualBytes,
                FileErrorContext(FileErrorContext::ActualBytes{}, actualBytes)};
    }

    explicit constexpr operator bool() const { return static_cast<bool>(result); }
    constexpr          operator Result() const { return result; }
    constexpr Result   toResult() const { return result; }
    constexpr bool     isError(FileError error) const { return result.isError(FileResultCategory, error); }
};

//! @}
} // namespace SC
