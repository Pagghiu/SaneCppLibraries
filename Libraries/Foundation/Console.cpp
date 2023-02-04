// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Console.h"
#include "Limits.h"
#include "Platform.h"
#include "String.h"
#include "StringConverter.h"
#include "StringNative.h"

#include <stdio.h> // stdout

#if SC_PLATFORM_WINDOWS
#include <Windows.h>
#endif

void SC::Console::print(const StringView str)
{
    if (str.isEmpty())
        return;
    SC_DEBUG_ASSERT(str.sizeInBytes() < static_cast<int>(MaxValue()));
#if SC_PLATFORM_WINDOWS
    StringView encodedPath;
    if (str.getEncoding() == StringEncoding::Ascii)
    {
        WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), str.bytesWithoutTerminator(),
                      static_cast<DWORD>(str.sizeInBytes()), nullptr, nullptr);
#if SC_DEBUG
        if (str.isNullTerminated())
        {
            OutputDebugStringA(str.bytesIncludingTerminator());
        }
        else
        {
            temporaryBuffer.clearWithoutInitializing();
            if (StringConverter::toNullTerminatedUTF16(str, temporaryBuffer, encodedPath, false))
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
        temporaryBuffer.clearWithoutInitializing();
        if (StringConverter::toNullTerminatedUTF16(str, temporaryBuffer, encodedPath, false))
        {
            WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), encodedPath.getNullTerminatedNative(),
                          static_cast<DWORD>(encodedPath.sizeInBytes() / sizeof(wchar_t)), nullptr, nullptr);
#if SC_DEBUG
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
    fwrite(str.bytesWithoutTerminator(), sizeof(char), static_cast<int>(str.sizeInBytes()), stdout);
#endif
}

void SC::Console::printNullTerminatedASCII(const StringView str)
{
    if (str.isEmpty() || str.getEncoding() != StringEncoding::Ascii)
        return;

        // SC_DEBUG_ASSERT(str.sizeInBytes() < static_cast<int>(MaxValue()));
#if SC_PLATFORM_WINDOWS
    WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), str.bytesWithoutTerminator(), static_cast<DWORD>(str.sizeInBytes()),
                  nullptr, nullptr);
    OutputDebugStringA(str.bytesIncludingTerminator());
#else
    fwrite(str.bytesWithoutTerminator(), sizeof(char), static_cast<int>(str.sizeInBytes()), stdout);
#endif
}
