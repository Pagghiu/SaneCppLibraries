// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../Foundation/Vector.h"
#include "Descriptors.h"
#include "System.h"

#include <errno.h>  // errno
#include <fcntl.h>  // fcntl
#include <netdb.h>  // AF_INET / IPPROTO_TCP / AF_UNSPEC
#include <unistd.h> // close
constexpr int SOCKET_ERROR = -1;

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
