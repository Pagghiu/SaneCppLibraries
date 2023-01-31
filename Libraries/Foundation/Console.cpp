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
struct SC::Console::Internal
{
    static StringNative<512> mainThreadBuffer;
};
SC::StringNative<512> SC::Console::Internal::mainThreadBuffer;
#endif

void SC::Console::print(const StringView str)
{
    if (str.isEmpty())
        return;
    SC_DEBUG_ASSERT(str.sizeInBytes() < static_cast<int>(MaxValue()));
#if SC_PLATFORM_WINDOWS
    if (str.getEncoding() == StringEncoding::Ascii)
    {
        WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), str.getIterator<StringIteratorASCII>().getIt(),
                      static_cast<DWORD>(str.sizeInBytes()), nullptr, nullptr);
#if SC_DEBUG
        if (str.isNullTerminated())
        {
            OutputDebugStringA(str.getIterator<StringIteratorASCII>().getIt());
        }
        else if (Internal::mainThreadBuffer.convertToNullTerminated(str))
        {
            OutputDebugStringW(Internal::mainThreadBuffer.getText());
        }
#endif
    }
    else if (Internal::mainThreadBuffer.convertToNullTerminated(str))
    {
        WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), Internal::mainThreadBuffer.getText(),
                      static_cast<DWORD>(Internal::mainThreadBuffer.getTextLengthInPoints()), nullptr, nullptr);
#if SC_DEBUG
        OutputDebugStringW(Internal::mainThreadBuffer.getText());
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
