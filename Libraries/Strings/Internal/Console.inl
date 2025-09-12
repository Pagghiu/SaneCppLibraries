// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../Foundation/Platform.h"
#include "../../Strings/StringConverter.h"
#include "../Console.h"

#include <stdio.h>  // stdout
#include <string.h> // strlen

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

SC::Console::Console(Span<char> conversionBuffer) : conversionBuffer(conversionBuffer)
{
    // Minimum size for conversion buffer (two wide chars + null terminator)
    SC_ASSERT_RELEASE(conversionBuffer.sizeInBytes() >= 6);
#if SC_PLATFORM_WINDOWS
    handle     = ::GetStdHandle(STD_OUTPUT_HANDLE);
    isConsole  = ::GetFileType(handle) == FILE_TYPE_CHAR;
    isDebugger = ::IsDebuggerPresent() == TRUE;
#endif
}

bool SC::Console::tryAttachingToParentConsole()
{
#if SC_PLATFORM_WINDOWS
    return ::AttachConsole(ATTACH_PARENT_PROCESS) == TRUE;
#else
    return true;
#endif
}

bool SC::Console::isAttachedToConsole()
{
#if SC_PLATFORM_WINDOWS
    return ::GetConsoleWindow() != 0;
#else
    return true;
#endif
}

void SC::Console::printLine(const StringView str)
{
    print(str);
    print("\n"_a8);
}

void SC::Console::print(const StringView str)
{
    if (str.isEmpty())
        return;
#if SC_PLATFORM_WINDOWS
    if (!isConsole)
    {
        if (str.getEncoding() == StringEncoding::Utf16)
        {
            // copy chunks of wide chars from str to conversion buffer, convert to utf8, looping until all are written
            const size_t totalNumWChars   = str.sizeInBytes() / sizeof(wchar_t);
            size_t       numWrittenWChars = 0;
            const size_t maxInputWChars   = conversionBuffer.sizeInBytes() / 2;
            while (numWrittenWChars < totalNumWChars)
            {
                const size_t remainingWChars    = totalNumWChars - numWrittenWChars;
                size_t       numToConvertWChars = remainingWChars < maxInputWChars ? remainingWChars : maxInputWChars;

                int numChars = 0;
                // reduce number of wide chars to convert until conversion succeeds
                while (numToConvertWChars > 0)
                {
                    numChars = ::WideCharToMultiByte(
                        CP_UTF8, 0, reinterpret_cast<const wchar_t*>(str.bytesWithoutTerminator()) + numWrittenWChars,
                        static_cast<int>(numToConvertWChars), reinterpret_cast<char*>(conversionBuffer.data()),
                        static_cast<int>(conversionBuffer.sizeInBytes() - 1), nullptr, 0);
                    if (numChars > 0)
                    {
                        break;
                    }
                    numToConvertWChars--;
                }
                if (numChars > 0)
                {
                    conversionBuffer.data()[numChars] = 0; // null terminator
                    ::WriteFile(handle, conversionBuffer.data(), static_cast<DWORD>(numChars), nullptr, nullptr);
                }
                numWrittenWChars += numToConvertWChars;
            }
        }
        else
        {
            ::WriteFile(handle, str.bytesWithoutTerminator(), static_cast<DWORD>(str.sizeInBytes()), nullptr, nullptr);
        }
    }
    if (isConsole or isDebugger)
    {
        if (str.getEncoding() == StringEncoding::Ascii)
        {
            if (isConsole)
            {
                ::WriteConsoleA(handle, str.bytesWithoutTerminator(), static_cast<DWORD>(str.sizeInBytes()), nullptr,
                                nullptr);
            }
#if SC_CONFIGURATION_DEBUG
            if (isDebugger)
            {
                if (str.isNullTerminated())
                {
                    ::OutputDebugStringA(str.bytesIncludingTerminator());
                }
                else
                {
                    // copy chunks of bytes from str to conversion buffer looping until all of them are written
                    size_t numWritten = 0;
                    while (numWritten < str.sizeInBytes())
                    {
                        const size_t numToWrite =
                            (str.sizeInBytes() - numWritten) < (conversionBuffer.sizeInBytes() - 1)
                                ? (str.sizeInBytes() - numWritten)
                                : (conversionBuffer.sizeInBytes() - 1);
                        ::memcpy(conversionBuffer.data(), str.bytesWithoutTerminator() + numWritten, numToWrite);
                        conversionBuffer[numToWrite] = 0; // null terminator
                        ::OutputDebugStringA(conversionBuffer.data());
                        numWritten += numToWrite;
                    }
                }
            }
#endif
        }
        else if (str.getEncoding() == StringEncoding::Utf16)
        {
            if (isConsole)
            {
                ::WriteConsoleW(handle, reinterpret_cast<const wchar_t*>(str.bytesWithoutTerminator()),
                                static_cast<DWORD>(str.sizeInBytes() / sizeof(wchar_t)), nullptr, nullptr);
            }

#if SC_CONFIGURATION_DEBUG
            if (isDebugger)
            {
                if (str.isNullTerminated())
                {
                    ::OutputDebugStringW(str.getNullTerminatedNative());
                }
                else
                {
                    // copy chunks of bytes from str to conversion buffer looping until all of them are written
                    wchar_t*     buffer                = reinterpret_cast<wchar_t*>(conversionBuffer.data());
                    const size_t bufferSizeInWChars    = conversionBuffer.sizeInBytes() / sizeof(wchar_t);
                    size_t       numWrittenWChars      = 0;
                    const size_t totalNumToWriteWChars = str.sizeInBytes() / sizeof(wchar_t);
                    while (numWrittenWChars < totalNumToWriteWChars)
                    {
                        // copy just a chunk of bytes to buffer and null terminate it, updating numWrittenWChars
                        const size_t remainingWChars = totalNumToWriteWChars - numWrittenWChars;
                        const size_t numToWriteWChars =
                            remainingWChars < (bufferSizeInWChars - 1) ? remainingWChars : bufferSizeInWChars - 1;
                        ::memcpy(buffer,
                                 reinterpret_cast<const wchar_t*>(str.bytesWithoutTerminator()) + numWrittenWChars,
                                 numToWriteWChars * sizeof(wchar_t));
                        buffer[numToWriteWChars] = 0; // null terminator
                        numWrittenWChars += numToWriteWChars;
                        ::OutputDebugStringW(buffer);
                    }
                }
            }
#endif
        }
        else if (str.getEncoding() == StringEncoding::Utf8)
        {
            // convert to wide char and use OutputDebugStringW / WriteConsoleW
            wchar_t*     buffer       = reinterpret_cast<wchar_t*>(conversionBuffer.data());
            const size_t bufferWChars = conversionBuffer.sizeInBytes() / sizeof(wchar_t);
#if SC_CONFIGURATION_DEBUG
            size_t numWrittenBytes = 0;
            while (numWrittenBytes < str.sizeInBytes())
            {
                // convert just a chunk of bytes to wide char buffer, ensuring not to cut UTF8 characters, and null
                // terminate it convert the maximum number of bytes that can fit in the wide char buffer
                const size_t remainingBytes = str.sizeInBytes() - numWrittenBytes;
                size_t numToWrite = remainingBytes < (bufferWChars - 1) * 2 ? remainingBytes : (bufferWChars - 1) * 2;
                // Find the largest numToWrite that forms complete UTF8 characters
                int numWideChars = 0;
                while (numToWrite > 0)
                {
                    numWideChars = ::MultiByteToWideChar(
                        CP_UTF8, 0, str.bytesWithoutTerminator() + numWrittenBytes, static_cast<int>(numToWrite),
                        buffer,
                        static_cast<int>(bufferWChars - 1)); // -1 to leave space for null terminator
                    if (numWideChars > 0)
                    {
                        break;
                    }
                    numToWrite--;
                }
                if (numWideChars > 0)
                {
                    buffer[numWideChars] = 0;
                    if (isDebugger)
                    {
                        ::OutputDebugStringW(buffer);
                    }
                    if (isConsole)
                    {
                        ::WriteConsoleW(handle, buffer, static_cast<DWORD>(numWideChars), nullptr, nullptr);
                    }
                }
                numWrittenBytes += numToWrite;
            }
#endif
        }
    }

#else
    fwrite(str.bytesWithoutTerminator(), sizeof(char), str.sizeInBytes(), stdout);
#endif
}
