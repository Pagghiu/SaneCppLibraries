// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../File/FileDescriptor.h"
#include "../Foundation/OpaqueObject.h"
#include "../Foundation/Result.h"
#include "../Strings/StringView.h"

namespace SC
{
struct FileSystemIterator;
} // namespace SC

//! @defgroup group_file_system_iterator FileSystem Iterator
//! @copybrief library_file_system_iterator (see @ref library_file_system_iterator for more details)

//! @addtogroup group_file_system_iterator
//! @{

/// @brief Iterates files and directories inside a given path.
/// FileSystemIterator uses an iterator pattern to enumerate files instead of a callback.
/// This allows avoiding blocking on enumeration of very large directories and also the allocation of a huge number of
/// strings to hold all filenames.
/// When configuring an iteration, the caller can ask for a fully recursive enumeration or manually call
/// SC::FileSystemIterator::recurseSubdirectory when the current SC::FileSystemIterator::Entry item
/// (obtained with SC::FileSystemIterator::get) matches a directory of interest.
///
/// Example of recursive iteration of a directory:
/// \snippet Tests/Libraries/FileSystemIterator/FileSystemIteratorTest.cpp walkRecursiveSnippet
///
/// If only some directories should be recursed, manual recursion can help speeding up directory iteration:
/// \snippet Tests/Libraries/FileSystemIterator/FileSystemIteratorTest.cpp walkRecursiveManualSnippet
struct SC::FileSystemIterator
{
  private:
    struct Internal;

  public:
    /// Entry type (File or Directory)
    enum class Type
    {
        Directory,
        File
    };

    /// @brief Contains information on a file or directory
    struct Entry
    {
        StringView name;               ///< Name of current entry (file with extension or directory)
        StringView path;               ///< Absolute path of the current entry
        uint32_t   level = 0;          ///< Current level of nesting from start of iteration
        Type       type  = Type::File; ///< Tells if it's a file or a directory

        /// @brief Check if current entry is a directory
        bool isDirectory() const { return type == Type::Directory; }

      private:
        FileDescriptor parentFileDescriptor;
        friend struct Internal;
    };

    /// @brief Options when iterating (recursive and other options)
    struct Options
    {
        bool recursive      = false; ///< `true` will recurse automatically into subdirectories
        bool forwardSlashes = false; ///< `true` will return paths forward slash `/` even on Windows
    };

    Options options; ///< Options to control recursive behaviour and other options

    /// @brief Destroys the FileSystemIterator object
    ~FileSystemIterator();

    /// @brief Get current Entry being iterated
    /// @return Current entry
    const Entry& get() const { return currentEntry; }

    /// @brief Check if any error happened during iteration
    /// @return A valid Result if no errors have happened during file system iteration
    [[nodiscard]] Result checkErrors()
    {
        errorsChecked = true;
        return errorResult;
    }

    /// @brief Initializes the iterator on a given directory
    /// @param directory Directory to iterate
    /// @return Valid result if directory exists and is accessible
    [[nodiscard]] Result init(StringView directory);

    /// Returned string is only valid until next enumerateNext call and/or another init call

    /// @brief Moves iterator to next file
    /// @return Valid result if there are more files to iterate
    [[nodiscard]] Result enumerateNext();

    /// @brief Recurse into current item (assuming Entry::isDirectory == `true`)
    /// @return Valid result if current item is a directory and it can be accessed successfully
    [[nodiscard]] Result recurseSubdirectory();

  private:
    struct InternalDefinition
    {
        static constexpr int Windows = 4272;
        static constexpr int Apple   = 2104;
        static constexpr int Linux   = 2104;
        static constexpr int Default = Linux;

        static constexpr size_t Alignment = sizeof(uint64_t);

        using Object = Internal;
    };

  public:
    using InternalOpaque = OpaqueObject<InternalDefinition>;

  private:
    InternalOpaque internal;

    Entry  currentEntry;
    Result errorResult   = Result(true);
    bool   errorsChecked = false;
};

//! @}
