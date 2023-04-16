// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Console.h"
#include "Limits.h"
#include "Platform.h"
#include "StringConverter.h"

#include <stdio.h>  // stdout
#include <string.h> // strlen

#if SC_PLATFORM_WINDOWS
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
            printConversionBuffer.clearWithoutInitializing();
            if (StringConverter::convertEncodingToUTF16(str, printConversionBuffer, &encodedPath))
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
        printConversionBuffer.clearWithoutInitializing();
        if (StringConverter::convertEncodingToUTF16(str, printConversionBuffer, &encodedPath))
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

void SC::printAssertion(const char_t* expression, const char_t* filename, const char_t* functionName, int lineNumber)
{
    // Here we're explicitly avoiding usage of StringFormat to avoid synamic allocation
    Console::printNullTerminatedASCII("Assertion failed: ("_a8);
    Console::printNullTerminatedASCII(StringView(expression, strlen(expression), true, StringEncoding::Ascii));
    Console::printNullTerminatedASCII(")\nFile: "_a8);
    Console::printNullTerminatedASCII(StringView(filename, strlen(filename), true, StringEncoding::Ascii));
    Console::printNullTerminatedASCII("\nFunction: "_a8);
    Console::printNullTerminatedASCII(StringView(functionName, strlen(functionName), true, StringEncoding::Ascii));
    Console::printNullTerminatedASCII("\nLine: "_a8);
    char_t    buffer[50];
    const int numCharsExcludingTerminator = snprintf(buffer, sizeof(buffer), "%d", lineNumber);
    Console::printNullTerminatedASCII(StringView(buffer, numCharsExcludingTerminator, true, StringEncoding::Ascii));
    Console::printNullTerminatedASCII("\n"_a8);
}
