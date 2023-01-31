// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Console.h"
#include "Limits.h"
#include "Platform.h"
#include "String.h"
#include "StringConverter.h"

#include <stdarg.h> // va_list
#include <stdio.h>  // printf

#if SC_PLATFORM_WINDOWS
#include <Windows.h>
struct SC::Console::Internal
{
    static Vector<wchar_t> mainThreadBuffer;
};
SC::Vector<wchar_t> SC::Console::Internal::mainThreadBuffer;
#endif

void SC::Console::print(const StringView str)
{
    if (str.isEmpty())
        return;
    SC_DEBUG_ASSERT(str.sizeInBytes() < static_cast<int>(MaxValue()));

#if SC_PLATFORM_WINDOWS
    const wchar_t* nullTerminated = nullptr;
    (void)Internal::mainThreadBuffer.resizeWithoutInitializing(0);
    if (StringConverter::toNullTerminatedUTF16(str, Internal::mainThreadBuffer, &nullTerminated, true))
    {
        WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), Internal::mainThreadBuffer.data(),
                      static_cast<DWORD>(Internal::mainThreadBuffer.size()), nullptr, nullptr);
#if SC_DEBUG
        OutputDebugStringW(Internal::mainThreadBuffer.data());
#endif
    }
    else
    {
        WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), L"ERROR: cannot format string",
                      static_cast<DWORD>(wcslen(L"ERROR: cannot format string")), nullptr, nullptr);
    }
#else
    fwrite(str.bytesWithoutTerminator(), sizeof(char), static_cast<int>(str.sizeInBytes()), stdout);
#endif
}

void SC::Console::print(const String& str) { print(str.view()); }
