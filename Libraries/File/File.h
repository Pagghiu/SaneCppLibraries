// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Strings/StringView.h"
#include "FileDescriptor.h"

namespace SC
{
struct Buffer;
struct String;
} // namespace SC
//! @addtogroup group_file
//! @{

/// @brief Wraps a SC::FileDescriptor to open it and use strings / buffers.
/// Example usage:
/// \snippet Tests/Libraries/File/FileTest.cpp FileSnippet
struct SC::File
{
    FileDescriptor& fd;

    File(FileDescriptor& descriptor) : fd(descriptor) {}

    /// @brief Define mode for opening the file (read, write etc.)
    enum OpenMode
    {
        ReadOnly,            ///< Opens in read-only mode
        WriteCreateTruncate, ///< Opens in write mode, creating or truncating it if another file exists at same location
        WriteAppend,         ///< Opens write mode, appending to existing file that must exist at the same location.
        ReadAndWrite         ///< Opens file for read / write mode
    };

    /// @brief Additional flags to be set when opening files
    struct OpenOptions
    {
        bool inheritable = false; ///< Set to true to make the file visible to child processes
        bool blocking    = true;  ///< Set to false if file will be used for Async I/O (see @ref library_async)
    };

    /// @brief Opens file at `path` with a given `mode`
    /// @param path The path to file
    /// @param mode The mode used to open file (read-only, write-append etc.)
    /// @return Valid Result if file is opened successfully
    [[nodiscard]] Result open(StringView path, OpenMode mode);

    /// @brief Opens file at `path` with a given `mode`
    /// @param path The path to file
    /// @param mode The mode used to open file
    /// @param options Options that can be applied when opening the file (inheritable, blocking etc.)
    /// @return Valid Result if file is opened successfully
    [[nodiscard]] Result open(StringView path, OpenMode mode, OpenOptions options);

    /// @brief Reads into a given dynamic buffer until End of File (EOF) is signaled.
    ///        It works also for non-seekable file descriptors (stdout / in / err).
    /// @param destination A destination buffer to write to (it will be resized as needed)
    /// @return Valid result if read succeeded until EOF
    [[nodiscard]] Result readUntilEOF(Buffer& destination);

    /// @brief Reads into a given string until End of File (EOF) is signaled
    ///        It works also for non-seekable file descriptors (stdout / in / err).
    /// @param destination A destination string to write to (it will be sized as needed)
    /// @return Valid result if read succeeded until EOF
    [[nodiscard]] Result readUntilEOF(String& destination);

  private:
    struct Internal;
    struct ReadResult;
    Result readUntilEOFTemplate(Buffer& destination);
};

//! @}
