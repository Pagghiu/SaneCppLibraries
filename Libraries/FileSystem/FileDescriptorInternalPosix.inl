// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "FileDescriptor.h"

#include <errno.h>
#include <fcntl.h>     // fcntl
#include <stdio.h>     // stdout, stdin
#include <sys/ioctl.h> // ioctl
#include <unistd.h>    // close

struct SC::FileDescriptor::Internal
{
  private:
    enum TypeFlag
    {
        TypeStatusFlag,
        TypeDescriptorFlag
    };
    static ReturnCode setFileFlags(int flagRead, int flagWrite, const FileDescriptorNative fileDescriptor,
                                   const bool setFlag, const int flag)
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
    static ReturnCode setFileDescriptorFlags(FileDescriptorNative fileDescriptor, bool setFlag)
    {
        // We can OR the allowed flags here to provide some safety
        static_assert(flag == FD_CLOEXEC, "setFileStatusFlags invalid value");
        return setFileFlags(F_GETFD, F_SETFD, fileDescriptor, setFlag, flag);
    }

    template <int flag>
    static ReturnCode setFileStatusFlags(FileDescriptorNative fileDescriptor, bool setFlag)
    {
        // We can OR the allowed flags here to provide some safety
        static_assert(flag == O_NONBLOCK, "setFileStatusFlags invalid value");
        return setFileFlags(F_GETFL, F_SETFL, fileDescriptor, setFlag, flag);
    }
};

SC::Result<SC::FileDescriptor::ReadResult> SC::FileDescriptor::readAppend(Vector<char>& output,
                                                                          Span<char>    fallbackBuffer)
{
    ssize_t              numReadBytes;
    const bool           useVector = output.capacity() > output.size();
    FileDescriptorNative fileDescriptor;
    SC_TRY_IF(handle.get(fileDescriptor, "FileDescriptor::readAppend - Invalid Handle"_a8));
    if (useVector)
    {
        do
        {
            numReadBytes = read(fileDescriptor, output.data() + output.size(), output.capacity() - output.size());
        } while (numReadBytes == -1 && errno == EINTR); // Syscall may be interrupted and userspace must retry
    }
    else
    {
        SC_TRY_MSG(fallbackBuffer.sizeInBytes() != 0,
                   "FileDescriptor::readAppend - buffer must be bigger than zero"_a8);
        do
        {
            numReadBytes = read(fileDescriptor, fallbackBuffer.data(), fallbackBuffer.sizeInBytes());
        } while (numReadBytes == -1 && errno == EINTR); // Syscall may be interrupted and userspace must retry
    }
    if (numReadBytes > 0)
    {
        if (useVector)
        {
            SC_TRY_MSG(output.resizeWithoutInitializing(output.size() + numReadBytes),
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

SC::ReturnCode SC::FileDescriptor::setBlocking(bool blocking)
{
    FileDescriptorNative fileDescriptor;
    SC_TRY_IF(handle.get(fileDescriptor, "FileDescriptor::setBlocking - Invalid Handle"_a8));
    return Internal::setFileStatusFlags<O_NONBLOCK>(fileDescriptor, not blocking);
}

SC::ReturnCode SC::FileDescriptor::setInheritable(bool inheritable)
{
    FileDescriptorNative fileDescriptor;
    SC_TRY_IF(handle.get(fileDescriptor, "FileDescriptor::setInheritable - Invalid Handle"_a8));
    return Internal::setFileDescriptorFlags<FD_CLOEXEC>(fileDescriptor, not inheritable);
}

SC::ReturnCode SC::FileDescriptorNativeClose(FileDescriptorNative& fileDescriptor)
{
    if (::close(fileDescriptor) != 0)
    {
        return "FileDescriptorNativeClose - close failed"_a8;
    }
    return true;
}

SC::ReturnCode SC::FileDescriptorPipe::createPipe(InheritableReadFlag readFlag, InheritableWriteFlag writeFlag)
{
    int pipes[2];
    if (::pipe(pipes) != 0)
    {
        return ReturnCode("FileDescriptorPipe::createPipe - pipe failed"_a8);
    }
    SC_TRY_MSG(readPipe.handle.assign(pipes[0]), "Cannot assign read pipe"_a8);
    SC_TRY_MSG(writePipe.handle.assign(pipes[1]), "Cannot assign write pipe"_a8);
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

SC::ReturnCode SC::FileDescriptorPosix::duplicateAndReplace(int fds)
{
    FileDescriptorNative nativeFd;
    SC_TRY_IF(fileDescriptor.handle.get(nativeFd, "FileDescriptor::duplicateAndReplace - Invalid Handle"_a8));
    if (::dup2(nativeFd, fds) == -1)
    {
        return ReturnCode("FileDescriptorPosix::duplicateAndReplace - dup2 failed"_a8);
    }
    return true;
}
