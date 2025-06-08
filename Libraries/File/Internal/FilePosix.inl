// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Strings/String.h"
#include "../../Strings/StringConverter.h"
#include "../File.h"

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

//-------------------------------------------------------------------------------------------------------
// FileDescriptor
//-------------------------------------------------------------------------------------------------------
struct SC::FileDescriptor::Internal
{
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

SC::Result SC::FileDescriptor::openNativeEncoding(StringViewData filePath, FileOpen mode)
{
    if (filePath.getEncoding() == StringEncoding::Utf16)
    {
        return Result::Error("FileDescriptor::openNativeEncoding: POSIX supports only UTF8 and ASCII encoding");
    }
    const int flags  = mode.toPosixFlags();
    const int access = mode.toPosixAccess();

    const int fileDescriptor = ::open(filePath.getNullTerminatedNative(), flags, access);
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

//-------------------------------------------------------------------------------------------------------
// File
//-------------------------------------------------------------------------------------------------------
SC::Result SC::File::open(StringView path, FileOpen mode)
{
    StringNative<1024> buffer = StringEncoding::Native;
    StringConverter    convert(buffer);
    StringView         filePath;
    SC_TRY(convert.convertNullTerminateFastPath(path, filePath));
    if (not filePath.startsWithAnyOf({'/'}))
        return Result::Error("Path must be absolute");
    return fd.openNativeEncoding(filePath, mode);
}

struct SC::File::Internal
{
    template <typename T, typename U>
    [[nodiscard]] static Result readAppend(FileDescriptor::Handle fileDescriptor, U& output, Span<T> fallbackBuffer,
                                           ReadResult& result)
    {
        ssize_t    numReadBytes;
        const bool useVector = output.capacity() > output.size();
        if (useVector)
        {
            do
            {
                const size_t bytesToRead = (output.capacity() - output.size()) * sizeof(T);
                numReadBytes             = ::read(fileDescriptor, output.data() + output.size(), bytesToRead);
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
            if (useVector)
            {
                SC_TRY_MSG(
                    output.resizeWithoutInitializing(output.size() + static_cast<size_t>(numReadBytes) / sizeof(T)),
                    "FileDescriptor::readAppend - resize failed");
            }
            else
            {
                SC_TRY_MSG(
                    output.append({fallbackBuffer.data(), static_cast<size_t>(numReadBytes) / sizeof(T)}),
                    "FileDescriptor::readAppend - append failed. Bytes have been read from stream and will get lost");
            }
            result = ReadResult{static_cast<size_t>(numReadBytes), false};
            return Result(true);
        }
        else if (numReadBytes == 0)
        {
            // EOF
            result = ReadResult{0, true};
            return Result(true);
        }
        else
        {
            // TODO: Parse read result errno
            return Result::Error("read failed");
        }
    }
};

//-------------------------------------------------------------------------------------------------------
// PipeDescriptor
//-------------------------------------------------------------------------------------------------------
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
