// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../../Foundation/Containers/Vector.h"
#include "../../Foundation/Strings/SmallString.h"
#include "../../Foundation/Strings/StringConverter.h"
#include "../FileDescriptor.h"

#include <errno.h>  // errno
#include <fcntl.h>  // fcntl
#include <unistd.h> // close

namespace SC
{
struct FileDescriptorPosixHelpers;
}
struct SC::FileDescriptorPosixHelpers
{
  private:
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

  public:
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

// FileDescriptor

SC::Result SC::FileDescriptorTraits::releaseHandle(Handle& handle)
{
    if (::close(handle) != 0)
    {
        return Result::Error("FileDescriptorTraits::releaseHandle - close failed");
    }
    return Result(true);
}

SC::Result SC::FileDescriptor::open(StringView path, OpenMode mode, OpenOptions options)
{
    StringNative<1024> buffer = StringEncoding::Native;
    StringConverter    convert(buffer);
    StringView         filePath;
    SC_TRY(convert.convertNullTerminateFastPath(path, filePath));
    if (not filePath.startsWithChar('/'))
        return Result::Error("Path must be absolute");
    int flags = 0;
    switch (mode)
    {
    case ReadOnly: flags |= O_RDONLY; break;
    case WriteCreateTruncate: flags |= O_WRONLY | O_CREAT | O_TRUNC; break;
    case WriteAppend: flags |= O_WRONLY | O_APPEND; break;
    case ReadAndWrite: flags |= O_RDWR; break;
    }

    if (not options.inheritable)
    {
        flags |= O_CLOEXEC;
    }
    const int access         = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    const int fileDescriptor = ::open(filePath.getNullTerminatedNative(), flags, access);
    SC_TRY_MSG(fileDescriptor != -1, "open failed");
    return assign(fileDescriptor);
}

SC::Result SC::FileDescriptor::setBlocking(bool blocking)
{
    return FileDescriptorPosixHelpers::setFileStatusFlags<O_NONBLOCK>(handle, not blocking);
}

SC::Result SC::FileDescriptor::setInheritable(bool inheritable)
{
    return FileDescriptorPosixHelpers::setFileDescriptorFlags<FD_CLOEXEC>(handle, not inheritable);
}

SC::Result SC::FileDescriptor::isInheritable(bool& hasValue) const
{
    auto res = FileDescriptorPosixHelpers::hasFileDescriptorFlags<FD_CLOEXEC>(handle, hasValue);
    hasValue = not hasValue;
    return res;
}

SC::Result SC::FileDescriptor::readAppend(Vector<char>& output, Span<char> fallbackBuffer, ReadResult& result)
{
    ssize_t                numReadBytes;
    const bool             useVector = output.capacity() > output.size();
    FileDescriptor::Handle fileDescriptor;
    SC_TRY(get(fileDescriptor, Result::Error("FileDescriptor::readAppend - Invalid Handle")));
    if (useVector)
    {
        do
        {
            numReadBytes = ::read(fileDescriptor, output.data() + output.size(), output.capacity() - output.size());
        } while (numReadBytes == -1 && errno == EINTR); // Syscall may be interrupted and userspace must retry
    }
    else
    {
        SC_TRY_MSG(fallbackBuffer.sizeInBytes() != 0, "FileDescriptor::readAppend - buffer must be bigger than zero");
        do
        {
            numReadBytes = ::read(fileDescriptor, fallbackBuffer.data(), fallbackBuffer.sizeInBytes());
        } while (numReadBytes == -1 && errno == EINTR); // Syscall may be interrupted and userspace must retry
    }
    if (numReadBytes > 0)
    {
        if (useVector)
        {
            SC_TRY_MSG(output.resizeWithoutInitializing(output.size() + static_cast<size_t>(numReadBytes)),
                       "FileDescriptor::readAppend - resize failed");
        }
        else
        {
            SC_TRY_MSG(
                output.append({fallbackBuffer.data(), static_cast<size_t>(numReadBytes)}),
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

SC::Result SC::FileDescriptor::write(Span<const char> data, uint64_t offset)
{
    const ssize_t res = ::pwrite(handle, data.data(), data.sizeInBytes(), static_cast<off_t>(offset));
    SC_TRY_MSG(res >= 0, "pwrite failed");
    return Result(static_cast<size_t>(res) == data.sizeInBytes());
}

SC::Result SC::FileDescriptor::write(Span<const char> data)
{
    const ssize_t res = ::write(handle, data.data(), data.sizeInBytes());
    SC_TRY_MSG(res >= 0, "write failed");
    return Result(static_cast<size_t>(res) == data.sizeInBytes());
}

SC::Result SC::FileDescriptor::read(Span<char> data, Span<char>& actuallyRead, uint64_t offset)
{
    const ssize_t res = ::pread(handle, data.data(), data.sizeInBytes(), static_cast<off_t>(offset));
    SC_TRY_MSG(res >= 0, "pread failed");
    return Result(data.sliceStartLength(0, static_cast<size_t>(res), actuallyRead));
}

SC::Result SC::FileDescriptor::read(Span<char> data, Span<char>& actuallyRead)
{
    const ssize_t res = ::read(handle, data.data(), data.sizeInBytes());
    SC_TRY_MSG(res >= 0, "read failed");
    return Result(data.sliceStartLength(0, static_cast<size_t>(res), actuallyRead));
}

// PipeDescriptor

SC::Result SC::PipeDescriptor::createPipe(InheritableReadFlag readFlag, InheritableWriteFlag writeFlag)
{
    int pipes[2];
    // TODO: Use pipe2 to set cloexec flags immediately
    if (::pipe(pipes) != 0)
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
