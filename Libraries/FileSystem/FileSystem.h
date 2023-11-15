// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Result.h"
#include "../Strings/SmallString.h"
#include "../System/Time.h"

namespace SC
{
struct FileSystem;
struct StringConverter;
} // namespace SC

//! @defgroup group_file_system FileSystem
//! @copybrief library_file_system
//!
//! See @ref library_file_system library page for more details.<br>

//! @addtogroup group_file_system
//! @{

/// @brief Execute fs operations { exists | copy | delete } for { files | directories }.
/// It will scope all operations on relative paths to the `initialWorkingDirectory` passed in SC::FileSystem::init.
/// All methods can always return failure due to access or disk I/O errors, and they will be omitted in the return
/// clauses for each method. Only the specific returned result behaviour of the given method will be described.
struct SC::FileSystem
{
  private:
    StringNative<512> currentDirectory = StringEncoding::Native;

  public:
    bool preciseErrorMessages = false; ///< Formats errors in an internal buffer when returning failed Result

    /// @brief Call init function when instantiating the class to set current `working` directory
    /// @param initialWorkingDirectory The wanted directory
    /// @return Valid Result if `initialWorkingDirectory` exists and it's accessible
    Result init(StringView initialWorkingDirectory);

    /// @brief Changes current working directory after init
    /// @param newWorkingDirectory The wanted directory
    /// @return Valid Result if `initialWorkingDirectory` exists and it's accessible
    Result changeDirectory(StringView newWorkingDirectory);

    /// @brief Specify copy options like overwriting existing files
    struct CopyFlags
    {
        CopyFlags()
        {
            overwrite           = false;
            useCloneIfSupported = true;
        }

        /// @brief If `true` copy will overwrite existing files in the destination
        /// @param value `true` if to overwrite
        /// @return itself
        CopyFlags& setOverwrite(bool value)
        {
            overwrite = value;
            return *this;
        }

        /// @brief If `true` copy will use native filesystem clone os api
        /// @param value `true` if using clone is wanted
        /// @return itself
        CopyFlags& setUseCloneIfSupported(bool value)
        {
            useCloneIfSupported = value;
            return *this;
        }

      private:
        friend struct FileSystem;
        bool overwrite;
        bool useCloneIfSupported;
    };

    /// @brief Specifiy source, destination and flags for a copy operation
    struct CopyOperation
    {
        StringView source;      ///< Copy operation source (can be a {relative | absolute} {file | directory} path)
        StringView destination; ///< Copy operation sink (can be a {relative | absolute} {file | directory} path)
        CopyFlags  copyFlags;   ///< Copy operation flags (overwrite, use clone api etc.)
    };

    /// @brief Copies many files
    /// @param sourceDestination View over a sequence of CopyOperation describing copies to be done
    /// @return Valid Result if all copies succeeded
    [[nodiscard]] Result copyFile(Span<const CopyOperation> sourceDestination);

    /// @brief Copy a single file
    /// @param source Source file path
    /// @param destination Destination file path
    /// @param copyFlags Copy flags (overwrite, use clone api etc.)
    /// @return Valid Result if copy succeded
    [[nodiscard]] Result copyFile(StringView source, StringView destination, CopyFlags copyFlags = CopyFlags())
    {
        return copyFile(CopyOperation{source, destination, copyFlags});
    }

    /// @brief Copy many directories
    /// @param sourceDestination View over a sequence of CopyOperation describing copies to be done
    /// @return Valid Result if all copies succeeded
    [[nodiscard]] Result copyDirectory(Span<const CopyOperation> sourceDestination);

    /// @brief Copy a single directory
    /// @param source Source directory path
    /// @param destination Destination directory path
    /// @param copyFlags Copy flags (overwrite, use clone api etc.)
    /// @return Valid Result if copy succeded
    [[nodiscard]] Result copyDirectory(StringView source, StringView destination, CopyFlags copyFlags = CopyFlags())
    {
        return copyDirectory(CopyOperation{source, destination, copyFlags});
    }

    /// @brief Remove multiple files
    /// @param files View over a list of paths
    /// @return Valid Result if file was removed
    [[nodiscard]] Result removeFile(Span<const StringView> files);

    /// @brief Remove a single file
    /// @param source A single file path to be removed
    /// @return Valid Result if file was existing and it has been removed successfully
    /// @see removeFileIfExists
    [[nodiscard]] Result removeFile(StringView source) { return removeFile(Span<const StringView>{source}); }

    /// @brief Remove a single file, giving no error if it doesn't exist
    /// @param source The file to be removed if it exists
    /// @return Valid Result if the file doesn't exist or if it exists and it has been successfully removed.
    [[nodiscard]] Result removeFileIfExists(StringView source);

    /// @brief Remove multiple directies with their entire content (like posix `rm -rf`)
    /// @param directories List of directories to remove
    /// @return Valid Result if all directories and their contents have been successfully removed
    [[nodiscard]] Result removeDirectoryRecursive(Span<const StringView> directories);

    /// @brief Removes a an empty directory
    /// @param directories List of empty directories to remove
    /// @return Invalid Result if the directory doesn't exist or it's not empty
    [[nodiscard]] Result removeEmptyDirectory(Span<const StringView> directories);

    /// @brief Removes a an empty directory that only contains other empty directories (but no files)
    /// @param directories List of empty directories to remove
    /// @return Invalid Result if the directory doesn't exist or if it contains files somewhere inside of it
    [[nodiscard]] Result removeEmptyDirectoryRecursive(Span<const StringView> directories);

    /// @brief  Creates new directories that do not already exist
    /// @param directories List of paths where to create such directories
    /// @return Invalid Results if directories already exist
    [[nodiscard]] Result makeDirectory(Span<const StringView> directories);

    /// @brief Creates new directories
    /// @param directories List of paths where to create such directories
    /// @return Invalid Results in case of I/O or access error
    [[nodiscard]] Result makeDirectoryIfNotExists(Span<const StringView> directories);

    /// @brief Create new directories, creating also intermediate non existing directories (like posix `mkdir -p`)
    /// @param directories List of paths where to create such directories
    /// @return Invalid Result in case of I/O or access error
    [[nodiscard]] Result makeDirectoryRecursive(Span<const StringView> directories);

    /// @brief Check if a file or directory exists at a given path
    /// @param fileOrDirectory Path to check
    /// @return `true` if a file or directory exists at the given path
    [[nodiscard]] bool exists(StringView fileOrDirectory);

    /// @brief Check if a directory exists at given path
    /// @param directory Directory path to check
    /// @return `true` if a directory exists at the given path
    [[nodiscard]] bool existsAndIsDirectory(StringView directory);

    /// @brief Check if a file exists at given path
    /// @param file Fil path to check
    /// @return `true` if a file exists at the given path
    [[nodiscard]] bool existsAndIsFile(StringView file);

    /// @brief Writes a block of memory to a file
    /// @param file Path to the file that is meant to be written
    /// @param data Block of memory to write
    /// @return Valid Result if the memory was successfully written
    [[nodiscard]] Result write(StringView file, Span<const char> data);

    /// @brief Reads contents of a file into a SC::Vector buffer
    /// @param file Path to the file to read
    /// @param[out] data Destination buffer that will receive data
    /// @return Valid Result if file was fully read successfully
    [[nodiscard]] Result read(StringView file, Vector<char>& data);

    /// @brief Writes a StringView to a file
    /// @param file Path to the file that is meant to be written
    /// @param text Text to be written
    /// @return Valid Result if the memory was successfully written
    [[nodiscard]] Result write(StringView file, StringView text);

    /// @brief Read contents of a file into a string with given encoding
    /// @param[in] file Path to the file to read
    /// @param[out] data Destination string that will receive the file contents
    /// @param[in] encoding The wanted encoding of the resulting string
    /// @return Valid Result if the entire content of the file was read into the String
    [[nodiscard]] Result read(StringView file, String& data, StringEncoding encoding);

    /// @brief A structure to describe modified time
    struct FileTime
    {
        AbsoluteTime modifiedTime = 0; ///< Time when file was last modified
    };
    /// @brief Reads a FileTime structure for a given file
    /// @param file Path to the file of interest
    /// @param[out] fileTime Destination FileTime structure that will receive results
    /// @return Valid Result if file time for the given file was successfully read
    [[nodiscard]] Result getFileTime(StringView file, FileTime& fileTime);

    /// @brief Change last modified time of a given file
    /// @param file Path to the file of interest
    /// @param time The new last modified time, as specified in the AbsoluteTime struct
    /// @return Valid Result if file time for the given file was successfully set
    [[nodiscard]] Result setLastModifiedTime(StringView file, AbsoluteTime time);

  private:
    [[nodiscard]] bool convert(const StringView file, String& destination, StringView* encodedPath = nullptr);

    StringNative<128> fileFormatBuffer1  = StringEncoding::Native;
    StringNative<128> fileFormatBuffer2  = StringEncoding::Native;
    StringNative<128> errorMessageBuffer = StringEncoding::Native;

    Result formatError(int errorNumber, StringView item, bool isWindowsNativeError);
    struct Internal;
};
//! @}
