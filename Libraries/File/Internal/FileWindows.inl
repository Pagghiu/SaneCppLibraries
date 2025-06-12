// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Strings/String.h"
#include "../../Strings/StringConverter.h"
#include "../File.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

//-------------------------------------------------------------------------------------------------------
// FileDescriptorDefinition
//-------------------------------------------------------------------------------------------------------
SC::Result SC::detail::FileDescriptorDefinition::releaseHandle(Handle& handle)
{
    BOOL res;
    __try
    {
        res = ::CloseHandle(handle);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        res = FALSE;
    }
    if (res == FALSE)
    {
        return Result::Error("FileDescriptorDefinition::releaseHandle - CloseHandle failed");
    }
    return Result(true);
}

//-------------------------------------------------------------------------------------------------------
// FileDescriptor
//-------------------------------------------------------------------------------------------------------
SC::Result SC::FileDescriptor::setBlocking(bool blocking)
{
    // TODO: IMPLEMENT
    SC_COMPILER_UNUSED(blocking);
    return Result(false);
}

SC::Result SC::FileDescriptor::setInheritable(bool inheritable)
{
    if (::SetHandleInformation(handle, HANDLE_FLAG_INHERIT, inheritable ? TRUE : FALSE) == FALSE)
    {
        return Result::Error("FileDescriptor::setInheritable - ::SetHandleInformation failed");
    }
    return Result(true);
}

SC::Result SC::FileDescriptor::isInheritable(bool& hasValue) const
{
    DWORD dwFlags = 0;
    if (::GetHandleInformation(handle, &dwFlags) == FALSE)
    {
        return Result::Error("FileDescriptor::getInheritable = ::GetHandleInformation failed");
    }
    hasValue = (dwFlags & HANDLE_FLAG_INHERIT) != 0;
    return Result(true);
}

SC::Result SC::FileDescriptor::seek(SeekMode seekMode, uint64_t offset)
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
    SC_TRY_MSG(newPos != INVALID_SET_FILE_POINTER, "SetFilePointer failed");
    return Result(static_cast<uint64_t>(newPos) == offset);
}

SC::Result SC::FileDescriptor::currentPosition(size_t& position) const
{
    LARGE_INTEGER li, source;
    memset(&source, 0, sizeof(source));
    if (::SetFilePointerEx(handle, source, &li, FILE_CURRENT) != 0)
    {
        position = static_cast<size_t>(li.QuadPart);
        return Result(true);
    }
    return Result::Error("SetFilePointerEx failed");
}

SC::Result SC::FileDescriptor::sizeInBytes(size_t& sizeInBytes) const
{
    LARGE_INTEGER li;
    if (GetFileSizeEx(handle, &li) != 0)
    {
        sizeInBytes = static_cast<size_t>(li.QuadPart);
        return Result(true);
    }
    return Result::Error("GetFileSizeEx failed");
}

SC::Result SC::FileDescriptor::write(Span<const char> data, uint64_t offset)
{
    SC_TRY(seek(SeekStart, offset));
    return write(data);
}

SC::Result SC::FileDescriptor::write(Span<const char> data)
{
    DWORD      numberOfWrittenBytes;
    const BOOL res =
        ::WriteFile(handle, data.data(), static_cast<DWORD>(data.sizeInBytes()), &numberOfWrittenBytes, nullptr);
    SC_TRY_MSG(res, "WriteFile failed");
    return Result(static_cast<size_t>(numberOfWrittenBytes) == data.sizeInBytes());
}

SC::Result SC::FileDescriptor::read(Span<char> data, Span<char>& actuallyRead, uint64_t offset)
{
    SC_TRY(seek(SeekStart, offset));
    return read(data, actuallyRead);
}

SC::Result SC::FileDescriptor::read(Span<char> data, Span<char>& actuallyRead)
{
    DWORD      numberOfReadBytes;
    const BOOL res =
        ::ReadFile(handle, data.data(), static_cast<DWORD>(data.sizeInBytes()), &numberOfReadBytes, nullptr);
    SC_TRY_MSG(res, "ReadFile failed");
    return Result(data.sliceStartLength(0, static_cast<size_t>(numberOfReadBytes), actuallyRead));
}

SC::Result SC::FileDescriptor::openNativeEncoding(StringViewData filePath, FileOpen mode)
{
    if (filePath.getEncoding() != StringEncoding::Utf16)
    {
        return Result::Error("FileDescriptor::openNativeEncoding: Windows supports only UTF16 encoding");
    }
    DWORD accessMode        = 0;
    DWORD createDisposition = 0;
    DWORD fileFlags         = mode.blocking ? 0 : FILE_FLAG_OVERLAPPED;

    switch (mode.mode)
    {
    case FileOpen::Read:
        accessMode |= FILE_GENERIC_READ;
        createDisposition = OPEN_EXISTING;
        break;
    case FileOpen::Write:
        accessMode |= FILE_GENERIC_WRITE;
        createDisposition = CREATE_ALWAYS;
        break;
    case FileOpen::Append:
        accessMode |= FILE_APPEND_DATA;
        createDisposition = OPEN_ALWAYS;
        break;
    case FileOpen::ReadWrite:
        accessMode |= FILE_GENERIC_READ | FILE_GENERIC_WRITE;
        createDisposition = OPEN_ALWAYS;
        break;
    case FileOpen::WriteRead:
        accessMode |= FILE_GENERIC_READ | FILE_GENERIC_WRITE;
        createDisposition = CREATE_ALWAYS;
        break;
    case FileOpen::AppendRead:
        accessMode |= FILE_GENERIC_READ | FILE_APPEND_DATA;
        createDisposition = OPEN_ALWAYS;
        break;
    }

    if (mode.sync)
    {
        fileFlags |= FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING;
    }

    if (mode.exclusive)
    {
        createDisposition = CREATE_NEW;
    }

    DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;

    SECURITY_ATTRIBUTES security;
    security.nLength              = sizeof(SECURITY_ATTRIBUTES);
    security.bInheritHandle       = mode.inheritable ? TRUE : FALSE;
    security.lpSecurityDescriptor = nullptr;
    HANDLE fileDescriptor         = CreateFileW(filePath.getNullTerminatedNative(), accessMode, shareMode, &security,
                                                createDisposition, fileFlags, nullptr);

    DWORD lastErr = ::GetLastError();
    SC_COMPILER_UNUSED(lastErr);
    SC_TRY_MSG(fileDescriptor != INVALID_HANDLE_VALUE, "CreateFileW failed");
    return assign(fileDescriptor);
}

//-------------------------------------------------------------------------------------------------------
// File
//-------------------------------------------------------------------------------------------------------
SC::Result SC::File::open(StringView path, FileOpen mode)
{
    StringNative<1024> buffer = StringEncoding::Native;
    StringConverter    convert(buffer);
    StringView         filePath;
    SC_TRY(convert.convertNullTerminateFastPath(path, filePath));
    const wchar_t* widePath     = filePath.getNullTerminatedNative();
    const bool     isThreeChars = filePath.sizeInBytes() >= 3 * sizeof(native_char_t);
    if (not isThreeChars or (widePath[0] != L'\\' and widePath[1] != L':' and filePath != L"NUL"))
    {
        return Result::Error("Path must be absolute");
    }

    return fd.openNativeEncoding(filePath, mode);
}

struct SC::File::Internal
{
    template <typename T, typename U>
    [[nodiscard]] static Result readAppend(FileDescriptor::Handle fileDescriptor, U& output, Span<T> fallbackBuffer,
                                           ReadResult& result)
    {
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
                       "FileDescriptor::readAppend - buffer must be bigger than zero");
            success = ReadFile(fileDescriptor, fallbackBuffer.data(), static_cast<DWORD>(fallbackBuffer.sizeInBytes()),
                               &numReadBytes, nullptr);
        }
        if (Internal::isActualError(success, numReadBytes, fileDescriptor))
        {
            // TODO: Parse read result ERROR
            return Result::Error("FileDescriptor::readAppend ReadFile failed");
        }
        else if (numReadBytes > 0)
        {
            if (useVector)
            {
                SC_TRY_MSG(output.resizeWithoutInitializing(output.size() + numReadBytes),
                           "FileDescriptor::readAppend - resize failed");
            }
            else
            {
                SC_TRY_MSG(
                    output.append({fallbackBuffer.data(), static_cast<size_t>(numReadBytes)}),
                    "FileDescriptor::readAppend - append failed. Bytes have been read from stream and will get lost");
            }
            result = ReadResult{static_cast<size_t>(numReadBytes), false};
            return Result(true);
        }
        else
        {
            // EOF
            result = ReadResult{0, true};
            return Result(true);
        }
    }

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

//-------------------------------------------------------------------------------------------------------
// PipeDescriptor
//-------------------------------------------------------------------------------------------------------
SC::Result SC::PipeDescriptor::createPipe(InheritableReadFlag readFlag, InheritableWriteFlag writeFlag)
{
    // On Windows to inherit flags they must be flagged as inheritable
    // https://devblogs.microsoft.com/oldnewthing/20111216-00/?p=8873
    SECURITY_ATTRIBUTES security;
    memset(&security, 0, sizeof(security));
    security.nLength              = sizeof(security);
    security.bInheritHandle       = readFlag == ReadInheritable or writeFlag == WriteInheritable ? TRUE : FALSE;
    security.lpSecurityDescriptor = nullptr;

    HANDLE pipeRead  = INVALID_HANDLE_VALUE;
    HANDLE pipeWrite = INVALID_HANDLE_VALUE;

    if (CreatePipe(&pipeRead, &pipeWrite, &security, 0) == FALSE)
    {
        return Result::Error("PipeDescriptor::createPipe - ::CreatePipe failed");
    }
    SC_TRY(readPipe.assign(pipeRead));
    SC_TRY(writePipe.assign(pipeWrite));

    if (security.bInheritHandle)
    {
        if (readFlag == ReadNonInheritable)
        {
            SC_TRY_MSG(readPipe.setInheritable(false), "Cannot set read pipe inheritable");
        }
        if (writeFlag == WriteNonInheritable)
        {
            SC_TRY_MSG(writePipe.setInheritable(false), "Cannot set write pipe inheritable");
        }
    }
    return Result(true);
}
