// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Compiler.h"
#ifndef SC_EXPORT_LIBRARY_FILE_SYSTEM
#define SC_EXPORT_LIBRARY_FILE_SYSTEM 0
#endif
#define SC_FILE_SYSTEM_EXPORT SC_COMPILER_LIBRARY_EXPORT(SC_EXPORT_LIBRARY_FILE_SYSTEM)

#include "../Common/Result.h"
#include "../Foundation/Internal/IGrowableBuffer.h"
#include "../Foundation/PrimitiveTypes.h"
#include "../Foundation/StringPath.h"

namespace SC
{
//! @defgroup group_file_system FileSystem
//! @copybrief library_file_system (see  @ref library_file_system for more details)

//! @addtogroup group_file_system
//! @{

/// @brief A structure to describe file stats
enum class FileSystemEntryType : uint8_t
{
    Unknown,
    File,
    Directory,
    SymbolicLink,
    Other,
};

/// @brief A structure to describe file stats
struct FileSystemStat
{
    FileSystemEntryType entryType = FileSystemEntryType::Unknown; ///< Type of entry stored at path

    size_t fileSize      = 0; ///< Size of the file in bytes
    size_t hardLinkCount = 0; ///< Number of hard links to the entry
    TimeMs creationTime;      ///< Time when file was created when available
    TimeMs accessedTime;      ///< Time when file was last accessed when available
    TimeMs modifiedTime;      ///< Time when file was last modified

    struct
    {
        uint32_t mode          = 0; ///< POSIX st_mode
        uint32_t uid           = 0; ///< POSIX st_uid
        uint32_t gid           = 0; ///< POSIX st_gid
        uint64_t inode         = 0; ///< POSIX st_ino
        uint64_t device        = 0; ///< POSIX st_dev
        uint64_t specialDevice = 0; ///< POSIX st_rdev
        uint64_t blocks        = 0; ///< POSIX st_blocks
        uint64_t blockSize     = 0; ///< POSIX st_blksize
    } posix;

    struct
    {
        uint32_t attributes         = 0; ///< Windows file attributes
        uint32_t reparseTag         = 0; ///< Windows reparse tag
        uint32_t volumeSerialNumber = 0; ///< Windows volume serial number
        uint64_t fileIndex          = 0; ///< Windows file index
    } windows;
};

/// @brief Access mode for path checks
enum class FileSystemAccessMode : uint8_t
{
    Exists,  ///< Check if path exists
    Read,    ///< Check if path is readable
    Write,   ///< Check if path is writable
    Execute, ///< Check if path is executable / traversable
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

/// @brief Execute fs operations { exists, copy, delete } for { files and directories }.
/// It will scope all operations on relative paths to the `initialWorkingDirectory` passed in SC::FileSystem::init.
/// All methods can always return failure due to access or disk I/O errors, and they will be omitted in the return
/// clauses for each method. Only the specific returned result behaviour of the given method will be described.
///
/// \snippet Tests/Libraries/FileSystem/FileSystemTest.cpp FileSystemQuickSheetSnippet
struct SC_FILE_SYSTEM_EXPORT FileSystem
{
    bool preciseErrorMessages = false; ///< Formats errors in an internal buffer when returning failed Result

    /// @brief Access mode for path checks
    using AccessMode = FileSystemAccessMode;

    /// @brief Call init function when instantiating the class to set directory for all operations using relative paths.
    /// @param initialDirectory The wanted directory
    /// @return Valid Result if `initialDirectory` exists and it's accessible
    Result init(StringSpan initialDirectory);

    /// @brief Changes current directory. All operations with relative paths will be relative to this directory.
    /// @param newDirectory The wanted directory
    /// @return Valid Result if `initialWorkingDirectory` exists and it's accessible
    Result changeDirectory(StringSpan newDirectory);

    /// @brief Specify copy options like overwriting existing files
    using CopyFlags = FileSystemCopyFlags;

    /// @brief Specify source, destination and flags for a copy operation
    struct CopyOperation
    {
        StringSpan source;      ///< Copy operation source (can be a {relative | absolute} {file | directory} path)
        StringSpan destination; ///< Copy operation sink (can be a {relative | absolute} {file | directory} path)
        CopyFlags  copyFlags;   ///< Copy operation flags (overwrite, use clone api etc.)
    };

    /// @brief Copies many files
    /// @param sourceDestination View over a sequence of CopyOperation describing copies to be done
    /// @return Valid Result if all copies succeeded
    ///
    /// Example:
    /// @see copyFile (for an usage example)
    Result copyFiles(Span<const CopyOperation> sourceDestination);

    /// @brief Copy a single file
    /// @param source Source file path
    /// @param destination Destination file path
    /// @param copyFlags Copy flags (overwrite, use clone api etc.)
    /// @return Valid Result if copy succeeded
    ///
    /// Example:
    /// \snippet Tests/Libraries/FileSystem/FileSystemTest.cpp copyExistsFileSnippet
    Result copyFile(StringSpan source, StringSpan destination, CopyFlags copyFlags = CopyFlags())
    {
        return copyFiles(CopyOperation{source, destination, copyFlags});
    }

    /// @brief Copy many directories
    /// @param sourceDestination View over a sequence of CopyOperation describing copies to be done
    /// @return Valid Result if all copies succeeded
    /// @see copyDirectory (for an usage example)
    Result copyDirectories(Span<const CopyOperation> sourceDestination);

    /// @brief Copy a single directory
    /// @param source Source directory path
    /// @param destination Destination directory path
    /// @param copyFlags Copy flags (overwrite, use clone api etc.)
    /// @return Valid Result if copy succeeded
    ///
    /// Example:
    /// \snippet Tests/Libraries/FileSystem/FileSystemTest.cpp copyDirectoryRecursiveSnippet
    Result copyDirectory(StringSpan source, StringSpan destination, CopyFlags copyFlags = CopyFlags())
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
    Result rename(StringSpan path, StringSpan newPath);

    /// @brief Remove multiple files
    /// @param files View over a list of paths
    /// @return Valid Result if file was removed
    Result removeFiles(Span<const StringSpan> files);

    /// @brief Remove a single file
    /// @param source A single file path to be removed
    /// @return Valid Result if file was existing and it has been removed successfully
    /// @see write (for an usage example)
    Result removeFile(StringSpan source) { return removeFiles({source}); }

    /// @brief Remove a single file, giving no error if it doesn't exist
    /// @param source The file to be removed if it exists
    /// @return Valid Result if the file doesn't exist or if it exists and it has been successfully removed.
    Result removeFileIfExists(StringSpan source);

    /// @brief Remove a single link, giving no error if it doesn't exist
    /// @param source The link to be removed if it exists
    /// @return Valid Result if the file doesn't exist or if it exists and it has been successfully removed.
    Result removeLinkIfExists(StringSpan source);

    /// @brief Remove multiple directories with their entire content (like posix `rm -rf`)
    /// @param directories List of directories to remove
    /// @return Valid Result if all directories and their contents have been successfully removed
    /// @see removeDirectoryRecursive (for an usage example)
    Result removeDirectoriesRecursive(Span<const StringSpan> directories);

    /// @brief Remove single directory with its entire content (like posix `rm -rf`)
    /// @param directory Directory to remove
    /// @return Valid Result if directory contents has been successfully deleted
    ///
    /// Example:
    /// \snippet Tests/Libraries/FileSystem/FileSystemTest.cpp removeDirectoryRecursiveSnippet
    Result removeDirectoryRecursive(StringSpan directory) { return removeDirectoriesRecursive({directory}); }

    /// @brief Removes multiple empty directories
    /// @param directories List of empty directories to remove
    /// @return Invalid Result if one of the directories doesn't exist or it's not empty
    /// @see makeDirectoryRecursive (for an usage example)
    Result removeEmptyDirectories(Span<const StringSpan> directories);

    /// @brief Removes an empty directory
    /// @param directory Empty directory to remove
    /// @return Invalid Result if the directory doesn't exist or it's not empty
    /// @see makeDirectoryRecursive (for an usage example)
    Result removeEmptyDirectory(StringSpan directory) { return removeEmptyDirectories({directory}); }

    /// @brief  Creates new directories that do not already exist
    /// @param directories List of paths where to create such directories
    /// @return Invalid Results if directories already exist
    /// @see makeDirectoryRecursive (for an usage example)
    Result makeDirectories(Span<const StringSpan> directories);

    /// @brief  Creates a new directory that does not already exist
    /// @param directory Path where the directory should be created
    /// @return Invalid Results if directory already exist
    /// @see makeDirectoryRecursive (for an usage example)
    Result makeDirectory(StringSpan directory) { return makeDirectories({directory}); }

    /// @brief Creates new directories, if they don't already exist at the given path
    /// @param directories List of paths where to create such directories
    /// @return Invalid Results in case of I/O or access error
    Result makeDirectoriesIfNotExists(Span<const StringSpan> directories);

    /// @brief Creates a new directory, if it doesn't already exists at the given path
    /// @param directory Path where to create the new directory
    /// @return Invalid Results in case of I/O or access error
    Result makeDirectoryIfNotExists(StringSpan directory) { return makeDirectoriesIfNotExists({directory}); }

    /// @brief Create new directories, creating also intermediate non existing directories (like posix `mkdir -p`)
    /// @param directories List of paths where to create such directories
    /// @return Invalid Result in case of I/O or access error
    /// @see makeDirectoryRecursive (for an usage example)
    Result makeDirectoriesRecursive(Span<const StringSpan> directories);

    /// @brief Create a new directory, creating also intermediate non existing directories (like posix `mkdir -p`)
    /// @param directory Path where to create such directory
    /// @return Invalid Result in case of I/O or access error
    /// \snippet Tests/Libraries/FileSystem/FileSystemTest.cpp makeDirectoryRecursive
    Result makeDirectoryRecursive(StringSpan directory) { return makeDirectoriesRecursive({directory}); }

    /// @brief Creates a symbolic link at location linkFile pointing at sourceFileOrDirectory
    /// @param sourceFileOrDirectory The target of the link (can be a folder or directory)
    /// @param linkFile The location where the symbolic link will be created
    /// @return Invalid result if it's not possible creating the requested symbolic link
    Result createSymbolicLink(StringSpan sourceFileOrDirectory, StringSpan linkFile);

    /// @brief Creates a hard link at location linkFile pointing at sourceFile
    /// @param sourceFile The target file of the hard link
    /// @param linkFile The location where the hard link will be created
    /// @return Invalid result if it's not possible creating the requested hard link
    Result createHardLink(StringSpan sourceFile, StringSpan linkFile);

    /// @brief Check if a file or directory exists at a given path
    /// @param fileOrDirectory Path to check
    /// @return `true` if a file or directory exists at the given path
    /// @see existsAndIsFile (for an usage example)
    [[nodiscard]] bool exists(StringSpan fileOrDirectory);

    /// @brief Check if a directory exists at given path
    /// @param directory Directory path to check
    /// @return `true` if a directory exists at the given path
    [[nodiscard]] bool existsAndIsDirectory(StringSpan directory);

    /// @brief Check if a file exists at given path
    /// @param file File path to check
    /// @return `true` if a file exists at the given path
    ///
    /// Example:
    /// \snippet Tests/Libraries/FileSystem/FileSystemTest.cpp copyExistsFileSnippet
    [[nodiscard]] bool existsAndIsFile(StringSpan file);

    /// @brief Check if a link exists at given path
    /// @param file Link path to check
    /// @return `true` if a file exists at the given path
    [[nodiscard]] bool existsAndIsLink(StringSpan file);

    /// @brief Check whether the current process can access a path with the requested mode
    /// @param fileOrDirectory Path to check
    /// @param accessMode Requested access mode
    /// @return `true` if the requested access is allowed
    [[nodiscard]] bool canAccess(StringSpan fileOrDirectory, AccessMode accessMode = AccessMode::Exists);

    /// @brief Moves a directory from source to destination
    /// @param sourceDirectory The source directory that will be moved to destination
    /// @param destinationDirectory The destination directory
    /// @return `true` if the move succeeded
    [[nodiscard]] bool moveDirectory(StringSpan sourceDirectory, StringSpan destinationDirectory);

    /// @brief Writes a block of memory to a file
    /// @param file Path to the file that is meant to be written
    /// @param data Block of memory to write
    /// @return Valid Result if the memory was successfully written
    ///
    /// Example:
    /// \snippet Tests/Libraries/FileSystem/FileSystemTest.cpp writeReadRemoveFileSnippet
    Result write(StringSpan file, Span<const char> data);
    Result write(StringSpan file, Span<const uint8_t> data);

    /// @brief Replace the entire content of a file with the provided StringSpan
    /// @param file Path to the file that is meant to be written
    /// @param text Text to be written
    /// @return Valid Result if the memory was successfully written
    /// @see write (for an usage example)
    Result writeString(StringSpan file, StringSpan text);

    /// @brief Appends a StringSpan to a file
    /// @param file Path to the file that is meant to be appended
    /// @param text Text to be appended
    /// @return Valid Result if the memory was successfully appended
    Result writeStringAppend(StringSpan file, StringSpan text);

    /// @brief Read contents of a file into a String or Buffer
    /// @param[in] file Path to the file to read
    /// @param[out] data Destination String or Buffer that will receive file contents
    /// @return Valid Result if the entire file has been read successfully
    /// @see write (for an usage example)
    template <typename T>
    Result read(StringSpan file, T& data)
    {
        return read(file, GrowableBuffer<T>{data});
    }

    /// @brief Read contents of a file into an IGrowableBuffer
    /// @param[in] file Path to the file to read
    /// @param[out] buffer Destination IGrowableBuffer that will receive file contents
    /// @return Valid Result if the entire file has been read successfully
    Result read(StringSpan file, IGrowableBuffer&& buffer);

    /// @brief A structure to describe modified time
    using FileStat = FileSystemStat;

    /// @brief Obtains richer metadata about a path, following symbolic links
    /// @param file Path to the file of interest
    /// @param[out] fileStat Destination structure that will receive statistics about the file
    /// @return Valid Result if file stats for the given file was successfully read
    Result stat(StringSpan file, FileStat& fileStat);

    /// @brief Obtains richer metadata about a path without following symbolic links
    /// @param file Path to the file of interest
    /// @param[out] fileStat Destination structure that will receive statistics about the file
    /// @return Valid Result if file stats for the given file was successfully read
    Result lstat(StringSpan file, FileStat& fileStat);

    /// @brief Legacy convenience alias for stat(StringSpan, FileStat&)
    /// @param file Path to the file of interest
    /// @param[out] fileStat Destination structure that will receive statistics about the file
    /// @return Valid Result if file stats for the given file was successfully read
    Result getFileStat(StringSpan file, FileStat& fileStat);

    /// @brief Reads the target path stored by a symbolic link
    /// @param linkFile Path to the symbolic link
    /// @param[out] destination Destination receiving the native-encoded target path
    /// @return Valid Result if link target was successfully read
    Result readSymbolicLink(StringSpan linkFile, StringPath& destination);

    /// @brief Change file permission bits for a path, following symbolic links
    /// @param path Path to the file or directory of interest
    /// @param mode Platform-native numeric mode bits
    /// @return Valid Result if permissions were updated successfully
    Result chmod(StringSpan path, uint32_t mode);

    /// @brief Change owner and group for a path, following symbolic links
    /// @param path Path to the file or directory of interest
    /// @param uid Platform-native numeric owner identifier
    /// @param gid Platform-native numeric group identifier
    /// @return Valid Result if ownership was updated successfully
    Result chown(StringSpan path, uint32_t uid, uint32_t gid);

    /// @brief Change owner and group for a path without following symbolic links
    /// @param path Path to the file or directory of interest
    /// @param uid Platform-native numeric owner identifier
    /// @param gid Platform-native numeric group identifier
    /// @return Valid Result if ownership was updated successfully
    Result lchown(StringSpan path, uint32_t uid, uint32_t gid);

    /// @brief Change file permission bits for a symbolic link without following it
    /// @param path Path to the symbolic link of interest
    /// @param mode Platform-native numeric mode bits
    /// @return Valid Result if permissions were updated successfully
    Result lchmod(StringSpan path, uint32_t mode);

    /// @brief Change last modified time of a given file
    /// @param file Path to the file of interest
    /// @param time The new last modified time, as specified in the AbsoluteTime struct
    /// @return Valid Result if file time for the given file was successfully set
    Result setLastModifiedTime(StringSpan file, TimeMs time);

    /// @brief Low level filesystem API, requiring paths in native encoding (UTF-16 on Windows, UTF-8 elsewhere)
    /// @see SC::FileSystem is the higher level API that also handles paths in a different encoding is needed
    struct SC_FILE_SYSTEM_EXPORT Operations
    {
        static Result access(StringSpan path, AccessMode accessMode);
        static Result createSymbolicLink(StringSpan sourceFileOrDirectory, StringSpan linkFile);
        static Result createHardLink(StringSpan sourceFile, StringSpan linkFile);
        static Result makeDirectory(StringSpan dir);
        static Result exists(StringSpan path);
        static Result existsAndIsDirectory(StringSpan path);
        static Result existsAndIsFile(StringSpan path);
        static Result existsAndIsLink(StringSpan path);
        static Result makeDirectoryRecursive(StringSpan path);
        static Result removeEmptyDirectory(StringSpan path);
        static Result moveDirectory(StringSpan source, StringSpan destination);
        static Result removeFile(StringSpan path);
        static Result copyFile(StringSpan srcPath, StringSpan destPath, FileSystemCopyFlags flags);
        static Result rename(StringSpan path, StringSpan newPath);
        static Result copyDirectory(StringSpan srcPath, StringSpan destPath, FileSystemCopyFlags flags);
        static Result removeDirectoryRecursive(StringSpan directory);
        static Result stat(StringSpan path, FileSystemStat& fileStat);
        static Result lstat(StringSpan path, FileSystemStat& fileStat);
        static Result getFileStat(StringSpan path, FileSystemStat& fileStat);
        static Result readSymbolicLink(StringSpan path, StringPath& destination);
        static Result chmod(StringSpan path, uint32_t mode);
        static Result chown(StringSpan path, uint32_t uid, uint32_t gid);
        static Result lchown(StringSpan path, uint32_t uid, uint32_t gid);
        static Result lchmod(StringSpan path, uint32_t mode);
        static Result setLastModifiedTime(StringSpan path, TimeMs time);

        static StringSpan getExecutablePath(StringPath& executablePath);
        static StringSpan getCurrentWorkingDirectory(StringPath& currentWorkingDirectory);
        static StringSpan getApplicationRootDirectory(StringPath& applicationRootDirectory);

      private:
        struct Internal;
    };

  private:
    static constexpr size_t WindowsPathTransportCapacity = StringPath::MaxPath + 6;

    [[nodiscard]] Result convert(const StringSpan file, StringPath& destination,
                                 StringNativeBuffer<WindowsPathTransportCapacity + 1>& transportPath,
                                 StringSpan*                                           encodedPath = nullptr);

    StringPath fileFormatBuffer1;
    StringPath fileFormatBuffer2;
    StringPath currentDirectory;

    StringNativeBuffer<WindowsPathTransportCapacity + 1> fileTransportBuffer1;
    StringNativeBuffer<WindowsPathTransportCapacity + 1> fileTransportBuffer2;

    char errorMessageBuffer[256] = {0};

    Result formatError(int errorNumber, StringSpan item, bool isWindowsNativeError);
    struct Internal;
};
//! @}
} // namespace SC
