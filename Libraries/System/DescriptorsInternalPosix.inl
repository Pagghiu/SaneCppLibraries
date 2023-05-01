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

// SocketDescriptorTraits

SC::ReturnCode SC::SocketDescriptorTraits::releaseHandle(Handle& handle)
{
    ::close(handle);
    handle = Invalid;
    return true;
}
SC::ReturnCode SC::SocketDescriptor::setInheritable(bool inheritable)
{
    return FileDescriptorPosixHelpers::setFileDescriptorFlags<FD_CLOEXEC>(handle, not inheritable);
}

SC::ReturnCode SC::SocketDescriptor::setBlocking(bool blocking)
{
    return FileDescriptorPosixHelpers::setFileStatusFlags<O_NONBLOCK>(handle, not blocking);
}

SC::ReturnCode SC::SocketDescriptor::isInheritable(bool& hasValue) const
{
    auto res = FileDescriptorPosixHelpers::hasFileDescriptorFlags<FD_CLOEXEC>(handle, hasValue);
    hasValue = not hasValue;
    return res;
}
SC::ReturnCode SC::SocketDescriptor::create(IPType ipType, Protocol protocol, BlockingType blocking,
                                            InheritableType inheritable)
{
    SC_TRY_IF(SystemFunctions::isNetworkingInited());
    SC_TRUST_RESULT(close());
    int type = AF_UNSPEC;
    switch (ipType)
    {
    case IPTypeV4: type = AF_INET; break;
    case IPTypeV6: type = AF_INET6; break;
    }

    int proto = IPPROTO_TCP;

    switch (protocol)
    {
    case ProtocolTcp: proto = IPPROTO_TCP; break;
    }

    int flags = SOCK_STREAM;
#if defined(SOCK_NONBLOCK)
    if (blocking == NonBlocking)
    {
        flags |= SOCK_NONBLOCK;
    }
#endif // defined(SOCK_NONBLOCK)
#if defined(SOCK_CLOEXEC)
    if (inheritable == NonInheritable)
    {
        flags |= SOCK_CLOEXEC;
    }
#endif // defined(SOCK_CLOEXEC)
    do
    {
        handle = ::socket(type, flags, proto);
    } while (handle == -1 and errno == EINTR);
#if !defined(SOCK_CLOEXEC)
    SC_TRY_IF(setInheritable(inheritable == Inheritable));
#endif // !defined(SOCK_CLOEXEC)
#if !defined(SOCK_NONBLOCK)
    SC_TRY_IF(setBlocking(blocking == Blocking));
#endif // !defined(SOCK_NONBLOCK)

#if defined(SO_NOSIGPIPE)
    {
        int active = 1;
        setsockopt(handle, SOL_SOCKET, SO_NOSIGPIPE, &active, sizeof(active));
    }
#endif // defined(SO_NOSIGPIPE)
    return isValid();
}

// ProcessDescriptor

SC::ReturnCode SC::ProcessDescriptorTraits::releaseHandle(pid_t& handle)
{
    handle = Invalid;
    return true;
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
