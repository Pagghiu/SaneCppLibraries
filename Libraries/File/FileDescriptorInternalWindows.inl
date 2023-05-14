// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "../Foundation/String.h"
#include "../Foundation/StringConverter.h"
#include "../Foundation/Vector.h"
#include "FileDescriptor.h"

// FileDescriptor

SC::ReturnCode SC::FileDescriptorTraits::releaseHandle(Handle& handle)
{
    if (::CloseHandle(handle) == FALSE)
    {
        return "FileDescriptorTraits::releaseHandle - CloseHandle failed"_a8;
    }
    return true;
}

SC::ReturnCode SC::FileDescriptor::open(StringView path, OpenMode mode, OpenOptions options)
{
    StringNative<1024> buffer = StringEncoding::Native;
    StringConverter    convert(buffer);
    StringView         filePath;
    SC_TRY_IF(convert.convertNullTerminateFastPath(path, filePath));
    const wchar_t* wpath        = filePath.getNullTerminatedNative();
    const bool     isThreeChars = filePath.sizeInBytes() >= 3 * sizeof(utf_char_t);
    if (not isThreeChars or (wpath[0] != L'\\' and wpath[1] != L':'))
    {
        return "Path must be absolute"_a8;
    }
    DWORD accessMode        = 0;
    DWORD createDisposition = 0;
    switch (mode)
    {
    case ReadOnly:
        accessMode |= FILE_GENERIC_READ;
        createDisposition |= OPEN_EXISTING;
        break;
    case WriteCreateTruncate:
        accessMode |= FILE_GENERIC_WRITE;
        createDisposition |= CREATE_ALWAYS;
        break;
    case WriteAppend:
        accessMode |= FILE_GENERIC_WRITE;
        createDisposition |= CREATE_NEW;
        break;
    case ReadAndWrite: accessMode |= FILE_GENERIC_READ | FILE_GENERIC_WRITE; break;
    }
    DWORD               shareMode  = FILE_SHARE_READ | FILE_SHARE_WRITE;
    DWORD               attributes = options.async ? FILE_FLAG_OVERLAPPED : 0;
    SECURITY_ATTRIBUTES security;
    security.nLength              = sizeof(SECURITY_ATTRIBUTES);
    security.bInheritHandle       = options.inheritable ? TRUE : FALSE;
    security.lpSecurityDescriptor = nullptr;
    HANDLE fileDescriptor         = CreateFileW(filePath.getNullTerminatedNative(), accessMode, shareMode, &security,
                                                createDisposition, attributes, nullptr);
    DWORD  lastErr                = ::GetLastError();
    SC_UNUSED(lastErr);
    SC_TRY_MSG(fileDescriptor != INVALID_HANDLE_VALUE, "CreateFileW failed"_a8);
    return assign(fileDescriptor);
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

SC::ReturnCode SC::FileDescriptor::seek(SeekMode seekMode, uint64_t offset)
{
    int flags = 0;
    switch (seekMode)
    {
    case SeekMode::SeekStart: flags = FILE_BEGIN; break;
    case SeekMode::SeekEnd: flags = FILE_END; break;
    case SeekMode::SeekCurrent: flags = FILE_CURRENT; break;
    }
    const DWORD offsetLow  = static_cast<DWORD>(offset & 0xffffffff);
    DWORD       offsetHigh = static_cast<DWORD>((offset >> 32) & 0xffffffff);
    const DWORD newPos     = ::SetFilePointer(handle, offsetLow, (LONG*)&offsetHigh, flags);
    SC_TRY_MSG(newPos != INVALID_SET_FILE_POINTER, "SetFilePointer failed"_a8);
    return static_cast<uint64_t>(newPos) == offset;
}

SC::ReturnCode SC::FileDescriptor::write(Span<const char> data, uint64_t offset)
{
    SC_TRY_IF(seek(SeekStart, offset));
    return write(data);
}

SC::ReturnCode SC::FileDescriptor::write(Span<const char> data)
{
    DWORD      numberOfWrittenBytes;
    const BOOL res =
        ::WriteFile(handle, data.data(), static_cast<DWORD>(data.sizeInBytes()), &numberOfWrittenBytes, nullptr);
    SC_TRY_MSG(res, "WriteFile failed"_a8);
    return static_cast<size_t>(numberOfWrittenBytes) == data.sizeInBytes();
}

SC::ReturnCode SC::FileDescriptor::read(Span<char> data, Span<char>& actuallyRead, uint64_t offset)
{
    SC_TRY_IF(seek(SeekStart, offset));
    return read(data, actuallyRead, offset);
}

SC::ReturnCode SC::FileDescriptor::read(Span<char> data, Span<char>& actuallyRead)
{
    DWORD      numberOfReadBytes;
    const BOOL res =
        ::ReadFile(handle, data.data(), static_cast<DWORD>(data.sizeInBytes()), &numberOfReadBytes, nullptr);
    SC_TRY_MSG(res, "ReadFile failed"_a8);
    actuallyRead = data;
    actuallyRead.setSizeInBytes(static_cast<size_t>(numberOfReadBytes));
    return true;
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
