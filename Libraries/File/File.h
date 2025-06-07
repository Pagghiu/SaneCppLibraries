// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Strings/StringView.h"
#include "FileDescriptor.h"

namespace SC
{
struct Buffer;
struct String;
//! @addtogroup group_file
//! @{

/// @brief Wraps a SC::FileDescriptor to open it and use strings / buffers.
/// Example usage:
/// \snippet Tests/Libraries/File/FileTest.cpp FileSnippet
struct SC_COMPILER_EXPORT File
{
    FileDescriptor& fd;

    File(FileDescriptor& descriptor) : fd(descriptor) {}

    /// @brief Opens file at `path` with a given `mode`
    /// @param path The path to file
    /// @param mode The mode used to open file (read-only, write-append etc.)
    /// @return Valid Result if file is opened successfully
    [[nodiscard]] Result open(StringView path, FileOpen mode);

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
    struct ReadResult
    {
        size_t actuallyRead = 0;
        bool   isEOF        = false;
    };
    Result readUntilEOFTemplate(Buffer& destination);
};

//! @}
} // namespace SC
