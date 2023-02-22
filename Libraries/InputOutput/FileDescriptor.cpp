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
SC::FileDescriptor::FileDescriptor(FileDescriptor&& other)
{
    fileDescriptor = other.fileDescriptor;
    other.resetAsInvalid();
}
SC::FileDescriptor& SC::FileDescriptor::operator=(FileDescriptor&& other)
{
    SC_RELEASE_ASSERT(close());
    fileDescriptor = other.fileDescriptor;
    other.resetAsInvalid();
    return *this;
}
SC::FileDescriptor::~FileDescriptor() { (void)close(); }

bool SC::FileDescriptor::assign(FileNativeDescriptor newFileDescriptor)
{
    SC_TRY_IF(close());
    fileDescriptor = newFileDescriptor;
    return true;
}

#if SC_PLATFORM_WINDOWS

SC::Result<SC::FileDescriptor::ReadResult> SC::FileDescriptor::readAppend(Vector<char>& output,
                                                                          Span<char>    fallbackBuffer)
{
    DWORD      numReadBytes = 0xffffffff;
    bool       gotError     = true;
    const bool useVector    = output.capacity() > output.size();
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

bool SC::FileDescriptor::close()
{
    if (isValid())
    {
        BOOL res = CloseHandle(fileDescriptor);
        resetAsInvalid();
        return res == TRUE;
    }
    return true;
}

SC::ReturnCode SC::FileDescriptor::disableInherit()
{
    if (isValid())
    {
        if (!SetHandleInformation(fileDescriptor, HANDLE_FLAG_INHERIT, 0))
        {
            return "FileDescriptorPipe::createPipe - ::SetHandleInformation failed"_a8;
        }
        return true;
    }
    return "FileDescriptorPipe::createPipe - Invalid Handle"_a8;
}

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
    ssize_t    numReadBytes;
    const bool useVector = output.capacity() > output.size();
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

int SC::FileDescriptor::getStandardInputFDS() { return fileno(stdin); };
int SC::FileDescriptor::getStandardOutputFDS() { return fileno(stdout); };
int SC::FileDescriptor::getStandardErrorFDS() { return fileno(stderr); };

bool SC::FileDescriptor::close()
{
    if (fileDescriptor != InvalidFDS)
    {
        int res = ::close(fileDescriptor);
        resetAsInvalid();
        return res == 0;
    }
    return true;
}

bool SC::FileDescriptor::setCloseOnExec()
{
    if (fileDescriptor != InvalidFDS)
    {
        int res = ::fcntl(fileDescriptor, F_SETFD, FD_CLOEXEC);
        return res == 0;
    }
    return false;
}

SC::ReturnCode SC::FileDescriptor::redirect(int fds)
{
    SC_TRY_MSG(isValid(), "Not inited"_a8);
    if (::dup2(fileDescriptor, fds) == -1)
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
    SC_TRY_MSG(writePipe.assign(pipes[1]), "Cannot assign read pipe"_a8);
    return true;
}

#endif
