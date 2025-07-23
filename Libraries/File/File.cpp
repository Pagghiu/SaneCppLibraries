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
            SC_TRY_MSG(buffer.tryGrowTo(bufferData.sizeInBytes + static_cast<size_t>(numReadBytes)),
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
SC::Result SC::FileDescriptor::setBlocking(bool blocking)
{
    // TODO: IMPLEMENT
    SC_COMPILER_UNUSED(blocking);
    return Result(false);
}

SC::Result SC::FileDescriptor::setInheritable(bool inheritable)
{
    if (::SetHandleInformation(handle, HANDLE_FLAG_INHERIT, inheritable ? TRUE : FALSE) == FALSE)
    {
        return Result::Error("FileDescriptor::setInheritable - ::SetHandleInformation failed");
    }
    return Result(true);
}

SC::Result SC::FileDescriptor::isInheritable(bool& hasValue) const
{
    DWORD dwFlags = 0;
    if (::GetHandleInformation(handle, &dwFlags) == FALSE)
    {
        return Result::Error("FileDescriptor::getInheritable = ::GetHandleInformation failed");
    }
    hasValue = (dwFlags & HANDLE_FLAG_INHERIT) != 0;
    return Result(true);
}

SC::Result SC::FileDescriptor::seek(SeekMode seekMode, uint64_t offset)
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
    return Result(static_cast<uint64_t>(newPos) == offset);
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
    DWORD      numberOfReadBytes;
    const BOOL res =
        ::ReadFile(handle, data.data(), static_cast<DWORD>(data.sizeInBytes()), &numberOfReadBytes, nullptr);
    SC_TRY_MSG(res, "ReadFile failed");
    return Result(data.sliceStartLength(0, static_cast<size_t>(numberOfReadBytes), actuallyRead));
}

SC::Result SC::FileDescriptor::open(StringSpan filePath, FileOpen mode)
{
    StringPath nullTerminated;
    SC_TRY_MSG(nullTerminated.path.assign(filePath), "FileDescriptor::open - Path too long or invalid encoding");
    auto& nullTerminatedPath = nullTerminated.path.buffer;

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

    DWORD lastErr = ::GetLastError();
    SC_COMPILER_UNUSED(lastErr);
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
            SC_TRY_MSG(buffer.tryGrowTo(bufferData.sizeInBytes + static_cast<size_t>(numReadBytes)),
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
    static Result getFileFlags(int flagRead, const int fileDescriptor, int& outFlags)
    {
        do
        {
            outFlags = ::fcntl(fileDescriptor, flagRead);
        } while (outFlags == -1 and errno == EINTR);
        if (outFlags == -1)
        {
            return Result::Error("fcntl getFlag failed");
        }
        return Result(true);
    }

    static Result setFileFlags(int flagRead, int flagWrite, const int fileDescriptor, const bool setFlag,
                               const int flag)
    {
        int oldFlags;
        do
        {
            oldFlags = ::fcntl(fileDescriptor, flagRead);
        } while (oldFlags == -1 and errno == EINTR);
        if (oldFlags == -1)
        {
            return Result::Error("fcntl getFlag failed");
        }
        const int newFlags = setFlag ? oldFlags | flag : oldFlags & (~flag);
        if (newFlags != oldFlags)
        {
            int res;
            do
            {
                res = ::fcntl(fileDescriptor, flagWrite, newFlags);
            } while (res == -1 and errno == EINTR);
            if (res != 0)
            {
                return Result::Error("fcntl setFlag failed");
            }
        }
        return Result(true);
    }

    template <int flag>
    static Result hasFileDescriptorFlags(int fileDescriptor, bool& hasFlag)
    {
        static_assert(flag == FD_CLOEXEC, "hasFileDescriptorFlags invalid value");
        int flags = 0;
        SC_TRY(getFileFlags(F_GETFD, fileDescriptor, flags));
        hasFlag = (flags & flag) != 0;
        return Result(true);
    }

    template <int flag>
    static Result hasFileStatusFlags(int fileDescriptor, bool& hasFlag)
    {
        static_assert(flag == O_NONBLOCK, "hasFileStatusFlags invalid value");
        int flags = 0;
        SC_TRY(getFileFlags(F_GETFL, fileDescriptor, flags));
        hasFlag = (flags & flag) != 0;
        return Result(true);
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
    if (filePath.getEncoding() == StringEncoding::Utf16)
    {
        return Result::Error("FileDescriptor::open: POSIX supports only UTF8 and ASCII encoding");
    }
    const int flags  = mode.toPosixFlags();
    const int access = mode.toPosixAccess();

    StringPath nullTerminated;
    SC_TRY_MSG(nullTerminated.path.assign(filePath), "FileDescriptor::open - Path too long or invalid encoding");
    auto& nullTerminatedPath = nullTerminated.path.buffer;
    if (nullTerminatedPath[0] != '/')
        return Result::Error("FileDescriptor::open - Path must be absolute");
    const int fileDescriptor = ::open(nullTerminatedPath, flags, access);
    SC_TRY_MSG(fileDescriptor != -1, "open failed");
    SC_TRY(assign(fileDescriptor));
    if (not mode.blocking)
    {
        SC_TRY(setBlocking(false));
    }
    return Result(true);
}

SC::Result SC::FileDescriptor::setBlocking(bool blocking)
{
    return Internal::setFileStatusFlags<O_NONBLOCK>(handle, not blocking);
}

SC::Result SC::FileDescriptor::setInheritable(bool inheritable)
{
    return Internal::setFileDescriptorFlags<FD_CLOEXEC>(handle, not inheritable);
}

SC::Result SC::FileDescriptor::isInheritable(bool& hasValue) const
{
    auto res = Internal::hasFileDescriptorFlags<FD_CLOEXEC>(handle, hasValue);
    hasValue = not hasValue;
    return res;
}

SC::Result SC::FileDescriptor::seek(SeekMode seekMode, uint64_t offset)
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
    return Result(static_cast<uint64_t>(res) == offset);
}

SC::Result SC::FileDescriptor::currentPosition(size_t& position) const
{
    const off_t fileSize = ::lseek(handle, 0, SEEK_CUR);
    if (fileSize >= 0)
    {
        position = static_cast<size_t>(fileSize);
        return Result(true);
    }
    return Result::Error("lseek failed");
}

SC::Result SC::FileDescriptor::sizeInBytes(size_t& sizeInBytes) const
{
    struct stat fileStat;
    if (::fstat(handle, &fileStat) == 0)
    {
        sizeInBytes = static_cast<size_t>(fileStat.st_size);
        return Result(true);
    }
    return Result::Error("fstat failed");
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

SC::Result SC::FileDescriptor::readUntilEOF(IGrowableBuffer&& adapter)
{
    char buffer[1024];
    SC_TRY_MSG(isValid(), "FileDescriptor::readUntilEOFGrowable - Invalid handle");
    bool isEOF = false;
    SC_TRY_MSG(adapter.tryGrowTo(0), "FileDescriptor::readUntilEOFGrowable - Cannot reset string");
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
SC::Result SC::PipeDescriptor::createPipe(InheritableReadFlag readFlag, InheritableWriteFlag writeFlag)
{
    // On Windows to inherit flags they must be flagged as inheritable
    // https://devblogs.microsoft.com/oldnewthing/20111216-00/?p=8873
    SECURITY_ATTRIBUTES security;
    memset(&security, 0, sizeof(security));
    security.nLength              = sizeof(security);
    security.bInheritHandle       = readFlag == ReadInheritable or writeFlag == WriteInheritable ? TRUE : FALSE;
    security.lpSecurityDescriptor = nullptr;

    HANDLE pipeRead  = INVALID_HANDLE_VALUE;
    HANDLE pipeWrite = INVALID_HANDLE_VALUE;

    if (CreatePipe(&pipeRead, &pipeWrite, &security, 0) == FALSE)
    {
        return Result::Error("PipeDescriptor::createPipe - ::CreatePipe failed");
    }
    SC_TRY(readPipe.assign(pipeRead));
    SC_TRY(writePipe.assign(pipeWrite));

    if (security.bInheritHandle)
    {
        if (readFlag == ReadNonInheritable)
        {
            SC_TRY_MSG(readPipe.setInheritable(false), "Cannot set read pipe inheritable");
        }
        if (writeFlag == WriteNonInheritable)
        {
            SC_TRY_MSG(writePipe.setInheritable(false), "Cannot set write pipe inheritable");
        }
    }
    return Result(true);
}

#else
SC::Result SC::PipeDescriptor::createPipe(InheritableReadFlag readFlag, InheritableWriteFlag writeFlag)
{
    int pipes[2];
    // TODO: Use pipe2 to set cloexec flags immediately
    int res;
    do
    {
        res = ::pipe(pipes);
    } while (res == -1 and errno == EINTR);

    if (res != 0)
    {
        return Result::Error("PipeDescriptor::createPipe - pipe failed");
    }
    SC_TRY_MSG(readPipe.assign(pipes[0]), "Cannot assign read pipe");
    SC_TRY_MSG(writePipe.assign(pipes[1]), "Cannot assign write pipe");
    // On Posix by default descriptors are inheritable
    // https://devblogs.microsoft.com/oldnewthing/20111216-00/?p=8873
    if (readFlag == ReadNonInheritable)
    {
        SC_TRY_MSG(readPipe.setInheritable(false), "Cannot set close on exec on read pipe");
    }
    if (writeFlag == WriteNonInheritable)
    {
        SC_TRY_MSG(writePipe.setInheritable(false), "Cannot set close on exec on write pipe");
    }
    return Result(true);
}
#endif

SC::Result SC::PipeDescriptor::close()
{
    SC_TRY(readPipe.close());
    return writePipe.close();
}
