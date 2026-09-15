// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Common/ResultErrorFormatter.h"
#include "FileSystemIteratorError.h"

namespace SC
{
//! @addtogroup group_file_system_iterator
//! @{

namespace detail
{
inline ResultErrorFormat formatFileSystemIteratorErrorWithContext(FileSystemIteratorError error, uint32_t nativeError,
                                                                  uint32_t depth, Span<char> output)
{
    ResultErrorFormatter formatter(output);
    switch (error)
    {
    case FileSystemIteratorError::NotInitialized: formatter.append("File system iterator is not initialized"); break;
    case FileSystemIteratorError::InvalidRecursionState:
        formatter.append("Directory recursion is not valid in the current state");
        break;
    case FileSystemIteratorError::UnsupportedPathEncoding: formatter.append("Path encoding is not supported"); break;
    case FileSystemIteratorError::PathTooLong: formatter.append("Directory path is too long"); break;
    case FileSystemIteratorError::RecursionLimitExceeded:
        formatter.append("Directory recursion storage is exhausted");
        break;
    case FileSystemIteratorError::PathResolutionFailed: formatter.append("Failed to resolve directory path"); break;
    case FileSystemIteratorError::OpenDirectoryFailed: formatter.append("Failed to open directory"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }

    if (nativeError != 0)
    {
        formatter.append(" (native error: ");
        formatter.append(static_cast<uint64_t>(nativeError));
        formatter.append(", depth: ");
        formatter.append(static_cast<uint64_t>(depth));
        formatter.append(")");
    }
    else if (error == FileSystemIteratorError::RecursionLimitExceeded)
    {
        formatter.append(" (depth: ");
        formatter.append(static_cast<uint64_t>(depth));
        formatter.append(")");
    }
    return formatter.finish();
}
} // namespace detail

/// @brief Formats a canonical English FileSystemIterator diagnostic into caller-owned storage.
inline ResultErrorFormat formatFileSystemIteratorError(FileSystemIteratorError error, Span<char> output)
{
    return detail::formatFileSystemIteratorErrorWithContext(error, 0, 0, output);
}

/// @brief Formats a plain Result when it contains a FileSystemIterator error.
inline ResultErrorFormat formatFileSystemIteratorError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != FileSystemIteratorResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatFileSystemIteratorError(static_cast<FileSystemIteratorError>(result.errorValue()), output);
}

/// @brief Formats a FileSystemIterator result, including its fixed-size context when available.
inline ResultErrorFormat formatFileSystemIteratorError(ResultFileSystemIterator result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.result.category() != FileSystemIteratorResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return detail::formatFileSystemIteratorErrorWithContext(
        static_cast<FileSystemIteratorError>(result.result.errorValue()), result.nativeError, result.depth, output);
}

//! @}
} // namespace SC
