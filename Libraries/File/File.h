// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Internal/IGrowableBuffer.h"
#include "../Foundation/Result.h"
#include "../Foundation/Span.h"
#include "../Foundation/StringSpan.h"
#include "../Foundation/UniqueHandle.h"

//! @defgroup group_file File
//! @copybrief library_file (see @ref library_file for more details)

namespace SC
{
namespace detail
{
#if SC_PLATFORM_WINDOWS
struct SC_COMPILER_EXPORT FileDescriptorDefinition
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
//! [UniqueHandleDeclaration1Snippet]

/// @brief Definition used to declare FileDescriptor (as argument to UniqueHandle)
struct SC_COMPILER_EXPORT FileDescriptorDefinition
{
    using Handle = int; // fd
    static Result releaseHandle(Handle& handle);

    static constexpr Handle Invalid = -1; // invalid fd
};
//! [UniqueHandleDeclaration1Snippet]
#endif
} // namespace detail

//! @addtogroup group_file
//! @{

/// @brief Options used to open a file descriptor
struct SC_COMPILER_EXPORT FileOpen
{
    /// @brief Indicates the mode in which the file should be opened (read, write, append, etc.)
    enum Mode : uint8_t
    {
        Read = 0,   ///< `r`  Open for reading. An error occurs if the file does not exist.
        ReadWrite,  ///< `r+` Open for reading and writing. An error occurs if the file does not exist.
        Append,     ///< `a`  Open for appending. The file is created if it does not exist.
        AppendRead, ///< `a+` Open for reading and appending. The file is created if it does not exist.
        Write,      ///< `w`  Open for writing. The file is created (if it does not exist) or truncated (if it exists).
        WriteRead,  ///< `w+` Open for reading and writing. The file is created (if it does not exist) or truncated.
    };

    FileOpen(Mode mode = Read) : mode(mode) {}

    Mode mode;                ///< Open mode (read, write, append, etc.). See FileOpen::Mode for more details.
    bool inheritable = false; ///< Set to true to make the file visible to child processes
    bool blocking    = true;  ///< Set to false if file will be used for Async I/O (see @ref library_async)
    bool sync        = false; ///< Set to true to open file in synchronous mode, bypassing local file system cache
    bool exclusive   = false; ///< Set to true to fail if the file already exists (like 'x' flag in fopen)

#if !SC_PLATFORM_WINDOWS
    int toPosixFlags() const;
    int toPosixAccess() const;
#endif
};
//! [UniqueHandleDeclaration2Snippet]

/// @brief Open, read and write to/from a file descriptor (like a file or pipe).
struct SC_COMPILER_EXPORT FileDescriptor : public UniqueHandle<detail::FileDescriptorDefinition>
{
    using UniqueHandle::UniqueHandle;

    ///...
    //! [UniqueHandleDeclaration2Snippet]

    /// @brief Opens a file descriptor handle for writing to /dev/null or equivalent on current OS.
    /// @return `true` if file has been opened successfully
    Result openForWriteToDevNull();

    /// @brief Opens a file descriptor handle from a file system path.
    /// @param path The absolute path to file. It MUST be encoded in ASCII,UTF-8/16 on Windows, ASCII,UTF-8 on POSIX.
    /// @param mode The mode used to open file (read-only, write-append etc.)
    /// @return Valid Result if file is opened successfully
    Result open(StringSpan path, FileOpen mode);

    /// @brief Set blocking mode (read / write waiting for I/O). Can be set also during open with OpenOptions.
    /// @param blocking `true` to set file to blocking mode
    /// @return `true` if blocking mode has been changed successfully
    Result setBlocking(bool blocking);

    /// @brief Set inheritable flag (visibility to child processes). Can be set also during open with OpenOptions.
    /// @param inheritable `true` to set file to blocking mode
    /// @return `true` if blocking mode has been changed successfully
    Result setInheritable(bool inheritable);

    /// @brief Queries the inheritable state of this descriptor
    /// @param hasValue will be set to true if the file descriptor has inheritable file set
    /// @return Valid Result if the inheritable flag has been queried successfully
    Result isInheritable(bool& hasValue) const;

    /// @brief Reads bytes at offset into user supplied span
    /// @param data Span of bytes where data should be written to
    /// @param actuallyRead A sub-span of data of the actually read bytes. A zero sized span means EOF.
    /// @param offset Offset from begin of the file descriptor where read should be started
    /// @return Valid result if read succeeded
    Result read(Span<char> data, Span<char>& actuallyRead, uint64_t offset);

    /// @brief Reads bytes at offset into user supplied span
    /// @param data Span of bytes where data should be written to
    /// @param actuallyRead A sub-span of data of the actually read bytes. A zero sized span means EOF.
    /// @param offset Offset from begin of the file descriptor where read should be started
    /// @return Valid result if read succeeded
    Result read(Span<uint8_t> data, Span<uint8_t>& actuallyRead, uint64_t offset);

    /// @brief Reads bytes from current position (FileDescriptor::seek) into user supplied Span
    /// @param data Span of bytes where data should be written to
    /// @param actuallyRead A sub-span of data of the actually read bytes. A zero sized span means EOF.
    /// @return Valid result if read succeeded
    Result read(Span<char> data, Span<char>& actuallyRead);

    /// @brief Reads bytes from current position (FileDescriptor::seek) into user supplied Span
    /// @param data Span of bytes where data should be written to
    /// @param actuallyRead A sub-span of data of the actually read bytes. A zero sized span means EOF.
    /// @return Valid result if read succeeded
    Result read(Span<uint8_t> data, Span<uint8_t>& actuallyRead);

    /// @brief Reads into a given dynamic buffer until End of File (EOF) is signaled.
    ///        It works also for non-seekable file descriptors (stdout / in / err).
    /// @tparam T Type of the destination buffer implementing IGrowableBuffer interface (String, SmallString, Buffer)
    /// @param destination A destination buffer to write to (it will be resized as needed)
    /// @return Valid result if read succeeded until EOF
    template <typename T>
    Result readUntilEOF(T& destination)
    {
        return readUntilEOF(GrowableBuffer<T>{destination});
    }

    /// @brief Reads into a given dynamic buffer until End of File (EOF) is signaled.
    ///        It works also for non-seekable file descriptors (stdout / in / err).
    /// @param buffer A destination buffer to write to (it will be resized as needed)
    /// @return Valid result if read succeeded until EOF
    Result readUntilEOF(IGrowableBuffer&& buffer);

    /// @brief Writes bytes at offset from start of the file descriptor
    /// @param data Span of bytes containing the data to write
    /// @param offset Offset from begin of file descriptor to start writing
    /// @return Valid result if write succeeded
    Result write(Span<const char> data, uint64_t offset);

    /// @brief Writes bytes at offset from start of the file descriptor
    /// @param data Span of bytes containing the data to write
    /// @param offset Offset from begin of file descriptor to start writing
    /// @return Valid result if write succeeded
    Result write(Span<const uint8_t> data, uint64_t offset);

    /// @brief Writes bytes from current position (FileDescriptor::seek) of the file descriptor
    /// @param data Span of bytes containing the data to write
    /// @return Valid result if write succeeded
    Result write(Span<const char> data);

    /// @brief Writes bytes from current position (FileDescriptor::seek) of the file descriptor
    /// @param data Span of bytes containing the data to write
    /// @return Valid result if write succeeded
    Result write(Span<const uint8_t> data);

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
    /// @return Valid result if seek succeeds
    Result seek(SeekMode seekMode, uint64_t offset);

    /// @brief Gets current descriptor position (if seekable)
    /// @param position (output) current position of file descriptor
    /// @return Valid result if seek succeeds
    Result currentPosition(size_t& position) const;

    /// @brief Gets total file size in bytes (if seekable)
    /// @param sizeInBytes (output) total size of file
    /// @return Valid result if seek succeeds
    Result sizeInBytes(size_t& sizeInBytes) const;

  private:
    friend struct File;
    struct Internal;
};

/// @brief Read / Write pipe (Process stdin/stdout and IPC communication)
struct SC_COMPILER_EXPORT PipeDescriptor
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

    /// @brief Creates a Pipe. File descriptors are created with blocking mode enabled by default.
    /// @param readFlag Specifies how the read side should be created
    /// @param writeFlag Specifies how the write side should be created
    /// @return Valid Result if pipe creation succeeded
    Result createPipe(InheritableReadFlag  readFlag  = ReadNonInheritable,
                      InheritableWriteFlag writeFlag = WriteNonInheritable);

    /// @brief Closes the pipe
    /// @return Valid Result if pipe destruction succeeded
    Result close();
};
//! @}
} // namespace SC
