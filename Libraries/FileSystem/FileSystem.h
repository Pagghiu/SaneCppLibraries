// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Buffer.h"
#include "../Strings/String.h"
#include "FileSystemOperations.h"
namespace SC
{
//! @defgroup group_file_system FileSystem
//! @copybrief library_file_system (see  @ref library_file_system for more details)

//! @addtogroup group_file_system
//! @{

/// @brief Execute fs operations { exists, copy, delete } for { files and directories }.
/// It will scope all operations on relative paths to the `initialWorkingDirectory` passed in SC::FileSystem::init.
/// All methods can always return failure due to access or disk I/O errors, and they will be omitted in the return
/// clauses for each method. Only the specific returned result behaviour of the given method will be described.
///
/// \snippet Tests/Libraries/FileSystem/FileSystemTest.cpp FileSystemQuickSheetSnippet
struct SC_COMPILER_EXPORT FileSystem
{
  private:
    StringNative<512> currentDirectory = StringEncoding::Native;

  public:
    bool preciseErrorMessages = false; ///< Formats errors in an internal buffer when returning failed Result

    /// @brief Call init function when instantiating the class to set directory for all operations using relative paths.
    /// @param initialDirectory The wanted directory
    /// @return Valid Result if `initialDirectory` exists and it's accessible
    Result init(StringView initialDirectory);

    /// @brief Changes current directory. All operations with relative paths will be relative to this directory.
    /// @param newDirectory The wanted directory
    /// @return Valid Result if `initialWorkingDirectory` exists and it's accessible
    Result changeDirectory(StringView newDirectory);

    /// @brief Specify copy options like overwriting existing files
    using CopyFlags = FileSystemCopyFlags;

    /// @brief Specify source, destination and flags for a copy operation
    struct CopyOperation
    {
        StringView source;      ///< Copy operation source (can be a {relative | absolute} {file | directory} path)
        StringView destination; ///< Copy operation sink (can be a {relative | absolute} {file | directory} path)
        CopyFlags  copyFlags;   ///< Copy operation flags (overwrite, use clone api etc.)
    };

    /// @brief Copies many files
    /// @param sourceDestination View over a sequence of CopyOperation describing copies to be done
    /// @return Valid Result if all copies succeeded
    ///
    /// Example:
    /// @see copyFile (for an usage example)
    [[nodiscard]] Result copyFiles(Span<const CopyOperation> sourceDestination);

    /// @brief Copy a single file
    /// @param source Source file path
    /// @param destination Destination file path
    /// @param copyFlags Copy flags (overwrite, use clone api etc.)
    /// @return Valid Result if copy succeeded
    ///
    /// Example:
    /// \snippet Tests/Libraries/FileSystem/FileSystemTest.cpp copyExistsFileSnippet
    [[nodiscard]] Result copyFile(StringView source, StringView destination, CopyFlags copyFlags = CopyFlags())
    {
        return copyFiles(CopyOperation{source, destination, copyFlags});
    }

    /// @brief Copy many directories
    /// @param sourceDestination View over a sequence of CopyOperation describing copies to be done
    /// @return Valid Result if all copies succeeded
    /// @see copyDirectory (for an usage example)
    [[nodiscard]] Result copyDirectories(Span<const CopyOperation> sourceDestination);

    /// @brief Copy a single directory
    /// @param source Source directory path
    /// @param destination Destination directory path
    /// @param copyFlags Copy flags (overwrite, use clone api etc.)
    /// @return Valid Result if copy succeeded
    ///
    /// Example:
    /// \snippet Tests/Libraries/FileSystem/FileSystemTest.cpp copyDirectoryRecursiveSnippet
    [[nodiscard]] Result copyDirectory(StringView source, StringView destination, CopyFlags copyFlags = CopyFlags())
    {
        return copyDirectories(CopyOperation{source, destination, copyFlags});
    }

    /// @brief Rename a file or directory
    /// @param path The path to the file or directory to rename
    /// @param newPath The new path to the file or directory
    /// @return Valid Result if the file or directory was renamed
    ///
    /// Example:
    /// \snippet Tests/Libraries/FileSystem/FileSystemTest.cpp renameFileSnippet
    /// \snippet Tests/Libraries/FileSystem/FileSystemTest.cpp renameDirectorySnippet
    [[nodiscard]] Result rename(StringView path, StringView newPath);

    /// @brief Remove multiple files
    /// @param files View over a list of paths
    /// @return Valid Result if file was removed
    [[nodiscard]] Result removeFiles(Span<const StringView> files);

    /// @brief Remove a single file
    /// @param source A single file path to be removed
    /// @return Valid Result if file was existing and it has been removed successfully
    /// @see write (for an usage example)
    [[nodiscard]] Result removeFile(StringView source) { return removeFiles({source}); }

    /// @brief Remove a single file, giving no error if it doesn't exist
    /// @param source The file to be removed if it exists
    /// @return Valid Result if the file doesn't exist or if it exists and it has been successfully removed.
    [[nodiscard]] Result removeFileIfExists(StringView source);

    /// @brief Remove a single link, giving no error if it doesn't exist
    /// @param source The link to be removed if it exists
    /// @return Valid Result if the file doesn't exist or if it exists and it has been successfully removed.
    [[nodiscard]] Result removeLinkIfExists(StringView source);

    /// @brief Remove multiple directories with their entire content (like posix `rm -rf`)
    /// @param directories List of directories to remove
    /// @return Valid Result if all directories and their contents have been successfully removed
    /// @see removeDirectoryRecursive (for an usage example)
    [[nodiscard]] Result removeDirectoriesRecursive(Span<const StringView> directories);

    /// @brief Remove single directory with its entire content (like posix `rm -rf`)
    /// @param directory Directory to remove
    /// @return Valid Result if directory contents has been successfully deleted
    ///
    /// Example:
    /// \snippet Tests/Libraries/FileSystem/FileSystemTest.cpp removeDirectoryRecursiveSnippet
    [[nodiscard]] Result removeDirectoryRecursive(StringView directory)
    {
        return removeDirectoriesRecursive({directory});
    }

    /// @brief Removes multiple empty directories
    /// @param directories List of empty directories to remove
    /// @return Invalid Result if one of the directories doesn't exist or it's not empty
    /// @see makeDirectoryRecursive (for an usage example)
    [[nodiscard]] Result removeEmptyDirectories(Span<const StringView> directories);

    /// @brief Removes an empty directory
    /// @param directory Empty directory to remove
    /// @return Invalid Result if the directory doesn't exist or it's not empty
    /// @see makeDirectoryRecursive (for an usage example)
    [[nodiscard]] Result removeEmptyDirectory(StringView directory) { return removeEmptyDirectories({directory}); }

    /// @brief Removes multiple empty directories that only contains other empty directories (but no files)
    /// @param directories List of empty directories to remove
    /// @return Invalid Result if one of the directories doesn't exist or if it contains files somewhere inside of it
    /// @see makeDirectoryRecursive (for an usage example)
    [[nodiscard]] Result removeEmptyDirectoriesRecursive(Span<const StringView> directories);

    /// @brief Removes an empty directory that only contains other empty directories (but no files)
    /// @param directory List of empty directories to remove
    /// @return Invalid Result if the directory doesn't exist or if it contains files somewhere inside of it
    /// @see makeDirectoryRecursive (for an usage example)
    [[nodiscard]] Result removeEmptyDirectoryRecursive(StringView directory)
    {
        return removeEmptyDirectoriesRecursive({directory});
    }

    /// @brief  Creates new directories that do not already exist
    /// @param directories List of paths where to create such directories
    /// @return Invalid Results if directories already exist
    /// @see makeDirectoryRecursive (for an usage example)
    [[nodiscard]] Result makeDirectories(Span<const StringView> directories);

    /// @brief  Creates a new directory that does not already exist
    /// @param directory Path where the directory should be created
    /// @return Invalid Results if directory already exist
    /// @see makeDirectoryRecursive (for an usage example)
    [[nodiscard]] Result makeDirectory(StringView directory) { return makeDirectories({directory}); }

    /// @brief Creates new directories, if they don't already exist at the given path
    /// @param directories List of paths where to create such directories
    /// @return Invalid Results in case of I/O or access error
    [[nodiscard]] Result makeDirectoriesIfNotExists(Span<const StringView> directories);

    /// @brief Creates a new directory, if it doesn't already exists at the given path
    /// @param directory Path where to create the new directory
    /// @return Invalid Results in case of I/O or access error
    [[nodiscard]] Result makeDirectoryIfNotExists(StringView directory)
    {
        return makeDirectoriesIfNotExists({directory});
    }

    /// @brief Create new directories, creating also intermediate non existing directories (like posix `mkdir -p`)
    /// @param directories List of paths where to create such directories
    /// @return Invalid Result in case of I/O or access error
    /// @see makeDirectoryRecursive (for an usage example)
    [[nodiscard]] Result makeDirectoriesRecursive(Span<const StringView> directories);

    /// @brief Create a new directory, creating also intermediate non existing directories (like posix `mkdir -p`)
    /// @param directory Path where to create such directory
    /// @return Invalid Result in case of I/O or access error
    /// \snippet Tests/Libraries/FileSystem/FileSystemTest.cpp makeRemoveDirectoryRecursive
    [[nodiscard]] Result makeDirectoryRecursive(StringView directory) { return makeDirectoriesRecursive({directory}); }

    /// @brief Creates a symbolic link at location linkFile pointing at sourceFileOrDirectory
    /// @param sourceFileOrDirectory The target of the link (can be a folder or directory)
    /// @param linkFile The location where the symbolic link will be created
    /// @return Invalid result if it's not possible creating the requested symbolic link
    [[nodiscard]] Result createSymbolicLink(StringView sourceFileOrDirectory, StringView linkFile);

    /// @brief Check if a file or directory exists at a given path
    /// @param fileOrDirectory Path to check
    /// @return `true` if a file or directory exists at the given path
    /// @see existsAndIsFile (for an usage example)
    [[nodiscard]] bool exists(StringView fileOrDirectory);

    /// @brief Check if a directory exists at given path
    /// @param directory Directory path to check
    /// @return `true` if a directory exists at the given path
    [[nodiscard]] bool existsAndIsDirectory(StringView directory);

    /// @brief Check if a file exists at given path
    /// @param file File path to check
    /// @return `true` if a file exists at the given path
    ///
    /// Example:
    /// \snippet Tests/Libraries/FileSystem/FileSystemTest.cpp copyExistsFileSnippet
    [[nodiscard]] bool existsAndIsFile(StringView file);

    /// @brief Check if a link exists at given path
    /// @param file Link path to check
    /// @return `true` if a file exists at the given path
    [[nodiscard]] bool existsAndIsLink(StringView file);

    /// @brief Moves a directory from source to destination
    /// @param sourceDirectory The source directory that will be moved to destination
    /// @param destinationDirectory The destination directory
    /// @return `true` if the move succeeded
    [[nodiscard]] bool moveDirectory(StringView sourceDirectory, StringView destinationDirectory);

    /// @brief Writes a block of memory to a file
    /// @param file Path to the file that is meant to be written
    /// @param data Block of memory to write
    /// @return Valid Result if the memory was successfully written
    ///
    /// Example:
    /// \snippet Tests/Libraries/FileSystem/FileSystemTest.cpp writeReadRemoveFileSnippet
    [[nodiscard]] Result write(StringView file, Span<const char> data);
    [[nodiscard]] Result write(StringView file, Span<const uint8_t> data);

    /// @brief Reads contents of a file into a SC::Buffer
    /// @param file Path to the file to read
    /// @param[out] data Destination buffer that will receive data
    /// @return Valid Result if file was fully read successfully
    /// @see write (for an usage example)
    [[nodiscard]] Result read(StringView file, Buffer& data);

    /// @brief Replace the entire content of a file with the provided StringView
    /// @param file Path to the file that is meant to be written
    /// @param text Text to be written
    /// @return Valid Result if the memory was successfully written
    /// @see write (for an usage example)
    [[nodiscard]] Result writeString(StringView file, StringView text);

    /// @brief Appends a StringView to a file
    /// @param file Path to the file that is meant to be appended
    /// @param text Text to be appended
    /// @return Valid Result if the memory was successfully appended
    [[nodiscard]] Result writeStringAppend(StringView file, StringView text);

    /// @brief Read contents of a file into a string with given encoding
    /// @param[in] file Path to the file to read
    /// @param[out] data Destination string that will receive the file contents
    /// @param[in] encoding The wanted encoding of the resulting string
    /// @return Valid Result if the entire content of the file was read into the String
    /// @see write (for an usage example)
    [[nodiscard]] Result read(StringView file, String& data, StringEncoding encoding);

    /// @brief A structure to describe modified time
    using FileStat = FileSystemStat;

    /// @brief Obtains stats (size, modified time) about a file
    /// @param file Path to the file of interest
    /// @param[out] fileStat Destination structure that will receive statistics about the file
    /// @return Valid Result if file stats for the given file was successfully read
    [[nodiscard]] Result getFileStat(StringView file, FileStat& fileStat);

    /// @brief Change last modified time of a given file
    /// @param file Path to the file of interest
    /// @param time The new last modified time, as specified in the AbsoluteTime struct
    /// @return Valid Result if file time for the given file was successfully set
    [[nodiscard]] Result setLastModifiedTime(StringView file, Time::Realtime time);

  private:
    [[nodiscard]] bool convert(const StringView file, String& destination, StringView* encodedPath = nullptr);

    StringNative<128> fileFormatBuffer1  = StringEncoding::Native;
    StringNative<128> fileFormatBuffer2  = StringEncoding::Native;
    StringNative<128> errorMessageBuffer = StringEncoding::Native;

    Result formatError(int errorNumber, StringView item, bool isWindowsNativeError);
    struct Internal;

    // Fallbacks
    [[nodiscard]] static Result fallbackCopyDirectory(String& sourceDirectory, String& destinationDirectory,
                                                      FileSystem::CopyFlags options);
    [[nodiscard]] static Result fallbackRemoveDirectoryRecursive(String& sourceDirectory);
};
//! @}
} // namespace SC
