// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Common/Result.h"

namespace SC
{
//! @addtogroup group_file_system_iterator
//! @{

/// @brief Stable error codes returned by the FileSystemIterator library.
enum class FileSystemIteratorError : uint32_t
{
    NotInitialized = 1,
    InvalidRecursionState,
    UnsupportedPathEncoding,
    PathTooLong,
    RecursionLimitExceeded,
    PathResolutionFailed,
    OpenDirectoryFailed,
};

/// @brief Stable detail identifying the backend stage of a FileSystemIterator failure.
enum class FileSystemIteratorErrorDetail : uint16_t
{
    None = 0,
    WindowsResolveLogicalPath,
    BuildPath,
    PosixOpen,
    PosixFdOpenDir,
    WindowsFindFirstFile,
    PushRecursionState,
};

/// @brief Stable category assigned to errors owned by the FileSystemIterator library.
static constexpr ResultCategory FileSystemIteratorResultCategory = ResultCategory(4);

/// @brief FileSystemIterator result retaining optional native, backend-stage, and directory-depth details.
/// @details nativeError is zero when unavailable. depth is the affected root-relative directory depth and detail is
/// None when no lower-level stage is relevant. The path storage bounds successful traversal depth by
/// StringPath::MaxPath, which is at most 65535 on supported platforms, so depth and detail can safely use 16 bits
/// each. This keeps the type 16 bytes after the legacy Result message pointer is removed. Converting to Result
/// preserves the portable error identity and deliberately discards all context fields.
struct [[nodiscard]] ResultFileSystemIterator
{
    Result result;

    uint32_t                      nativeError = 0;
    uint16_t                      depth       = 0;
    FileSystemIteratorErrorDetail detail      = FileSystemIteratorErrorDetail::None;

    explicit constexpr ResultFileSystemIterator(bool valid = true) : result(valid) {}
    constexpr ResultFileSystemIterator(FileSystemIteratorError       error,
                                       FileSystemIteratorErrorDetail detail = FileSystemIteratorErrorDetail::None,
                                       uint32_t nativeError = 0, uint32_t depth = 0)
        : result(Result::Error(FileSystemIteratorResultCategory, error)), nativeError(nativeError),
          depth(static_cast<uint16_t>(depth)), detail(detail)
    {}
    constexpr ResultFileSystemIterator(Result result) : result(result) {}

    template <typename ResultLike>
    constexpr ResultFileSystemIterator(const ResultLike& other) : result(other.toResult())
    {}

    explicit constexpr operator bool() const { return static_cast<bool>(result); }

    constexpr operator Result() const { return result; }

    constexpr Result toResult() const { return result; }
    constexpr bool   isError(FileSystemIteratorError error) const
    {
        return result.isError(FileSystemIteratorResultCategory, error);
    }
};

//! @}
} // namespace SC
