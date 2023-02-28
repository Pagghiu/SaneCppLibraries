// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "FileDescriptor.h"
#include "FileDescriptorInternalPosix.h"

#include <fcntl.h>  // fcntl
#include <stdio.h>  // stdout, stdin
#include <unistd.h> // close

SC::Result<SC::FileDescriptor::ReadResult> SC::FileDescriptor::readAppend(Vector<char>& output,
                                                                          Span<char>    fallbackBuffer)
{
    ssize_t              numReadBytes;
    const bool           useVector = output.capacity() > output.size();
    FileNativeDescriptor fileDescriptor;
    SC_TRY_IF(fileNativeHandle.get().get(fileDescriptor, "FileDescriptor::readAppend - Invalid Handle"_a8));
    if (useVector)
    {
        numReadBytes = read(fileDescriptor, output.data() + output.size(), output.capacity() - output.size());
    }
    else
    {
        SC_TRY_MSG(fallbackBuffer.sizeInBytes() != 0,
                   "FileDescriptor::readAppend - buffer must be bigger than zero"_a8);
        numReadBytes = read(fileDescriptor, fallbackBuffer.data(), fallbackBuffer.sizeInBytes());
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

SC::ReturnCode SC::FileNativeDescriptorClose(const FileNativeDescriptor& fileDescriptor)
{
    if (::close(fileDescriptor) != 0)
    {
        return "FileNativeDescriptorClose - close failed"_a8;
    }
    return true;
}

SC::ReturnCode SC::FileDescriptorPipe::createPipe()
{
    int pipes[2];
    if (::pipe(pipes) != 0)
    {
        return ReturnCode("pipe failed"_a8);
    }
    SC_TRY_MSG(readPipe.fileNativeHandle.get().assign(pipes[0]), "Cannot assign read pipe"_a8);
    SC_TRY_MSG(writePipe.fileNativeHandle.get().assign(pipes[1]), "Cannot assign write pipe"_a8);
    return true;
}

SC::ReturnCode SC::FileDescriptorWindows::disableInherit() { return true; }
SC::ReturnCode SC::FileDescriptorPosix::setCloseOnExec()
{
    FileNativeDescriptor nativeFd;
    SC_TRY_IF(
        fileDescriptor.fileNativeHandle.get().get(nativeFd, "FileDescriptor::setCloseOnExec - Invalid Handle"_a8));
    if (::fcntl(nativeFd, F_SETFD, FD_CLOEXEC) != 0)
    {
        return "FileDescriptor::setCloseOnExec - fcntl failed"_a8;
    }
    return true;
}

SC::ReturnCode SC::FileDescriptorPosix::redirect(int fds)
{
    FileNativeDescriptor nativeFd;
    SC_TRY_IF(fileDescriptor.fileNativeHandle.get().get(nativeFd, "FileDescriptor::redirect - Invalid Handle"_a8));
    if (::dup2(nativeFd, fds) == -1)
    {
        return ReturnCode("dup2 failed"_a8);
    }
    return true;
}
