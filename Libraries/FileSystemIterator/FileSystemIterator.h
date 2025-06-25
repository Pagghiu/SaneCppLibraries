// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Result.h"
#include "../Foundation/StringViewData.h"

namespace SC
{

//! @defgroup group_file_system_iterator FileSystem Iterator
//! @copybrief library_file_system_iterator (see @ref library_file_system_iterator for more details)

//! @addtogroup group_file_system_iterator
//! @{

/// @brief Iterates files and directories inside a given path without allocating any memory.
/// FileSystemIterator uses an iterator pattern to enumerate files instead of a callback.
/// This allows avoiding blocking on enumeration of very large directories and also the allocation of a huge number of
/// strings to hold all filenames.
/// When configuring an iteration, the caller can ask for a fully recursive enumeration or manually call
/// SC::FileSystemIterator::recurseSubdirectory when the current SC::FileSystemIterator::Entry item
/// (obtained with SC::FileSystemIterator::get) matches a directory of interest.
/// The maximum number of nested recursion levels that will be allowed depends on the size of the
/// FileSystemIterator::FolderState span (can be a static array) passed in during init by the caller.
///
/// @note This class doesn't allocate any dynamic memory.
///
/// Example of recursive iteration of a directory:
/// \snippet Tests/Libraries/FileSystemIterator/FileSystemIteratorTest.cpp walkRecursiveSnippet
///
/// If only some directories should be recursed, manual recursion can help speeding up directory iteration:
/// \snippet Tests/Libraries/FileSystemIterator/FileSystemIteratorTest.cpp walkRecursiveManualSnippet
struct FileSystemIterator
{
    /// Entry type (File or Directory)
    enum class Type
    {
        Directory,
        File
    };

    /// @brief Holds state of a folder when recursing into it to list its files
    struct FolderState
    {
#if SC_PLATFORM_WINDOWS
        void* fileDescriptor = (void*)(long long)-1;
#else
        int   fileDescriptor = -1;
        void* dirEnumerator  = nullptr;
#endif
        size_t textLengthInBytes = 0;
        bool   gotDot1           = false;
        bool   gotDot2           = false;
    };

    /// @brief Contains information on a file or directory
    struct Entry
    {
        StringViewData name;               ///< Name of current entry (file with extension or directory)
        StringViewData path;               ///< Absolute path of the current entry
        uint32_t       level = 0;          ///< Current level of nesting from start of iteration
        Type           type  = Type::File; ///< Tells if it's a file or a directory

        /// @brief Check if current entry is a directory
        bool isDirectory() const { return type == Type::Directory; }

#if SC_PLATFORM_WINDOWS
        void* parentFileDescriptor = nullptr;
#else
        int parentFileDescriptor = 0;
#endif
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
    Result checkErrors()
    {
        errorsChecked = true;
        return errorResult;
    }

    /// @brief Initializes the iterator on a given directory
    /// @param directory Directory to iterate
    /// @param recursiveEntries User supplied buffer for the stack used during folder recursion (must be >= 1 elements)
    /// @return Valid result if directory exists and is accessible
    Result init(StringViewData directory, Span<FolderState> recursiveEntries);

    /// Returned string is only valid until next enumerateNext call and/or another init call

    /// @brief Moves iterator to next file
    /// @return Valid result if there are more files to iterate
    Result enumerateNext();

    /// @brief Recurse into current item (assuming Entry::isDirectory == `true`)
    /// @return Valid result if current item is a directory and it can be accessed successfully
    Result recurseSubdirectory();

  private:
    static constexpr auto MaxPath = StringViewData::MaxPath;
    struct Internal;
    struct RecurseStack
    {
        Span<FolderState> recursiveEntries;

        int currentEntry = -1;

        FolderState& back();

        void   pop_back();
        Result push_back(const FolderState& other);
        size_t size() const { return size_t(currentEntry + 1); }
        bool   isEmpty() const { return currentEntry == -1; }
    };
    RecurseStack recurseStack;

    Entry  currentEntry;
    Result errorResult   = Result(true);
    bool   errorsChecked = false;

#if SC_PLATFORM_WINDOWS
    bool     expectDotDirectories = true;
    wchar_t  currentPathString[MaxPath];
    uint64_t dirEnumeratorBuffer[592 / sizeof(uint64_t)];
#else
    char currentPathString[MaxPath];
#endif
    Result enumerateNextInternal(Entry& entry);
    Result recurseSubdirectoryInternal(Entry& entry);
};

//! @}
} // namespace SC
