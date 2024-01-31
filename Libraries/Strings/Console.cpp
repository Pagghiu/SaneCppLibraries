// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "Console.h"
#include "../Foundation/Limits.h"
#include "../Foundation/Platform.h"
#include "../Strings/StringConverter.h"

#include <stdio.h>  // stdout
#include <string.h> // strlen

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

SC::Console::Console(Vector<char>& encodingConversionBuffer) : encodingConversionBuffer(encodingConversionBuffer)
{
#if SC_PLATFORM_WINDOWS
    handle     = ::GetStdHandle(STD_OUTPUT_HANDLE);
    isConsole  = ::GetFileType(handle) == FILE_TYPE_CHAR;
    isDebugger = ::IsDebuggerPresent() == TRUE;
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
    StringView encodedPath;
    if (!isConsole)
    {
        if (str.getEncoding() == StringEncoding::Utf16)
        {
            encodingConversionBuffer.clearWithoutInitializing();
            if (StringConverter::convertEncodingToUTF8(str, encodingConversionBuffer, &encodedPath))
            {
                ::WriteFile(handle, encodedPath.bytesWithoutTerminator(), static_cast<DWORD>(encodedPath.sizeInBytes()),
                            nullptr, nullptr);
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
                    encodingConversionBuffer.clearWithoutInitializing();
                    if (StringConverter::convertEncodingToUTF16(str, encodingConversionBuffer, &encodedPath))
                    {
                        ::OutputDebugStringW(encodedPath.getNullTerminatedNative());
                    }
                    else
                    {
                        if (isConsole)
                        {
                            ::WriteConsoleW(handle, L"ERROR: cannot format string",
                                            static_cast<DWORD>(wcslen(L"ERROR: cannot format string")), nullptr,
                                            nullptr);
                        }
                    }
                }
            }
#endif
        }
        else
        {
            encodingConversionBuffer.clearWithoutInitializing();
            if (StringConverter::convertEncodingToUTF16(str, encodingConversionBuffer, &encodedPath))
            {
                if (isConsole)
                {
                    ::WriteConsoleW(handle, encodedPath.getNullTerminatedNative(),
                                    static_cast<DWORD>(encodedPath.sizeInBytes() / sizeof(wchar_t)), nullptr, nullptr);
                }

#if SC_CONFIGURATION_DEBUG
                if (isDebugger)
                {
                    ::OutputDebugStringW(encodedPath.getNullTerminatedNative());
                }
#endif
            }
            else
            {
                if (isConsole)
                {
                    ::WriteConsoleW(handle, L"ERROR: cannot format string",
                                    static_cast<DWORD>(wcslen(L"ERROR: cannot format string")), nullptr, nullptr);
                }
            }
        }
    }

#else
    fwrite(str.bytesWithoutTerminator(), sizeof(char), str.sizeInBytes(), stdout);
#endif
}
