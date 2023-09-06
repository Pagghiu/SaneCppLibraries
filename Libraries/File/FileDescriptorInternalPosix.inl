// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../Foundation/String.h"
#include "../Foundation/StringConverter.h"
#include "../Foundation/Vector.h"
#include "FileDescriptor.h"

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
    static ReturnCode getFileFlags(int flagRead, const int fileDescriptor, int& outFlags)
    {
        do
        {
            outFlags = ::fcntl(fileDescriptor, flagRead);
        } while (outFlags == -1 and errno == EINTR);
        if (outFlags == -1)
        {
            return "fcntl getFlag failed"_a8;
        }
        return true;
    }
    static ReturnCode setFileFlags(int flagRead, int flagWrite, const int fileDescriptor, const bool setFlag,
                                   const int flag)
    {
        int oldFlags;
        do
        {
            oldFlags = ::fcntl(fileDescriptor, flagRead);
        } while (oldFlags == -1 and errno == EINTR);
        if (oldFlags == -1)
        {
            return "fcntl getFlag failed"_a8;
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
                return "fcntl setFlag failed"_a8;
            }
        }
        return true;
    }

  public:
    template <int flag>
    static ReturnCode hasFileDescriptorFlags(int fileDescriptor, bool& hasFlag)
    {
        static_assert(flag == FD_CLOEXEC, "hasFileDescriptorFlags invalid value");
        int flags = 0;
        SC_TRY_IF(getFileFlags(F_GETFD, fileDescriptor, flags));
        hasFlag = (flags & flag) != 0;
        return true;
    }
    template <int flag>
    static ReturnCode hasFileStatusFlags(int fileDescriptor, bool& hasFlag)
    {
        static_assert(flag == O_NONBLOCK, "hasFileStatusFlags invalid value");
        int flags = 0;
        SC_TRY_IF(getFileFlags(F_GETFL, fileDescriptor, flags));
        hasFlag = (flags & flag) != 0;
        return true;
    }
    template <int flag>
    static ReturnCode setFileDescriptorFlags(int fileDescriptor, bool setFlag)
    {
        // We can OR the allowed flags here to provide some safety
        static_assert(flag == FD_CLOEXEC, "setFileStatusFlags invalid value");
        return setFileFlags(F_GETFD, F_SETFD, fileDescriptor, setFlag, flag);
    }

    template <int flag>
    static ReturnCode setFileStatusFlags(int fileDescriptor, bool setFlag)
    {
        // We can OR the allowed flags here to provide some safety
        static_assert(flag == O_NONBLOCK, "setFileStatusFlags invalid value");
        return setFileFlags(F_GETFL, F_SETFL, fileDescriptor, setFlag, flag);
    }
};

// FileDescriptor

SC::ReturnCode SC::FileDescriptorTraits::releaseHandle(Handle& handle)
{
    if (::close(handle) != 0)
    {
        return "FileDescriptorTraits::releaseHandle - close failed"_a8;
    }
    return true;
}

SC::ReturnCode SC::FileDescriptor::open(StringView path, OpenMode mode, OpenOptions options)
{
    StringNative<1024> buffer = StringEncoding::Native;
    StringConverter    convert(buffer);
    StringView         filePath;
    SC_TRY_IF(convert.convertNullTerminateFastPath(path, filePath));
    if (not filePath.startsWithChar('/'))
        return "Path must be absolute"_a8;
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
    SC_TRY_MSG(fileDescriptor != -1, "open failed"_a8);
    return assign(fileDescriptor);
}

SC::ReturnCode SC::FileDescriptor::setBlocking(bool blocking)
{
    return FileDescriptorPosixHelpers::setFileStatusFlags<O_NONBLOCK>(handle, not blocking);
}

SC::ReturnCode SC::FileDescriptor::setInheritable(bool inheritable)
{
    return FileDescriptorPosixHelpers::setFileDescriptorFlags<FD_CLOEXEC>(handle, not inheritable);
}

SC::ReturnCode SC::FileDescriptor::isInheritable(bool& hasValue) const
{
    auto res = FileDescriptorPosixHelpers::hasFileDescriptorFlags<FD_CLOEXEC>(handle, hasValue);
    hasValue = not hasValue;
    return res;
}

SC::Result<SC::FileDescriptor::ReadResult> SC::FileDescriptor::readAppend(Vector<char>& output,
                                                                          Span<char>    fallbackBuffer)
{
    ssize_t                numReadBytes;
    const bool             useVector = output.capacity() > output.size();
    FileDescriptor::Handle fileDescriptor;
    SC_TRY_IF(get(fileDescriptor, "FileDescriptor::readAppend - Invalid Handle"_a8));
    if (useVector)
    {
        do
        {
            numReadBytes = ::read(fileDescriptor, output.data() + output.size(), output.capacity() - output.size());
        } while (numReadBytes == -1 && errno == EINTR); // Syscall may be interrupted and userspace must retry
    }
    else
    {
        SC_TRY_MSG(fallbackBuffer.sizeInBytes() != 0,
                   "FileDescriptor::readAppend - buffer must be bigger than zero"_a8);
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
                       "FileDescriptor::readAppend - resize failed"_a8);
        }
        else
        {
            SC_TRY_MSG(
                output.appendCopy(fallbackBuffer.data(), static_cast<size_t>(numReadBytes)),
                "FileDescriptor::readAppend - appendCopy failed. Bytes have been read from stream and will get lost"_a8);
        }
        return ReadResult{static_cast<size_t>(numReadBytes), false};
    }
    else if (numReadBytes == 0)
    {
        // EOF
        return ReadResult{0, true};
    }
    else
    {
        // TODO: Parse read result errno
        return ReturnCode("read failed"_a8);
    }
}

SC::ReturnCode SC::FileDescriptor::seek(SeekMode seekMode, uint64_t offset)
{
    int flags = 0;
    switch (seekMode)
    {
    case SeekMode::SeekStart: flags = SEEK_SET; break;
    case SeekMode::SeekEnd: flags = SEEK_END; break;
    case SeekMode::SeekCurrent: flags = SEEK_CUR; break;
    }
    const off_t res = ::lseek(handle, static_cast<off_t>(offset), flags);
    SC_TRY_MSG(res >= 0, "lseek failed"_a8);
    return static_cast<uint64_t>(res) == offset;
}

SC::ReturnCode SC::FileDescriptor::write(Span<const char> data, uint64_t offset)
{
    const ssize_t res = ::pwrite(handle, data.data(), data.sizeInBytes(), static_cast<off_t>(offset));
    SC_TRY_MSG(res >= 0, "pwrite failed"_a8);
    return static_cast<size_t>(res) == data.sizeInBytes();
}

SC::ReturnCode SC::FileDescriptor::write(Span<const char> data)
{
    const ssize_t res = ::write(handle, data.data(), data.sizeInBytes());
    SC_TRY_MSG(res >= 0, "write failed"_a8);
    return static_cast<size_t>(res) == data.sizeInBytes();
}

SC::ReturnCode SC::FileDescriptor::read(Span<char> data, Span<char>& actuallyRead, uint64_t offset)
{
    const ssize_t res = ::pread(handle, data.data(), data.sizeInBytes(), static_cast<off_t>(offset));
    SC_TRY_MSG(res >= 0, "pread failed"_a8);
    return data.sliceStartLength(0, static_cast<size_t>(res), actuallyRead);
}

SC::ReturnCode SC::FileDescriptor::read(Span<char> data, Span<char>& actuallyRead)
{
    const ssize_t res = ::read(handle, data.data(), data.sizeInBytes());
    SC_TRY_MSG(res >= 0, "read failed"_a8);
    return data.sliceStartLength(0, static_cast<size_t>(res), actuallyRead);
}

// PipeDescriptor

SC::ReturnCode SC::PipeDescriptor::createPipe(InheritableReadFlag readFlag, InheritableWriteFlag writeFlag)
{
    int pipes[2];
    // TODO: Use pipe2 to set cloexec flags immediately
    if (::pipe(pipes) != 0)
    {
        return ReturnCode("PipeDescriptor::createPipe - pipe failed"_a8);
    }
    SC_TRY_MSG(readPipe.assign(pipes[0]), "Cannot assign read pipe"_a8);
    SC_TRY_MSG(writePipe.assign(pipes[1]), "Cannot assign write pipe"_a8);
    // On Posix by default descriptors are inheritable
    // https://devblogs.microsoft.com/oldnewthing/20111216-00/?p=8873
    if (readFlag == ReadNonInheritable)
    {
        SC_TRY_MSG(readPipe.setInheritable(false), "Cannot set close on exec on read pipe"_a8);
    }
    if (writeFlag == WriteNonInheritable)
    {
        SC_TRY_MSG(writePipe.setInheritable(false), "Cannot set close on exec on write pipe"_a8);
    }
    return true;
}
