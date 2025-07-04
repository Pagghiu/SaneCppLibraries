// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Result.h"
#include "../Foundation/StringPath.h"
#include "../Time/Time.h"

namespace SC
{
/// @brief A structure to describe file stats
struct FileSystemStat
{
    size_t         fileSize     = 0; ///< Size of the file in bytes
    Time::Realtime modifiedTime = 0; ///< Time when file was last modified
};

/// @brief A structure to describe copy flags
struct FileSystemCopyFlags
{
    FileSystemCopyFlags()
    {
        overwrite           = false;
        useCloneIfSupported = true;
    }

    /// @brief If `true` copy will overwrite existing files in the destination
    /// @param value `true` if to overwrite
    /// @return itself
    FileSystemCopyFlags& setOverwrite(bool value)
    {
        overwrite = value;
        return *this;
    }

    /// @brief If `true` copy will use native filesystem clone os api
    /// @param value `true` if using clone is wanted
    /// @return itself
    FileSystemCopyFlags& setUseCloneIfSupported(bool value)
    {
        useCloneIfSupported = value;
        return *this;
    }

    bool overwrite;           ///< If `true` copy will overwrite existing files in the destination
    bool useCloneIfSupported; ///< If `true` copy will use native filesystem clone os api
};

/// @brief Low level filesystem operations, requiring paths in native encoding (UTF-16 on Windows, UTF-8 elsewhere)
/// @see SC::FileSystem when an higher level API that also handles paths in a different encoding is needed
struct SC_COMPILER_EXPORT FileSystemOperations
{
    /// @brief Create a symbolic link
    /// @param sourceFileOrDirectory The source file or directory to link to
    /// @param linkFile The link file to create
    /// @return Result::Error if the symbolic link could not be created
    static Result createSymbolicLink(StringViewData sourceFileOrDirectory, StringViewData linkFile);

    /// @brief Create a directory
    /// @param dir The directory to create
    /// @return Result::Error if the directory could not be created
    static Result makeDirectory(StringViewData dir);

    /// @brief Check if a path exists
    /// @param path The path to check
    /// @return Result::Error if the path could not be checked
    static Result exists(StringViewData path);

    /// @brief Check if a path exists and is a directory
    /// @param path The path to check
    /// @return Result::Error if the path could not be checked
    static Result existsAndIsDirectory(StringViewData path);

    /// @brief Check if a path exists and is a file
    /// @param path The path to check
    /// @return Result::Error if the path could not be checked
    static Result existsAndIsFile(StringViewData path);

    /// @brief Check if a path exists and is a link
    /// @param path The path to check
    /// @return Result::Error if the path could not be checked
    static Result existsAndIsLink(StringViewData path);

    /// @brief Create a directory and all parent directories as needed (like `mkdir -p`)
    /// @param path The directory to create
    /// @return Result::Error if the directory could not be created
    static Result makeDirectoryRecursive(StringViewData path);

    /// @brief Remove an empty directory
    /// @param path The path to the empty directory to remove
    /// @return Result::Error if the directory could not be removed
    static Result removeEmptyDirectory(StringViewData path);

    /// @brief Move a directory
    /// @param source The source directory to move
    /// @param destination The destination directory to move to
    static Result moveDirectory(StringViewData source, StringViewData destination);

    /// @brief Remove a file
    /// @param path The path to the file to remove
    /// @return Result::Error if the file could not be removed
    static Result removeFile(StringViewData path);

    /// @brief Copy a file
    /// @param srcPath The source file to copy
    /// @param destPath The destination file to copy to
    /// @param flags The copy flags
    /// @return Result::Error if the file could not be copied
    static Result copyFile(StringViewData srcPath, StringViewData destPath, FileSystemCopyFlags flags);

    /// @brief Rename a file or directory
    /// @param path The path to the file or directory to rename
    /// @param newPath The new path to the file or directory
    /// @return Result::Error if the file or directory could not be renamed
    static Result rename(StringViewData path, StringViewData newPath);

    /// @brief Copy a directory
    /// @param srcPath The source directory to copy
    /// @param destPath The destination directory to copy to
    /// @param flags The copy flags
    /// @return Result::Error if the directory could not be copied
    static Result copyDirectory(StringViewData srcPath, StringViewData destPath, FileSystemCopyFlags flags);

    /// @brief Remove a directory recursively
    /// @param directory The directory to remove
    /// @return Result::Error if the directory could not be removed
    static Result removeDirectoryRecursive(StringViewData directory);

    /// @brief Get the file stat
    /// @param path The path to the file to get the stat of
    /// @param fileStat The file stat to fill
    static Result getFileStat(StringViewData path, FileSystemStat& fileStat);

    /// @brief Set the last modified time of a file
    /// @param path The path to the file to set the last modified time of
    /// @param time The last modified time to set
    /// @return Result::Error if the last modified time could not be set
    static Result setLastModifiedTime(StringViewData path, Time::Realtime time);

    /// @brief Absolute executable path with extension (UTF16 on Windows, UTF8 elsewhere)
    static StringViewData getExecutablePath(StringPath& executablePath);

    /// @brief Absolute Application path with extension (UTF16 on Windows, UTF8 elsewhere)
    /// @note on macOS this is different from FileSystemDirectories::getExecutablePath
    static StringViewData getApplicationRootDirectory(StringPath& applicationRootDirectory);

  private:
    struct Internal;
};
} // namespace SC
