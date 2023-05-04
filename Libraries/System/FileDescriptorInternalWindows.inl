// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "../Foundation/Vector.h"
#include "FileDescriptor.h"
#include "System.h"

// FileDescriptor

SC::ReturnCode SC::FileDescriptorTraits::releaseHandle(Handle& handle)
{
    if (::CloseHandle(handle) == FALSE)
    {
        return "FileDescriptorTraits::releaseHandle - CloseHandle failed"_a8;
    }
    return true;
}

SC::ReturnCode SC::FileDescriptor::setBlocking(bool blocking)
{
    // TODO: IMPLEMENT
    SC_UNUSED(blocking);
    return false;
}

SC::ReturnCode SC::FileDescriptor::setInheritable(bool inheritable)
{
    if (::SetHandleInformation(handle, HANDLE_FLAG_INHERIT, inheritable ? TRUE : FALSE) == FALSE)
    {
        return "FileDescriptor::setInheritable - ::SetHandleInformation failed"_a8;
    }
    return true;
}

SC::ReturnCode SC::FileDescriptor::isInheritable(bool& hasValue) const
{
    DWORD dwFlags = 0;
    if (::GetHandleInformation(handle, &dwFlags) == FALSE)
    {
        return "FileDescriptor::getInheritable = ::GetHandleInformation failed"_a8;
    }
    hasValue = (dwFlags & HANDLE_FLAG_INHERIT) != 0;
    return true;
}

struct SC::FileDescriptor::Internal
{
    [[nodiscard]] static bool isActualError(BOOL success, DWORD numReadBytes, FileDescriptor::Handle fileDescriptor)
    {
        if (success == FALSE && numReadBytes == 0 && GetFileType(fileDescriptor) == FILE_TYPE_PIPE &&
            GetLastError() == ERROR_BROKEN_PIPE)
        {
            // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile
            // Pipes

            // If an anonymous pipe is being used and the write handle has been closed, when ReadFile attempts to read
            // using the pipe's corresponding read handle, the function returns FALSE and GetLastError returns
            // ERROR_BROKEN_PIPE.
            return false;
        }
        return success == FALSE;
    }
};

SC::Result<SC::FileDescriptor::ReadResult> SC::FileDescriptor::readAppend(Vector<char>& output,
                                                                          Span<char>    fallbackBuffer)
{
    FileDescriptor::Handle fileDescriptor;
    SC_TRY_IF(get(fileDescriptor, "FileDescriptor::readAppend - Invalid Handle"_a8));

    const bool useVector = output.capacity() > output.size();

    DWORD numReadBytes = 0xffffffff;
    BOOL  success;
    if (useVector)
    {
        success = ReadFile(fileDescriptor, output.data() + output.size(),
                           static_cast<DWORD>(output.capacity() - output.size()), &numReadBytes, nullptr);
    }
    else
    {
        SC_TRY_MSG(fallbackBuffer.sizeInBytes() != 0,
                   "FileDescriptor::readAppend - buffer must be bigger than zero"_a8);
        success = ReadFile(fileDescriptor, fallbackBuffer.data(), static_cast<DWORD>(fallbackBuffer.sizeInBytes()),
                           &numReadBytes, nullptr);
    }
    if (Internal::isActualError(success, numReadBytes, fileDescriptor))
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

// PipeDescriptor

SC::ReturnCode SC::PipeDescriptor::createPipe(InheritableReadFlag readFlag, InheritableWriteFlag writeFlag)
{
    // On Windows to inherit flags they must be flagged as inheritable
    // https://devblogs.microsoft.com/oldnewthing/20111216-00/?p=8873
    SECURITY_ATTRIBUTES security;
    memset(&security, 0, sizeof(security));
    security.nLength              = sizeof(security);
    security.bInheritHandle       = readFlag == ReadInheritable or writeFlag == WriteInheritable ? TRUE : FALSE;
    security.lpSecurityDescriptor = nullptr;
    HANDLE pipeRead               = INVALID_HANDLE_VALUE;
    HANDLE pipeWrite              = INVALID_HANDLE_VALUE;

    if (CreatePipe(&pipeRead, &pipeWrite, &security, 0) == FALSE)
    {
        return "PipeDescriptor::createPipe - ::CreatePipe failed"_a8;
    }
    SC_TRY_IF(readPipe.assign(pipeRead));
    SC_TRY_IF(writePipe.assign(pipeWrite));

    if (security.bInheritHandle)
    {
        if (readFlag == ReadNonInheritable)
        {
            SC_TRY_MSG(readPipe.setInheritable(false), "Cannot set read pipe inheritable"_a8);
        }
        if (writeFlag == WriteNonInheritable)
        {
            SC_TRY_MSG(writePipe.setInheritable(false), "Cannot set write pipe inheritable"_a8);
        }
    }
    return true;
}
