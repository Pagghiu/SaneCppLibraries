// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
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
//! @copybrief library_file_system_iterator
//!
//! See @ref library_file_system_iterator library page for more details.<br>

//! @addtogroup group_file_system_iterator
//! @{

/// @brief Iterates files and directories inside a given path
struct SC::FileSystemIterator
{
    /// Entry type (File or Directory)
    enum class Type
    {
        Directory,
        File
    };

    /// @brief Contains information on a file or directory
    struct Entry
    {
        StringView name;
        StringView path;
        uint32_t   level = 0;
        Type       type  = Type::File;

        FileDescriptor parentFileDescriptor;

        bool isDirectory() const { return type == Type::Directory; }
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
    struct Internal;
    struct InternalDefinition
    {
        static constexpr int Windows = 4272;
        static constexpr int Apple   = 2104;
        static constexpr int Default = sizeof(void*);

        static constexpr size_t Alignment = alignof(void*);

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
