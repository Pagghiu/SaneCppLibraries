// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "FileDescriptor.h"

#if SC_PLATFORM_WINDOWS

#include <Windows.h>

#else

#include <fcntl.h>  // fcntl
#include <stdio.h>  // stdout, stdin
#include <unistd.h> // close

#endif

#if SC_PLATFORM_WINDOWS
SC::ReturnCode SC::FileNativeDescriptorCloseWindows(const FileNativeDescriptor& fileDescriptor)
{
    if (::CloseHandle(fileDescriptor) == FALSE)
    {
        return "FileNativeDescriptorCloseWindows - CloseHandle failed"_a8;
    }
    return true;
}
SC::Result<SC::FileDescriptor::ReadResult> SC::FileDescriptor::readAppend(Vector<char>& output,
                                                                          Span<char>    fallbackBuffer)
{
    DWORD                numReadBytes = 0xffffffff;
    bool                 gotError     = true;
    const bool           useVector    = output.capacity() > output.size();
    FileNativeDescriptor fileDescriptor;
    SC_TRY_IF(get(fileDescriptor, "FileDescriptor::readAppend - Invalid Handle"_a8));
    if (useVector)
    {
        BOOL success = ReadFile(fileDescriptor, output.data() + output.size(),
                                static_cast<DWORD>(output.capacity() - output.size()), &numReadBytes, nullptr);
        gotError     = success == FALSE;
    }
    else
    {
        SC_TRY_MSG(fallbackBuffer.sizeInBytes() != 0,
                   "FileDescriptor::readAppend - buffer must be bigger than zero"_a8);
        BOOL success = ReadFile(fileDescriptor, fallbackBuffer.data(), static_cast<DWORD>(fallbackBuffer.sizeInBytes()),
                                &numReadBytes, nullptr);

        gotError = success == FALSE;
        if (gotError && numReadBytes == 0 && GetFileType(fileDescriptor) == FILE_TYPE_PIPE &&
            GetLastError() == ERROR_BROKEN_PIPE)
        {
            // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile
            // Pipes

            // If an anonymous pipe is being used and the write handle has been closed, when ReadFile attempts to read
            // using the pipe's corresponding read handle, the function returns FALSE and GetLastError returns
            // ERROR_BROKEN_PIPE.
            gotError = false;
        }
    }
    if (gotError)
    {
        // TODO: Parse read result ERROR
        return ReturnCode("FileDescriptor::readAppend ReadFile failed"_a8);
    }
    else if (numReadBytes > 0)
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
    else
    {
        // EOF
        return ReadResult{0, true};
    }
}

SC::ReturnCode SC::FileDescriptorWindows::disableInherit()
{
    FileNativeDescriptor nativeFd;
    SC_TRY_IF(fileDescriptor.get(nativeFd, "FileDescriptorPipe::createPipe - Invalid Handle"_a8));
    if (!SetHandleInformation(nativeFd, HANDLE_FLAG_INHERIT, 0))
    {
        return "FileDescriptorPipe::createPipe - ::SetHandleInformation failed"_a8;
    }
    return true;
}
SC::ReturnCode SC::FileDescriptorPosix::setCloseOnExec() { return true; }

SC::ReturnCode SC::FileDescriptorPipe::createPipe()
{
    SECURITY_ATTRIBUTES security  = {0};
    security.nLength              = sizeof(security);
    security.bInheritHandle       = TRUE;
    security.lpSecurityDescriptor = nullptr;
    HANDLE pipeRead               = INVALID_HANDLE_VALUE;
    HANDLE pipeWrite              = INVALID_HANDLE_VALUE;

    if (!CreatePipe(&pipeRead, &pipeWrite, &security, 0))
    {
        return "FileDescriptorPipe::createPipe - ::CreatePipe failed"_a8;
    }
    SC_TRY_IF(readPipe.assign(pipeRead));
    SC_TRY_IF(writePipe.assign(pipeWrite));
    return true;
}
#else

SC::Result<SC::FileDescriptor::ReadResult> SC::FileDescriptor::readAppend(Vector<char>& output,
                                                                          Span<char>    fallbackBuffer)
{
    ssize_t              numReadBytes;
    const bool           useVector = output.capacity() > output.size();
    FileNativeDescriptor fileDescriptor;
    SC_TRY_IF(get(fileDescriptor, "FileDescriptor::readAppend - Invalid Handle"_a8));
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

int SC::FileDescriptorPosix::getStandardInputFDS() { return fileno(stdin); };
int SC::FileDescriptorPosix::getStandardOutputFDS() { return fileno(stdout); };
int SC::FileDescriptorPosix::getStandardErrorFDS() { return fileno(stderr); };

SC::ReturnCode SC::FileNativeDescriptorClosePosix(const FileNativeDescriptor& fileDescriptor)
{
    if (::close(fileDescriptor) != 0)
    {
        return "FileNativeDescriptorClosePosix - close failed"_a8;
    }
    return true;
}

SC::ReturnCode SC::FileDescriptorWindows::disableInherit() { return true; }
SC::ReturnCode SC::FileDescriptorPosix::setCloseOnExec()
{
    FileNativeDescriptor nativeFd;
    SC_TRY_IF(fileDescriptor.get(nativeFd, "FileDescriptor::setCloseOnExec - Invalid Handle"_a8));
    if (::fcntl(nativeFd, F_SETFD, FD_CLOEXEC) != 0)
    {
        return "FileDescriptor::setCloseOnExec - fcntl failed"_a8;
    }
    return true;
}

SC::ReturnCode SC::FileDescriptorPosix::redirect(int fds)
{
    FileNativeDescriptor nativeFd;
    SC_TRY_IF(fileDescriptor.get(nativeFd, "FileDescriptor::redirect - Invalid Handle"_a8));
    if (::dup2(nativeFd, fds) == -1)
    {
        return ReturnCode("dup2 failed"_a8);
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
    SC_TRY_MSG(readPipe.assign(pipes[0]), "Cannot assign read pipe"_a8);
    SC_TRY_MSG(writePipe.assign(pipes[1]), "Cannot assign write pipe"_a8);
    return true;
}

#endif