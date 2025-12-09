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
#include <errno.h>    // errno
#include <fcntl.h>    // fcntl
#include <sys/stat.h> // fstat
#include <unistd.h>   // close

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
        availableData = {availableData.data(), availableData.sizeInBytes() - readData.sizeInBytes()};
    }
    actuallyRead = {data.data(), data.sizeInBytes() - availableData.sizeInBytes()};
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
        snprintf(pipeName, sizeof(pipeName), "\\\\.\\pipe\\SC-%lu-%llu", ::GetCurrentProcessId(), (intptr_t)this);

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
    int pipes[2];
    // TODO: Use pipe2 to set cloexec flags immediately
    int res;
    do
    {
        res = ::pipe(pipes);
    } while (res == -1 and errno == EINTR);

    SC_TRY_MSG(res == 0, "PipeDescriptor::createPipe - pipe failed");
    SC_TRY_MSG(readPipe.assign(pipes[0]), "Cannot assign read pipe");
    SC_TRY_MSG(writePipe.assign(pipes[1]), "Cannot assign write pipe");
    // On Posix by default descriptors are inheritable
    // https://devblogs.microsoft.com/oldnewthing/20111216-00/?p=8873
    if (options.readInheritable == false)
    {
        const Result pipeRes1 = FileDescriptor::Internal::setFileDescriptorFlags<FD_CLOEXEC>(pipes[0], true);
        SC_TRY_MSG(pipeRes1, "Cannot set close on exec on read pipe");
    }
    if (options.writeInheritable == false)
    {
        const Result pipeRes2 = FileDescriptor::Internal::setFileDescriptorFlags<FD_CLOEXEC>(pipes[1], true);
        SC_TRY_MSG(pipeRes2, "Cannot set close on exec on write pipe");
    }
    if (options.blocking == false)
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
