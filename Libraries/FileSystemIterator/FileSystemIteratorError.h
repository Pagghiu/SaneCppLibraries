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

/// @brief Stable category assigned to errors owned by the FileSystemIterator library.
static constexpr ResultCategory FileSystemIteratorResultCategory = ResultCategory(4);

/// @brief FileSystemIterator result retaining optional native error and directory-depth details.
/// @details nativeError is zero when unavailable. depth is the affected root-relative directory depth. Converting to
/// Result preserves the portable error identity and deliberately discards both context fields.
struct [[nodiscard]] ResultFileSystemIterator
{
    Result   result;
    uint32_t nativeError = 0;
    uint32_t depth       = 0;

    explicit constexpr ResultFileSystemIterator(bool valid = true) : result(valid) {}
    constexpr ResultFileSystemIterator(FileSystemIteratorError error, uint32_t nativeError = 0, uint32_t depth = 0)
        : result(Result::Error(FileSystemIteratorResultCategory, error)), nativeError(nativeError), depth(depth)
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
