// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../File/File.h"
#include "../Foundation/StringPath.h"

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

//-------------------------------------------------------------------------------------------------------
// FileDescriptorDefinition
//-------------------------------------------------------------------------------------------------------
SC::Result SC::detail::FileDescriptorDefinition::releaseHandle(Handle& handle)
{
    BOOL res;
    __try
    {
        res = ::CloseHandle(handle);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        res = FALSE;
    }
    if (res == FALSE)
    {
        return Result::Error("FileDescriptorDefinition::releaseHandle - CloseHandle failed");
    }
    return Result(true);
}

//-------------------------------------------------------------------------------------------------------
// FileDescriptor
//-------------------------------------------------------------------------------------------------------
struct SC::FileDescriptor::Internal
{
    static Result readAppend(FileDescriptor::Handle fileDescriptor, IGrowableBuffer& buffer, Span<char> fallbackBuffer,
                             bool& isEOF)
    {
        auto  bufferData   = buffer.getDirectAccess();
        DWORD numReadBytes = 0xffffffff;
        BOOL  success;

        const bool useVector = bufferData.capacityInBytes > bufferData.sizeInBytes;
        if (useVector)
        {
            success = ::ReadFile(fileDescriptor, static_cast<char*>(bufferData.data) + bufferData.sizeInBytes,
                                 static_cast<DWORD>(bufferData.capacityInBytes - bufferData.sizeInBytes), &numReadBytes,
                                 nullptr);
        }
        else
        {
            SC_TRY_MSG(fallbackBuffer.sizeInBytes() != 0,
                       "FileDescriptor::readAppend - buffer must be bigger than zero");
            success = ::ReadFile(fileDescriptor, fallbackBuffer.data(),
                                 static_cast<DWORD>(fallbackBuffer.sizeInBytes()), &numReadBytes, nullptr);
        }
        if (Internal::isActualError(success, numReadBytes, fileDescriptor))
        {
            // TODO: Parse read result ERROR
            return Result::Error("FileDescriptor::readAppend ReadFile failed");
        }
        else if (numReadBytes > 0)
        {
            SC_TRY_MSG(buffer.resizeWithoutInitializing(bufferData.sizeInBytes + static_cast<size_t>(numReadBytes)),
                       "FileDescriptor::readAppend - resize failed");
            if (not useVector)
            {
                auto newBufferData = buffer.getDirectAccess();
                ::memcpy(static_cast<char*>(newBufferData.data) + bufferData.sizeInBytes, fallbackBuffer.data(),
                         static_cast<size_t>(numReadBytes));
            }
            isEOF = false;
            return Result(true);
        }
        else
        {
            // EOF
            isEOF = true;
            return Result(true);
        }
    }

    [[nodiscard]] static bool isActualError(BOOL success, DWORD numReadBytes, FileDescriptor::Handle fileDescriptor)
    {
        if (success == FALSE && numReadBytes == 0 && GetFileType(fileDescriptor) == FILE_TYPE_PIPE &&
            GetLastError() == ERROR_BROKEN_PIPE)
        {
            // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile
            // Pipes

            // If an anonymous pipe is being used and the write handle has been closed, when ReadFile attempts to read
            // using the pipe's corresponding read handle, the function returns FALSE and GetLastError returns
            // ERROR_BROKEN_PIPE.
            return false;
        }
        return success == FALSE;
    }
};

SC::Result SC::FileDescriptor::seek(SeekMode seekMode, int64_t offset)
{
    int flags = 0;
    switch (seekMode)
    {
    case SeekMode::SeekStart: flags = FILE_BEGIN; break;
    case SeekMode::SeekEnd: flags = FILE_END; break;
    case SeekMode::SeekCurrent: flags = FILE_CURRENT; break;
    }
    const DWORD offsetLow  = static_cast<DWORD>(offset & 0xffffffff);
    DWORD       offsetHigh = static_cast<DWORD>((offset >> 32) & 0xffffffff);
    const DWORD newPos     = ::SetFilePointer(handle, offsetLow, (LONG*)&offsetHigh, flags);
    SC_TRY_MSG(newPos != INVALID_SET_FILE_POINTER, "SetFilePointer failed");
    return Result(true);
}

SC::Result SC::FileDescriptor::currentPosition(size_t& position) const
{
    LARGE_INTEGER li, source;
    memset(&source, 0, sizeof(source));
    if (::SetFilePointerEx(handle, source, &li, FILE_CURRENT) != 0)
    {
        position = static_cast<size_t>(li.QuadPart);
        return Result(true);
    }
    return Result::Error("SetFilePointerEx failed");
}

SC::Result SC::FileDescriptor::sizeInBytes(size_t& sizeInBytes) const
{
    LARGE_INTEGER li;
    if (GetFileSizeEx(handle, &li) != 0)
    {
        sizeInBytes = static_cast<size_t>(li.QuadPart);
        return Result(true);
    }
    return Result::Error("GetFileSizeEx failed");
}

SC::Result SC::FileDescriptor::write(Span<const char> data, uint64_t offset)
{
    SC_TRY(seek(SeekStart, offset));
    return write(data);
}

SC::Result SC::FileDescriptor::write(Span<const char> data)
{
    DWORD      numberOfWrittenBytes;
    const BOOL res =
        ::WriteFile(handle, data.data(), static_cast<DWORD>(data.sizeInBytes()), &numberOfWrittenBytes, nullptr);
    SC_TRY_MSG(res, "WriteFile failed");
    return Result(static_cast<size_t>(numberOfWrittenBytes) == data.sizeInBytes());
}

SC::Result SC::FileDescriptor::read(Span<char> data, Span<char>& actuallyRead, uint64_t offset)
{
    SC_TRY(seek(SeekStart, offset));
    return read(data, actuallyRead);
}

SC::Result SC::FileDescriptor::read(Span<char> data, Span<char>& actuallyRead)
{
    DWORD      numberOfReadBytes = 0;
    const BOOL res =
        ::ReadFile(handle, data.data(), static_cast<DWORD>(data.sizeInBytes()), &numberOfReadBytes, nullptr);
    if (res == FALSE and ::GetLastError() != ERROR_BROKEN_PIPE)
    {
        return Result::Error("ReadFile failed");
    }
    return Result(data.sliceStartLength(0, static_cast<size_t>(numberOfReadBytes), actuallyRead));
}

SC::Result SC::FileDescriptor::open(StringSpan filePath, FileOpen mode)
{
    StringPath nullTerminated;
    SC_TRY_MSG(nullTerminated.assign(filePath), "FileDescriptor::open - Path too long or invalid encoding");
    const wchar_t* nullTerminatedPath = nullTerminated.view().getNullTerminatedNative();

    const bool isThreeChars = filePath.sizeInBytes() >= 3 * sizeof(wchar_t);
    if (not isThreeChars or
        (nullTerminatedPath[0] != L'\\' and nullTerminatedPath[1] != L':' and wcscmp(nullTerminatedPath, L"NUL") != 0))
    {
        return Result::Error("FileDescriptor::open - Path must be absolute");
    }

    DWORD accessMode        = 0;
    DWORD createDisposition = 0;
    DWORD fileFlags         = mode.blocking ? 0 : FILE_FLAG_OVERLAPPED;

    switch (mode.mode)
    {
    case FileOpen::Read:
        accessMode |= FILE_GENERIC_READ;
        createDisposition = OPEN_EXISTING;
        break;
    case FileOpen::Write:
        accessMode |= FILE_GENERIC_WRITE;
        createDisposition = CREATE_ALWAYS;
        break;
    case FileOpen::Append:
        accessMode |= FILE_APPEND_DATA;
        createDisposition = OPEN_ALWAYS;
        break;
    case FileOpen::ReadWrite:
        accessMode |= FILE_GENERIC_READ | FILE_GENERIC_WRITE;
        createDisposition = OPEN_ALWAYS;
        break;
    case FileOpen::WriteRead:
        accessMode |= FILE_GENERIC_READ | FILE_GENERIC_WRITE;
        createDisposition = CREATE_ALWAYS;
        break;
    case FileOpen::AppendRead:
        accessMode |= FILE_GENERIC_READ | FILE_APPEND_DATA;
        createDisposition = OPEN_ALWAYS;
        break;
    }

    if (mode.sync)
    {
        fileFlags |= FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING;
    }

    if (mode.exclusive)
    {
        createDisposition = CREATE_NEW;
    }

    DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;

    SECURITY_ATTRIBUTES security;
    security.nLength              = sizeof(SECURITY_ATTRIBUTES);
    security.bInheritHandle       = mode.inheritable ? TRUE : FALSE;
    security.lpSecurityDescriptor = nullptr;

    HANDLE fileDescriptor =
        ::CreateFileW(nullTerminatedPath, accessMode, shareMode, &security, createDisposition, fileFlags, nullptr);

    SC_TRY_MSG(fileDescriptor != INVALID_HANDLE_VALUE, "CreateFileW failed");
    return assign(fileDescriptor);
}

#else
//! [UniqueHandleDefinitionSnippet]
#include <errno.h>      // errno
#include <fcntl.h>      // fcntl
#include <sys/socket.h> // socket/connect/accept
#include <sys/stat.h>   // fstat
#include <sys/un.h>     // sockaddr_un
#include <unistd.h>     // close

//-------------------------------------------------------------------------------------------------------
// FileDescriptorDefinition
//-------------------------------------------------------------------------------------------------------
SC::Result SC::detail::FileDescriptorDefinition::releaseHandle(Handle& handle)
{
    if (::close(handle) != 0)
    {
        return Result::Error("FileDescriptorDefinition::releaseHandle - close failed");
    }
    return Result(true);
}
//! [UniqueHandleDefinitionSnippet]

//-------------------------------------------------------------------------------------------------------
// FileDescriptor
//-------------------------------------------------------------------------------------------------------
struct SC::FileDescriptor::Internal
{
    static Result readAppend(FileDescriptor::Handle fileDescriptor, IGrowableBuffer& buffer, Span<char> fallbackBuffer,
                             bool& isEOF)
    {
        auto       bufferData = buffer.getDirectAccess();
        ssize_t    numReadBytes;
        const bool useVector = bufferData.capacityInBytes > bufferData.sizeInBytes;
        if (useVector)
        {
            do
            {
                const size_t bytesToRead = (bufferData.capacityInBytes - bufferData.sizeInBytes);
                numReadBytes =
                    ::read(fileDescriptor, static_cast<char*>(bufferData.data) + bufferData.sizeInBytes, bytesToRead);
            } while (numReadBytes == -1 && errno == EINTR); // Syscall may be interrupted and userspace must retry
        }
        else
        {
            SC_TRY_MSG(fallbackBuffer.sizeInBytes() != 0,
                       "FileDescriptor::readAppend - buffer must be bigger than zero");
            do
            {
                numReadBytes = ::read(fileDescriptor, fallbackBuffer.data(), fallbackBuffer.sizeInBytes());
            } while (numReadBytes == -1 && errno == EINTR); // Syscall may be interrupted and userspace must retry
        }
        if (numReadBytes > 0)
        {
            SC_TRY_MSG(buffer.resizeWithoutInitializing(bufferData.sizeInBytes + static_cast<size_t>(numReadBytes)),
                       "FileDescriptor::readAppend - resize failed");
            if (not useVector)
            {
                auto newBufferData = buffer.getDirectAccess();
                ::memcpy(static_cast<char*>(newBufferData.data) + bufferData.sizeInBytes, fallbackBuffer.data(),
                         static_cast<size_t>(numReadBytes));
            }
            isEOF = false;
            return Result(true);
        }
        else if (numReadBytes == 0)
        {
            // EOF
            isEOF = true;
            return Result(true);
        }
        else
        {
            // TODO: Parse read result errno
            return Result::Error("read failed");
        }
    }

    static Result setFileFlags(int flagRead, int flagWrite, const int fileDescriptor, const bool setFlag,
                               const int flag)
    {
        int oldFlags;
        do
        {
            oldFlags = ::fcntl(fileDescriptor, flagRead);
        } while (oldFlags == -1 && errno == EINTR);
        SC_TRY_MSG(oldFlags != -1, "fcntl getFlag failed");
        const int newFlags = setFlag ? oldFlags | flag : oldFlags & (~flag);
        if (newFlags != oldFlags)
        {
            int res;
            do
            {
                res = ::fcntl(fileDescriptor, flagWrite, newFlags);
            } while (res == -1 && errno == EINTR);
            SC_TRY_MSG(res == 0, "fcntl setFlag failed");
        }
        return SC::Result(true);
    }

    template <int flag>
    static Result setFileDescriptorFlags(int fileDescriptor, bool setFlag)
    {
        // We can OR the allowed flags here to provide some safety
        static_assert(flag == FD_CLOEXEC, "setFileStatusFlags invalid value");
        return setFileFlags(F_GETFD, F_SETFD, fileDescriptor, setFlag, flag);
    }

    template <int flag>
    static Result setFileStatusFlags(int fileDescriptor, bool setFlag)
    {
        // We can OR the allowed flags here to provide some safety
        static_assert(flag == O_NONBLOCK, "setFileStatusFlags invalid value");
        return setFileFlags(F_GETFL, F_SETFL, fileDescriptor, setFlag, flag);
    }
};

int SC::FileOpen::toPosixFlags() const
{
    int flags = 0;
    switch (mode)
    {
    case FileOpen::Read: flags |= O_RDONLY; break;
    case FileOpen::Write: flags |= O_WRONLY | O_CREAT | O_TRUNC; break;
    case FileOpen::Append: flags |= O_WRONLY | O_APPEND | O_CREAT; break;
    case FileOpen::ReadWrite: flags |= O_RDWR; break;
    case FileOpen::WriteRead: flags |= O_RDWR | O_CREAT | O_TRUNC; break;
    case FileOpen::AppendRead: flags |= O_RDWR | O_APPEND | O_CREAT; break;
    }

    if (sync)
    {
        flags |= O_SYNC;
    }

    if (exclusive)
    {
        flags |= O_EXCL;
    }

    if (not inheritable)
    {
        flags |= O_CLOEXEC;
    }
    return flags;
}

int SC::FileOpen::toPosixAccess() const { return S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH; }

SC::Result SC::FileDescriptor::open(StringSpan filePath, FileOpen mode)
{
    SC_TRY_MSG(filePath.getEncoding() != StringEncoding::Utf16,
               "FileDescriptor::open: POSIX supports only UTF8 and ASCII encoding");
    const int flags  = mode.toPosixFlags();
    const int access = mode.toPosixAccess();

    StringPath nullTerminated;
    SC_TRY_MSG(nullTerminated.assign(filePath), "FileDescriptor::open - Path too long or invalid encoding");
    const char* nullTerminatedPath = nullTerminated.view().bytesIncludingTerminator();
    SC_TRY_MSG(nullTerminatedPath[0] == '/', "FileDescriptor::open - Path must be absolute");
    const int fileDescriptor = ::open(nullTerminatedPath, flags, access);
    SC_TRY_MSG(fileDescriptor != -1, "open failed");
    SC_TRY(assign(fileDescriptor));
    if (not mode.blocking)
    {
        SC_TRY(Internal::setFileStatusFlags<O_NONBLOCK>(handle, true));
    }
    return Result(true);
}

SC::Result SC::FileDescriptor::seek(SeekMode seekMode, int64_t offset)
{
    int flags = 0;
    switch (seekMode)
    {
    case SeekMode::SeekStart: flags = SEEK_SET; break;
    case SeekMode::SeekEnd: flags = SEEK_END; break;
    case SeekMode::SeekCurrent: flags = SEEK_CUR; break;
    }
    const off_t res = ::lseek(handle, static_cast<off_t>(offset), flags);
    SC_TRY_MSG(res >= 0, "lseek failed");
    return Result(true);
}

SC::Result SC::FileDescriptor::currentPosition(size_t& position) const
{
    const off_t fileSize = ::lseek(handle, 0, SEEK_CUR);
    SC_TRY_MSG(fileSize >= 0, "lseek failed");
    position = static_cast<size_t>(fileSize);
    return Result(true);
}

SC::Result SC::FileDescriptor::sizeInBytes(size_t& sizeInBytes) const
{
    struct stat fileStat;
    SC_TRY_MSG(::fstat(handle, &fileStat) == 0, "fstat failed");
    sizeInBytes = static_cast<size_t>(fileStat.st_size);
    return Result(true);
}

SC::Result SC::FileDescriptor::write(Span<const char> data, uint64_t offset)
{
    ssize_t res;
    do
    {
        res = ::pwrite(handle, data.data(), data.sizeInBytes(), static_cast<off_t>(offset));
    } while (res == -1 and errno == EINTR);
    SC_TRY_MSG(res >= 0, "pwrite failed");
    return Result(static_cast<size_t>(res) == data.sizeInBytes());
}

SC::Result SC::FileDescriptor::write(Span<const char> data)
{
    ssize_t res;
    do
    {
        res = ::write(handle, data.data(), data.sizeInBytes());
    } while (res == -1 and errno == EINTR);
    SC_TRY_MSG(res >= 0, "write failed");
    return Result(static_cast<size_t>(res) == data.sizeInBytes());
}

SC::Result SC::FileDescriptor::read(Span<char> data, Span<char>& actuallyRead, uint64_t offset)
{
    ssize_t res;
    do
    {
        res = ::pread(handle, data.data(), data.sizeInBytes(), static_cast<off_t>(offset));
    } while (res == -1 and errno == EINTR);
    SC_TRY_MSG(res >= 0, "pread failed");
    return Result(data.sliceStartLength(0, static_cast<size_t>(res), actuallyRead));
}

SC::Result SC::FileDescriptor::read(Span<char> data, Span<char>& actuallyRead)
{
    ssize_t res;
    do
    {
        res = ::read(handle, data.data(), data.sizeInBytes());
    } while (res == -1 and errno == EINTR);
    SC_TRY_MSG(res >= 0, "read failed");
    return Result(data.sliceStartLength(0, static_cast<size_t>(res), actuallyRead));
}
#endif

//-------------------------------------------------------------------------------------------------------
// FileDescriptor (shared)
//-------------------------------------------------------------------------------------------------------

SC::Result SC::FileDescriptor::openForWriteToDevNull()
{
#if SC_PLATFORM_WINDOWS
    return open(L"NUL", FileOpen::Append);
#else
    return open("/dev/null", FileOpen::Append);
#endif
}

SC::Result SC::FileDescriptor::openStdOutDuplicate()
{
#if SC_PLATFORM_WINDOWS
    HANDLE stdHandle = ::GetStdHandle(STD_OUTPUT_HANDLE);
    if (stdHandle == INVALID_HANDLE_VALUE)
    {
        return Result::Error("GetStdHandle failed");
    }
    HANDLE duplicated;
    BOOL   res = ::DuplicateHandle(::GetCurrentProcess(), stdHandle, ::GetCurrentProcess(), &duplicated, 0, TRUE,
                                   DUPLICATE_SAME_ACCESS);
    if (res == FALSE)
    {
        return Result::Error("DuplicateHandle failed");
    }
    return assign(duplicated);
#else
    const int duplicated = ::dup(STDOUT_FILENO);
    SC_TRY_MSG(duplicated != -1, "dup failed");
    return assign(duplicated);
#endif
}

SC::Result SC::FileDescriptor::openStdErrDuplicate()
{
#if SC_PLATFORM_WINDOWS
    HANDLE stdHandle = ::GetStdHandle(STD_ERROR_HANDLE);
    if (stdHandle == INVALID_HANDLE_VALUE)
    {
        return Result::Error("GetStdHandle failed");
    }
    HANDLE duplicated;
    BOOL   res = ::DuplicateHandle(::GetCurrentProcess(), stdHandle, ::GetCurrentProcess(), &duplicated, 0, TRUE,
                                   DUPLICATE_SAME_ACCESS);
    if (res == FALSE)
    {
        return Result::Error("DuplicateHandle failed");
    }
    return assign(duplicated);
#else
    const int duplicated = ::dup(STDERR_FILENO);
    SC_TRY_MSG(duplicated != -1, "dup failed");
    return assign(duplicated);
#endif
}

SC::Result SC::FileDescriptor::openStdInDuplicate()
{
#if SC_PLATFORM_WINDOWS
    HANDLE stdHandle = ::GetStdHandle(STD_INPUT_HANDLE);
    if (stdHandle == INVALID_HANDLE_VALUE)
    {
        return Result::Error("GetStdHandle failed");
    }
    HANDLE duplicated;
    BOOL   res = ::DuplicateHandle(::GetCurrentProcess(), stdHandle, ::GetCurrentProcess(), &duplicated, 0, TRUE,
                                   DUPLICATE_SAME_ACCESS);
    if (res == FALSE)
    {
        return Result::Error("DuplicateHandle failed");
    }
    return assign(duplicated);
#else
    const int duplicated = ::dup(STDIN_FILENO);
    SC_TRY_MSG(duplicated != -1, "dup failed");
    return assign(duplicated);
#endif
}

SC::Result SC::FileDescriptor::writeString(StringSpan data) { return write(data.toCharSpan()); }

SC::Result SC::FileDescriptor::write(Span<const uint8_t> data, uint64_t offset)
{
    return write({reinterpret_cast<const char*>(data.data()), data.sizeInBytes()}, offset);
}

SC::Result SC::FileDescriptor::read(Span<uint8_t> data, Span<uint8_t>& actuallyRead)
{
    Span<char> readBytes;
    SC_TRY(read({reinterpret_cast<char*>(data.data()), data.sizeInBytes()}, readBytes));
    actuallyRead = {reinterpret_cast<uint8_t*>(readBytes.data()), readBytes.sizeInBytes()};
    return Result(true);
}

SC::Result SC::FileDescriptor::read(Span<uint8_t> data, Span<uint8_t>& actuallyRead, uint64_t offset)
{
    Span<char> readBytes;
    SC_TRY(read({reinterpret_cast<char*>(data.data()), data.sizeInBytes()}, readBytes, offset));
    actuallyRead = {reinterpret_cast<uint8_t*>(readBytes.data()), readBytes.sizeInBytes()};
    return Result(true);
}

SC::Result SC::FileDescriptor::write(Span<const uint8_t> data)
{
    return write({reinterpret_cast<const char*>(data.data()), data.sizeInBytes()});
}

SC::Result SC::FileDescriptor::readUntilFullOrEOF(Span<char> data, Span<char>& actuallyRead)
{
    auto availableData = data;
    while (not availableData.empty())
    {
        Span<char> readData;
        SC_TRY(read(availableData, readData));
        if (readData.empty())
            break;
        SC_TRY(availableData.sliceStartLength(readData.sizeInBytes(),
                                              availableData.sizeInBytes() - readData.sizeInBytes(), availableData));
    }
    SC_TRY(data.sliceStartLength(0, data.sizeInBytes() - availableData.sizeInBytes(), actuallyRead));
    return Result(true);
}

SC::Result SC::FileDescriptor::readUntilEOF(IGrowableBuffer&& adapter)
{
    char buffer[1024];
    SC_TRY_MSG(isValid(), "FileDescriptor::readUntilEOFGrowable - Invalid handle");
    bool isEOF = false;
    SC_TRY_MSG(adapter.resizeWithoutInitializing(0), "FileDescriptor::readUntilEOFGrowable - Cannot reset string");
    while (not isEOF)
    {
        SC_TRY(Internal::readAppend(handle, adapter, {buffer, sizeof(buffer)}, isEOF));
    }
    return Result(true);
}

//-------------------------------------------------------------------------------------------------------
// PipeDescriptor
//-------------------------------------------------------------------------------------------------------
#if SC_PLATFORM_WINDOWS
#include <stdio.h>
SC::Result SC::PipeDescriptor::createPipe(PipeOptions options)
{
    // On Windows to inherit flags they must be flagged as inheritable
    // https://devblogs.microsoft.com/oldnewthing/20111216-00/?p=8873
    SECURITY_ATTRIBUTES security;
    ::memset(&security, 0, sizeof(security));
    security.nLength              = sizeof(security);
    security.bInheritHandle       = options.readInheritable or options.writeInheritable ? TRUE : FALSE;
    security.lpSecurityDescriptor = nullptr;

    HANDLE pipeRead  = INVALID_HANDLE_VALUE;
    HANDLE pipeWrite = INVALID_HANDLE_VALUE;

    if (options.blocking == false)
    {
        char pipeName[64];
#if SC_PLATFORM_64_BIT
        snprintf(pipeName, sizeof(pipeName), "\\\\.\\pipe\\SC-%lu-%llu", ::GetCurrentProcessId(), (intptr_t)this);
#else
        snprintf(pipeName, sizeof(pipeName), "\\\\.\\pipe\\SC-%lu-%lu", ::GetCurrentProcessId(), (intptr_t)this);
#endif

        DWORD pipeFlags = PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED;
        DWORD pipeMode  = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT;

        pipeRead = ::CreateNamedPipeA(pipeName, pipeFlags, pipeMode, 1, 65536, 65536, 0, &security);
        if (pipeRead == INVALID_HANDLE_VALUE)
        {
            return Result::Error("PipeDescriptor::createPipe - CreateNamedPipeW failed");
        }
        pipeWrite = ::CreateFileA(pipeName, GENERIC_WRITE | FILE_READ_ATTRIBUTES, 0, &security, OPEN_EXISTING,
                                  FILE_FLAG_OVERLAPPED, nullptr);
        if (pipeWrite == INVALID_HANDLE_VALUE)
        {
            ::CloseHandle(pipeRead);
            return Result::Error("PipeDescriptor::createPipe - CreateFileW failed");
        }
        if (::ConnectNamedPipe(pipeRead, nullptr) == FALSE) // Connect the pipe immediately
        {
            if (GetLastError() != ERROR_PIPE_CONNECTED)
            {
                ::CloseHandle(pipeRead);
                ::CloseHandle(pipeWrite);
                return Result::Error("PipeDescriptor::createPipe - ConnectNamedPipe failed");
            }
        }
    }
    else
    {
        if (::CreatePipe(&pipeRead, &pipeWrite, &security, 0) == FALSE)
        {
            return Result::Error("PipeDescriptor::createPipe - ::CreatePipe failed");
        }
    }
    SC_TRY(readPipe.assign(pipeRead));
    SC_TRY(writePipe.assign(pipeWrite));

    if (security.bInheritHandle)
    {
        if (not options.readInheritable)
        {
            if (::SetHandleInformation(pipeRead, HANDLE_FLAG_INHERIT, FALSE) == FALSE)
            {
                return Result::Error("Cannot set read pipe inheritable");
            }
        }
        if (not options.writeInheritable)
        {
            if (::SetHandleInformation(pipeWrite, HANDLE_FLAG_INHERIT, FALSE) == FALSE)
            {
                return Result::Error("Cannot set write pipe inheritable");
            }
        }
    }
    return Result(true);
}

#else
SC::Result SC::PipeDescriptor::createPipe(PipeOptions options)
{
    int  pipes[2];
    int  res                     = -1;
    bool usedPipeCreationFlags   = false;
    bool usedNonBlockingAtCreate = false;
#if SC_PLATFORM_LINUX
    int pipeFlags = O_CLOEXEC;
    if (options.blocking == false)
    {
        pipeFlags |= O_NONBLOCK;
    }
    do
    {
        res = ::pipe2(pipes, pipeFlags);
    } while (res == -1 and errno == EINTR);
    if (res == 0)
    {
        usedPipeCreationFlags   = true;
        usedNonBlockingAtCreate = options.blocking == false;
    }
    else if (errno != ENOSYS and errno != EINVAL)
    {
        return Result::Error("PipeDescriptor::createPipe - pipe2 failed");
    }
#endif
    if (not usedPipeCreationFlags)
    {
        do
        {
            res = ::pipe(pipes);
        } while (res == -1 and errno == EINTR);
    }

    SC_TRY_MSG(res == 0, "PipeDescriptor::createPipe - pipe failed");
    SC_TRY_MSG(readPipe.assign(pipes[0]), "Cannot assign read pipe");
    SC_TRY_MSG(writePipe.assign(pipes[1]), "Cannot assign write pipe");
    const Result setReadCloExec =
        FileDescriptor::Internal::setFileDescriptorFlags<FD_CLOEXEC>(pipes[0], not options.readInheritable);
    SC_TRY_MSG(setReadCloExec, "Cannot set close on exec on read pipe");
    const Result setWriteCloExec =
        FileDescriptor::Internal::setFileDescriptorFlags<FD_CLOEXEC>(pipes[1], not options.writeInheritable);
    SC_TRY_MSG(setWriteCloExec, "Cannot set close on exec on write pipe");
    if (options.blocking == false and not usedNonBlockingAtCreate)
    {
        const Result pipeRes1 = FileDescriptor::Internal::setFileStatusFlags<O_NONBLOCK>(pipes[0], true);
        SC_TRY_MSG(pipeRes1, "Cannot set non-blocking flag on read");
        const Result pipeRes2 = FileDescriptor::Internal::setFileStatusFlags<O_NONBLOCK>(pipes[1], true);
        SC_TRY_MSG(pipeRes2, "Cannot set non-blocking flag on read");
    }
    return Result(true);
}
#endif

SC::Result SC::PipeDescriptor::close()
{
    SC_TRY(readPipe.close());
    return writePipe.close();
}

namespace
{
static SC::Result validateNamedPipeLogicalName(SC::StringSpan logicalName)
{
    SC_TRY_MSG(logicalName.getEncoding() != SC::StringEncoding::Utf16,
               "NamedPipeName::build logicalName only ASCII/UTF8");
    SC_TRY_MSG(not logicalName.isEmpty(), "NamedPipeName::build logicalName cannot be empty");

    const char*  bytes = logicalName.bytesWithoutTerminator();
    const size_t size  = logicalName.sizeInBytes();
    for (size_t i = 0; i < size; ++i)
    {
        SC_TRY_MSG(bytes[i] != '\0', "NamedPipeName::build logicalName contains null bytes");
        SC_TRY_MSG(bytes[i] != '/', "NamedPipeName::build logicalName cannot contain path separators");
        SC_TRY_MSG(bytes[i] != '\\', "NamedPipeName::build logicalName cannot contain path separators");
    }
    return SC::Result(true);
}
} // namespace

SC::Result SC::NamedPipeName::build(StringSpan logicalName, StringPath& outName, NamedPipeNameOptions options)
{
    SC_TRY(validateNamedPipeLogicalName(logicalName));

    StringPath nativeName;
#if SC_PLATFORM_WINDOWS
    (void)options;
    SC_TRY_MSG(nativeName.assign("\\\\.\\pipe\\"), "NamedPipeName::build failed to initialize windows prefix");
    SC_TRY_MSG(nativeName.append(logicalName), "NamedPipeName::build failed to append logicalName");
#else
    SC_TRY_MSG(options.posixDirectory.getEncoding() != StringEncoding::Utf16,
               "NamedPipeName::build posixDirectory only ASCII/UTF8");
    SC_TRY_MSG(not options.posixDirectory.isEmpty(), "NamedPipeName::build posixDirectory cannot be empty");
    const char* posixDirectoryBytes = options.posixDirectory.bytesWithoutTerminator();
    SC_TRY_MSG(posixDirectoryBytes[0] == '/', "NamedPipeName::build posixDirectory must be absolute");

    SC_TRY_MSG(nativeName.assign(options.posixDirectory), "NamedPipeName::build failed to copy posixDirectory");
    if (posixDirectoryBytes[options.posixDirectory.sizeInBytes() - 1] != '/')
    {
        SC_TRY_MSG(nativeName.append("/"), "NamedPipeName::build failed to append separator");
    }
    SC_TRY_MSG(nativeName.append(logicalName), "NamedPipeName::build failed to append logicalName");
#endif
    outName = move(nativeName);
    return Result(true);
}

//-------------------------------------------------------------------------------------------------------
// NamedPipe
//-------------------------------------------------------------------------------------------------------

#if SC_PLATFORM_WINDOWS
namespace
{
using SC::FileDescriptor;
using SC::NamedPipeServerOptions;
using SC::PipeDescriptor;
using SC::PipeOptions;
using SC::Result;
using SC::StringSpan;

static bool hasWindowsNamedPipePrefix(const wchar_t* fullName)
{
    constexpr auto dotPrefix      = L"\\\\.\\pipe\\";
    constexpr auto questionPrefix = L"\\\\?\\pipe\\";
    return ::wcsncmp(fullName, dotPrefix, 9) == 0 or ::wcsncmp(fullName, questionPrefix, 9) == 0;
}

static Result duplicateConnectedPipeHandle(HANDLE connectedHandle, PipeOptions options, PipeDescriptor& outConnection)
{
    HANDLE readHandle  = INVALID_HANDLE_VALUE;
    HANDLE writeHandle = INVALID_HANDLE_VALUE;
    if (::DuplicateHandle(::GetCurrentProcess(), connectedHandle, ::GetCurrentProcess(), &readHandle, 0,
                          options.readInheritable ? TRUE : FALSE, DUPLICATE_SAME_ACCESS) == FALSE)
    {
        return Result::Error("NamedPipe duplicate read handle failed");
    }
    if (::DuplicateHandle(::GetCurrentProcess(), connectedHandle, ::GetCurrentProcess(), &writeHandle, 0,
                          options.writeInheritable ? TRUE : FALSE, DUPLICATE_SAME_ACCESS) == FALSE)
    {
        ::CloseHandle(readHandle);
        return Result::Error("NamedPipe duplicate write handle failed");
    }
    PipeDescriptor duplicated;
    SC_TRY(duplicated.readPipe.assign(readHandle));
    SC_TRY(duplicated.writePipe.assign(writeHandle));
    outConnection = move(duplicated);
    return Result(true);
}

static Result createPendingServerInstance(StringSpan pipeName, const NamedPipeServerOptions& options,
                                          bool firstInstance, FileDescriptor& pendingConnection)
{
    const wchar_t* nullTerminatedName = pipeName.getNullTerminatedNative();

    DWORD openMode = PIPE_ACCESS_DUPLEX;
    if (not options.connectionOptions.blocking)
    {
        openMode |= FILE_FLAG_OVERLAPPED;
    }
    if (firstInstance)
    {
        openMode |= FILE_FLAG_FIRST_PIPE_INSTANCE;
    }

    DWORD maxPendingConnections = options.maxPendingConnections;
    if (maxPendingConnections == 0)
    {
        maxPendingConnections = 1;
    }
    if (maxPendingConnections > PIPE_UNLIMITED_INSTANCES)
    {
        maxPendingConnections = PIPE_UNLIMITED_INSTANCES;
    }

    const DWORD pipeMode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT;

    HANDLE handle =
        ::CreateNamedPipeW(nullTerminatedName, openMode, pipeMode, maxPendingConnections, 65536, 65536, 0, nullptr);
    SC_TRY_MSG(handle != INVALID_HANDLE_VALUE, "NamedPipeServer::create CreateNamedPipeW failed");

    return pendingConnection.assign(handle);
}
} // namespace

SC::Result SC::NamedPipeServer::create(StringSpan pipeName, NamedPipeServerOptions pipeOptions)
{
    SC_TRY_MSG(not created, "NamedPipeServer::create already created");
    SC_TRY_MSG(name.assign(pipeName), "NamedPipeServer::create invalid pipe name");

    const wchar_t* fullName = name.view().getNullTerminatedNative();
    SC_TRY_MSG(hasWindowsNamedPipePrefix(fullName),
               "NamedPipeServer::create path must start with \\\\.\\pipe\\ or \\\\?\\pipe\\");

    options       = pipeOptions;
    firstInstance = true;
    SC_TRY(createPendingServerInstance(name.view(), options, firstInstance, pendingConnection));
    firstInstance = false;
    created       = true;
    return Result(true);
}

SC::Result SC::NamedPipeServer::accept(PipeDescriptor& outConnection)
{
    SC_TRY_MSG(created and pendingConnection.isValid(), "NamedPipeServer::accept called before create");
    HANDLE pendingHandle;
    SC_TRY(pendingConnection.get(pendingHandle, Result::Error("NamedPipeServer::accept invalid pending handle")));

    if (::ConnectNamedPipe(pendingHandle, nullptr) == FALSE and ::GetLastError() != ERROR_PIPE_CONNECTED)
    {
        return Result::Error("NamedPipeServer::accept ConnectNamedPipe failed");
    }

    PipeDescriptor connected;
    SC_TRY(duplicateConnectedPipeHandle(pendingHandle, options.connectionOptions, connected));

    SC_TRY(pendingConnection.close());
    SC_TRY(createPendingServerInstance(name.view(), options, false, pendingConnection));

    outConnection = move(connected);
    return Result(true);
}

SC::Result SC::NamedPipeServer::close()
{
    if (not created)
    {
        return Result(true);
    }
    created       = false;
    firstInstance = true;
    return pendingConnection.close();
}

SC::Result SC::NamedPipeClient::connect(StringSpan pipeName, PipeDescriptor& outConnection,
                                        NamedPipeClientOptions options)
{
    StringPath nullTerminatedPath;
    SC_TRY_MSG(nullTerminatedPath.assign(pipeName), "NamedPipeClient::connect invalid pipe name");

    const wchar_t* fullName = nullTerminatedPath.view().getNullTerminatedNative();
    SC_TRY_MSG(hasWindowsNamedPipePrefix(fullName),
               "NamedPipeClient::connect path must start with \\\\.\\pipe\\ or \\\\?\\pipe\\");

    const DWORD fileFlags = options.connectionOptions.blocking ? 0 : FILE_FLAG_OVERLAPPED;

    HANDLE clientHandle =
        ::CreateFileW(fullName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, fileFlags, nullptr);
    if (clientHandle == INVALID_HANDLE_VALUE and ::GetLastError() == ERROR_PIPE_BUSY)
    {
        SC_TRY_MSG(::WaitNamedPipeW(fullName, options.windows.connectTimeoutMilliseconds) != FALSE,
                   "NamedPipeClient::connect timed out waiting for server");
        clientHandle =
            ::CreateFileW(fullName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, fileFlags, nullptr);
    }
    SC_TRY_MSG(clientHandle != INVALID_HANDLE_VALUE, "NamedPipeClient::connect CreateFileW failed");

    FileDescriptor connected;
    SC_TRY(connected.assign(clientHandle));

    HANDLE connectedHandle;
    SC_TRY(connected.get(connectedHandle, Result::Error("NamedPipeClient::connect invalid handle")));
    PipeDescriptor duplicated;
    SC_TRY(duplicateConnectedPipeHandle(connectedHandle, options.connectionOptions, duplicated));
    outConnection = move(duplicated);
    return Result(true);
}

#else

namespace
{
using SC::PipeDescriptor;
using SC::PipeOptions;
using SC::Result;

static Result setPosixDescriptorInheritable(int descriptor, bool inheritable)
{
    int flags;
    do
    {
        flags = ::fcntl(descriptor, F_GETFD);
    } while (flags == -1 and errno == EINTR);
    SC_TRY_MSG(flags != -1, "NamedPipe fcntl(F_GETFD) failed");

    const int wantedFlags = inheritable ? (flags & (~FD_CLOEXEC)) : (flags | FD_CLOEXEC);
    if (wantedFlags != flags)
    {
        int res;
        do
        {
            res = ::fcntl(descriptor, F_SETFD, wantedFlags);
        } while (res == -1 and errno == EINTR);
        SC_TRY_MSG(res == 0, "NamedPipe fcntl(F_SETFD) failed");
    }
    return Result(true);
}

static Result setPosixDescriptorBlocking(int descriptor, bool blocking)
{
    int flags;
    do
    {
        flags = ::fcntl(descriptor, F_GETFL);
    } while (flags == -1 and errno == EINTR);
    SC_TRY_MSG(flags != -1, "NamedPipe fcntl(F_GETFL) failed");

    const int wantedFlags = blocking ? (flags & (~O_NONBLOCK)) : (flags | O_NONBLOCK);
    if (wantedFlags != flags)
    {
        int res;
        do
        {
            res = ::fcntl(descriptor, F_SETFL, wantedFlags);
        } while (res == -1 and errno == EINTR);
        SC_TRY_MSG(res == 0, "NamedPipe fcntl(F_SETFL) failed");
    }
    return Result(true);
}

static Result duplicateConnectedSocket(int connectedDescriptor, PipeOptions options, PipeDescriptor& outConnection)
{
    int readDescriptor;
    do
    {
        readDescriptor = ::dup(connectedDescriptor);
    } while (readDescriptor == -1 and errno == EINTR);
    SC_TRY_MSG(readDescriptor != -1, "NamedPipe dup(read) failed");

    int writeDescriptor;
    do
    {
        writeDescriptor = ::dup(connectedDescriptor);
    } while (writeDescriptor == -1 and errno == EINTR);
    if (writeDescriptor == -1)
    {
        ::close(readDescriptor);
        return Result::Error("NamedPipe dup(write) failed");
    }

    PipeDescriptor duplicated;
    SC_TRY(duplicated.readPipe.assign(readDescriptor));
    SC_TRY(duplicated.writePipe.assign(writeDescriptor));

    int rawDescriptor = -1;
    SC_TRY(duplicated.readPipe.get(rawDescriptor, Result::Error("NamedPipe invalid read descriptor")));
    SC_TRY(setPosixDescriptorInheritable(rawDescriptor, options.readInheritable));
    SC_TRY(setPosixDescriptorBlocking(rawDescriptor, options.blocking));

    SC_TRY(duplicated.writePipe.get(rawDescriptor, Result::Error("NamedPipe invalid write descriptor")));
    SC_TRY(setPosixDescriptorInheritable(rawDescriptor, options.writeInheritable));
    SC_TRY(setPosixDescriptorBlocking(rawDescriptor, options.blocking));

    outConnection = move(duplicated);
    return Result(true);
}
} // namespace

SC::Result SC::NamedPipeServer::create(StringSpan pipeName, NamedPipeServerOptions pipeOptions)
{
    SC_TRY_MSG(not created, "NamedPipeServer::create already created");
    SC_TRY_MSG(pipeName.getEncoding() != StringEncoding::Utf16, "NamedPipeServer::create only ASCII/UTF8 paths");
    SC_TRY_MSG(name.assign(pipeName), "NamedPipeServer::create invalid pipe name");

    const char* fullPath = name.view().bytesIncludingTerminator();
    SC_TRY_MSG(fullPath[0] == '/', "NamedPipeServer::create path must be absolute");
    sockaddr_un sizeCheck;
    SC_TRY_MSG(::strlen(fullPath) < sizeof(sizeCheck.sun_path), "NamedPipeServer::create path too long");

    options = pipeOptions;
    if (options.posix.removeEndpointBeforeCreate)
    {
        int res;
        do
        {
            res = ::unlink(fullPath);
        } while (res == -1 and errno == EINTR);
        SC_TRY_MSG(res == 0 or errno == ENOENT, "NamedPipeServer::create cannot remove previous endpoint");
    }

    int listeningDescriptor;
    do
    {
        listeningDescriptor = ::socket(AF_UNIX, SOCK_STREAM, 0);
    } while (listeningDescriptor == -1 and errno == EINTR);
    SC_TRY_MSG(listeningDescriptor != -1, "NamedPipeServer::create socket failed");

    SC_TRY(listeningSocket.assign(listeningDescriptor));

    sockaddr_un address;
    ::memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    ::memcpy(address.sun_path, fullPath, ::strlen(fullPath) + 1);

    int bindResult;
    do
    {
        bindResult = ::bind(listeningDescriptor, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
    } while (bindResult == -1 and errno == EINTR);
    SC_TRY_MSG(bindResult == 0, "NamedPipeServer::create bind failed");

    int backlog = static_cast<int>(options.maxPendingConnections);
    if (backlog <= 0)
    {
        backlog = 1;
    }
    int listenResult;
    do
    {
        listenResult = ::listen(listeningDescriptor, backlog);
    } while (listenResult == -1 and errno == EINTR);
    SC_TRY_MSG(listenResult == 0, "NamedPipeServer::create listen failed");

    created = true;
    return Result(true);
}

SC::Result SC::NamedPipeServer::accept(PipeDescriptor& outConnection)
{
    SC_TRY_MSG(created and listeningSocket.isValid(), "NamedPipeServer::accept called before create");
    int listeningDescriptor;
    SC_TRY(listeningSocket.get(listeningDescriptor, Result::Error("NamedPipeServer::accept invalid listening socket")));

    int acceptedDescriptor;
    do
    {
        acceptedDescriptor = ::accept(listeningDescriptor, nullptr, nullptr);
    } while (acceptedDescriptor == -1 and errno == EINTR);
    SC_TRY_MSG(acceptedDescriptor != -1, "NamedPipeServer::accept accept failed");

    FileDescriptor acceptedSocket;
    SC_TRY(acceptedSocket.assign(acceptedDescriptor));

    int rawAcceptedDescriptor;
    SC_TRY(acceptedSocket.get(rawAcceptedDescriptor, Result::Error("NamedPipeServer::accept invalid accepted socket")));
    PipeDescriptor duplicated;
    SC_TRY(duplicateConnectedSocket(rawAcceptedDescriptor, options.connectionOptions, duplicated));
    outConnection = move(duplicated);
    return Result(true);
}

SC::Result SC::NamedPipeServer::close()
{
    if (not created)
    {
        return Result(true);
    }
    created = false;

    const Result closeResult = listeningSocket.close();

    if (options.posix.removeEndpointOnClose and not name.isEmpty())
    {
        int unlinkResult;
        do
        {
            unlinkResult = ::unlink(name.view().bytesIncludingTerminator());
        } while (unlinkResult == -1 and errno == EINTR);
        SC_TRY_MSG(unlinkResult == 0 or errno == ENOENT, "NamedPipeServer::close unlink failed");
    }
    return closeResult;
}

SC::Result SC::NamedPipeClient::connect(StringSpan pipeName, PipeDescriptor& outConnection,
                                        NamedPipeClientOptions options)
{
    SC_TRY_MSG(pipeName.getEncoding() != StringEncoding::Utf16, "NamedPipeClient::connect only ASCII/UTF8 paths");

    StringPath nullTerminatedPath;
    SC_TRY_MSG(nullTerminatedPath.assign(pipeName), "NamedPipeClient::connect invalid pipe name");
    const char* fullPath = nullTerminatedPath.view().bytesIncludingTerminator();
    SC_TRY_MSG(fullPath[0] == '/', "NamedPipeClient::connect path must be absolute");
    sockaddr_un sizeCheck;
    SC_TRY_MSG(::strlen(fullPath) < sizeof(sizeCheck.sun_path), "NamedPipeClient::connect path too long");

    int socketDescriptor;
    do
    {
        socketDescriptor = ::socket(AF_UNIX, SOCK_STREAM, 0);
    } while (socketDescriptor == -1 and errno == EINTR);
    SC_TRY_MSG(socketDescriptor != -1, "NamedPipeClient::connect socket failed");

    FileDescriptor connectedSocket;
    SC_TRY(connectedSocket.assign(socketDescriptor));

    sockaddr_un address;
    ::memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    ::memcpy(address.sun_path, fullPath, ::strlen(fullPath) + 1);

    int connectResult;
    do
    {
        connectResult = ::connect(socketDescriptor, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
    } while (connectResult == -1 and errno == EINTR);
    SC_TRY_MSG(connectResult == 0, "NamedPipeClient::connect connect failed");

    int rawConnectedDescriptor;
    SC_TRY(connectedSocket.get(rawConnectedDescriptor, Result::Error("NamedPipeClient::connect invalid socket")));

    PipeDescriptor duplicated;
    SC_TRY(duplicateConnectedSocket(rawConnectedDescriptor, options.connectionOptions, duplicated));
    outConnection = move(duplicated);
    return Result(true);
}

#endif
