// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Result.h"
#include "../Foundation/Span.h"
#include "../Foundation/UniqueHandle.h"
#include "../Strings/StringView.h"
//! @defgroup group_file File
//! @copybrief library_file
//!
//! See @ref library_file library page for more details.<br>

namespace SC
{
struct String;
template <typename T>
struct Vector;

struct FileDescriptor;
struct PipeDescriptor;
struct FileDescriptorDefinition;
} // namespace SC

//! @addtogroup group_file
//! @{

#if SC_PLATFORM_WINDOWS

struct SC::FileDescriptorDefinition
{
    using Handle = void*; // HANDLE
    static Result releaseHandle(Handle& handle);
#ifdef __clang__
    static constexpr void* Invalid = __builtin_constant_p(-1) ? (void*)-1 : (void*)-1; // INVALID_HANDLE_VALUE
#else
    static constexpr void* Invalid = (void*)-1; // INVALID_HANDLE_VALUE
#endif
};

#else
/// @brief Definition used to declare FileDescriptor (as argument to UniqueHandle)
struct SC::FileDescriptorDefinition
{
    using Handle = int; // fd
    static Result releaseHandle(Handle& handle);

    static constexpr Handle Invalid = -1; // invalid fd
};

#endif

/// @brief Wraps an OS File descriptor to read and write to and from it.
struct SC::FileDescriptor : public SC::UniqueHandle<SC::FileDescriptorDefinition>
{
    using UniqueHandle::UniqueHandle;
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
        bool inheritable = false; ///< Specify if file will be visible to child processes
        bool blocking    = true;  ///< Specify if reads or writes to file should be blocking for IO until done
        bool async       = false; ///< Specify if file will be used for Async I/O (see @ref library_async)
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

    /// @brief Set blocking mode (read / write waiting for I/O). Can be set also during open with OpenOptions.
    /// @param blocking `true` to set file to blocking mode
    /// @return `true` if blocking mode has been changed successfully
    [[nodiscard]] Result setBlocking(bool blocking);

    /// @brief Set inheritable flag (visibility to child processes). Can be set also during open with OpenOptions.
    /// @param inheritable `true` to set file to blocking mode
    /// @return `true` if blocking mode has been changed successfully
    [[nodiscard]] Result setInheritable(bool inheritable);

    /// @brief Queries the inheritable state of this decriptor
    /// @param hasValue will be set to true if the file descriptor has inheritable file set
    /// @return Valid Result if the inheritable flag has been queried successfully
    [[nodiscard]] Result isInheritable(bool& hasValue) const;

    /// @brief Reads bytes at offset into user supplied span
    /// @param data Span of bytes where data should be written to
    /// @param actuallyRead A sub-span of data of the actually read bytes. A zero sized span means EOF.
    /// @param offset Offset from begin of the file descriptor where read should be started
    /// @return Valid result if read succeeded
    [[nodiscard]] Result read(Span<char> data, Span<char>& actuallyRead, uint64_t offset);

    /// @brief Reads bytes from current position (FileDescriptor::seek) into user supplied Span
    /// @param data Span of bytes where data should be written to
    /// @param actuallyRead A sub-span of data of the actually read bytes. A zero sized span means EOF.
    /// @return Valid result if read succeeded
    [[nodiscard]] Result read(Span<char> data, Span<char>& actuallyRead);

    /// @brief Writes bytes at offset from start of the file descriptor
    /// @param data Span of bytes containing the data to write
    /// @param offset Offset from begin of file descriptor to start writing
    /// @return Valid result if write succeded
    [[nodiscard]] Result write(Span<const char> data, uint64_t offset);

    /// @brief Writes bytes from current position (FileDescriptor::seek) of the file descriptor
    /// @param data Span of bytes containing the data to write
    /// @return Valid result if write succeded
    [[nodiscard]] Result write(Span<const char> data);

    /// @brief How the offset to FileDescriptor::seek is defined
    enum SeekMode
    {
        SeekStart,   ///< Offset to FileDescriptor::seek is to be applied from start of descriptor
        SeekEnd,     ///< Offset to FileDescriptor::seek is to be applied (backwards) from end of descriptor
        SeekCurrent, ///< Offset to FileDescriptor::seek is to be applied from current descriptor position
    };

    /// @brief Changes the current position in the file descriptor, if seekable.
    /// @param seekMode How the offset is defined (from start, end, current)
    /// @param offset An offset to be applied according to seekMode to this descriptor
    /// @return Valid result if seek succeeed
    [[nodiscard]] Result seek(SeekMode seekMode, uint64_t offset);

    /// @brief Gets current descriptor position (if seekable)
    /// @param position (output) current position of file descriptor
    /// @return Valid result if seek succeeed
    [[nodiscard]] Result currentPosition(size_t& position) const;

    /// @brief Gets total file size in bytes (if seekable)
    /// @param sizeInBytes (output) total size of file
    /// @return Valid result if seek succeeed
    [[nodiscard]] Result sizeInBytes(size_t& sizeInBytes) const;

    /// @brief Reads into a given dynamic buffer until End of File (EOF) is signaled.
    ///         Works also for non-seekable file descriptors (stdout / in / err).
    /// @param destination A destination buffer to write to (it will be resized as needed)
    /// @return Valid result if read succeded until EOF
    [[nodiscard]] Result readUntilEOF(Vector<char>& destination);

    /// @brief Reads into a given string until End of File (EOF) is signaled
    ///         Works also for non-seekable file descriptors (stdout / in / err).
    /// @param destination A destination string to write to (it will be sized as needed)
    /// @return Valid result if read succeded until EOF
    [[nodiscard]] Result readUntilEOF(String& destination);

    /// @brief Results of readAppend function
    struct ReadResult
    {
        size_t actuallyRead = 0;
        bool   isEOF        = false;
    };

    /// @brief Reads bytes appending to buffer.
    /// @param buffer Destination buffer where bytes will be written to
    /// @param fallbackBuffer if buffer is full (capacity() == size()) then data will be read in fallbackBuffer first
    /// and only later appended to buffer. This allows sizing the buffer only to the actually needed size without
    /// having to resize the buffer beforehand.
    /// @param result returns information about EOF being reached and actuallyRead bytes
    /// @return Valid result if read succeded
    [[nodiscard]] Result readAppend(Vector<char>& buffer, Span<char> fallbackBuffer, ReadResult& result);

  private:
    struct Internal;
};

/// @brief Descriptor representing a Pipe used for InterProcess Communication (IPC)
struct SC::PipeDescriptor
{
    /// @brief Specifies a flag for read side of the pipe
    enum InheritableReadFlag
    {
        ReadInheritable,   ///< Requests read side of the pipe to be inheritable from child processes
        ReadNonInheritable ///< Requests read side of the pipe not to be inheritable from child processes
    };
    /// @brief Specifies a flag for write side of the pipe
    enum InheritableWriteFlag
    {
        WriteInheritable,   ///< Requests write side of the pipe to be inheritable from child processes
        WriteNonInheritable ///< Requests write side of the pipe to be inheritable from child processes
    };
    FileDescriptor readPipe;  ///< The read side of the pipe
    FileDescriptor writePipe; ///< The write side of the pipe
    /// Creates a Pipe. Default is non-inheritable / blocking

    /// @brief Creates a Pipe. File descriptors are created with blocking mode enabled by default.
    /// @param readFlag Specifies how the read side should be created
    /// @param writeFlag Specifies how the write side should be created
    /// @return Valid Result if pipe creation succeeded
    [[nodiscard]] Result createPipe(InheritableReadFlag readFlag, InheritableWriteFlag writeFlag);

    /// @brief Closes the pipe
    /// @return Valid Result if pipe destruction succeeded
    [[nodiscard]] Result close();
};
//! @}
