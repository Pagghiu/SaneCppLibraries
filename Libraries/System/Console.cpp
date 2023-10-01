// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.

#include "Console.h"
#include "../Foundation/Base/Limits.h"
#include "../Foundation/Base/Platform.h"
#include "../Foundation/Strings/StringConverter.h"

#include <stdio.h>  // stdout
#include <string.h> // strlen

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

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
    if (str.getEncoding() == StringEncoding::Ascii)
    {
        WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), str.bytesWithoutTerminator(),
                      static_cast<DWORD>(str.sizeInBytes()), nullptr, nullptr);
#if SC_CONFIGURATION_DEBUG
        if (str.isNullTerminated())
        {
            OutputDebugStringA(str.bytesIncludingTerminator());
        }
        else
        {
            encodingConversionBuffer.clearWithoutInitializing();
            if (StringConverter::convertEncodingToUTF16(str, encodingConversionBuffer, &encodedPath))
            {
                OutputDebugStringW(encodedPath.getNullTerminatedNative());
            }
            else
            {
                WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), L"ERROR: cannot format string",
                              static_cast<DWORD>(wcslen(L"ERROR: cannot format string")), nullptr, nullptr);
            }
        }
#endif
    }
    else
    {
        encodingConversionBuffer.clearWithoutInitializing();
        if (StringConverter::convertEncodingToUTF16(str, encodingConversionBuffer, &encodedPath))
        {
            WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), encodedPath.getNullTerminatedNative(),
                          static_cast<DWORD>(encodedPath.sizeInBytes() / sizeof(wchar_t)), nullptr, nullptr);
#if SC_CONFIGURATION_DEBUG
            OutputDebugStringW(encodedPath.getNullTerminatedNative());
#endif
        }
        else
        {
            WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), L"ERROR: cannot format string",
                          static_cast<DWORD>(wcslen(L"ERROR: cannot format string")), nullptr, nullptr);
        }
    }
#else
    fwrite(str.bytesWithoutTerminator(), sizeof(char), str.sizeInBytes(), stdout);
#endif
}
