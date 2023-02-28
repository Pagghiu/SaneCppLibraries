// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "FileDescriptor.h"
#include "FileDescriptorInternalWindows.h"

#include <Windows.h>

SC::ReturnCode SC::FileNativeDescriptorClose(const FileNativeDescriptor& fileDescriptor)
{
    if (::CloseHandle(fileDescriptor) == FALSE)
    {
        return "FileNativeDescriptorClose - CloseHandle failed"_a8;
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
    SC_TRY_IF(fileNativeHandle.get().get(fileDescriptor, "FileDescriptor::readAppend - Invalid Handle"_a8));
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
    SC_TRY_IF(readPipe.fileNativeHandle.get().assign(pipeRead));
    SC_TRY_IF(writePipe.fileNativeHandle.get().assign(pipeWrite));
    return true;
}

SC::ReturnCode SC::FileDescriptorWindows::disableInherit()
{
    FileNativeDescriptor nativeFd;
    SC_TRY_IF(
        fileDescriptor.fileNativeHandle.get().get(nativeFd, "FileDescriptorPipe::createPipe - Invalid Handle"_a8));
    if (!SetHandleInformation(nativeFd, HANDLE_FLAG_INHERIT, 0))
    {
        return "FileDescriptorPipe::createPipe - ::SetHandleInformation failed"_a8;
    }
    return true;
}

SC::ReturnCode SC::FileDescriptorPosix::setCloseOnExec() { return true; }
