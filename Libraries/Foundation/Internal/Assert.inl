// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/Assert.h"

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#pragma comment(lib, "Dbghelp.lib")
#include <DbgHelp.h> // For stack trace symbol resolution
#else
#include <execinfo.h> // backtrace
#include <string.h>   // strlen
#include <unistd.h>   // _exit
#endif

#include <stdio.h>  // fwrite (posix), snprintf (windows)
#include <stdlib.h> // free (posix), _exit (windows)

namespace SC
{
void Assert::exit(int code) { ::_exit(code); }

struct Assert::Internal
{
    static void printAscii(const char* str)
    {
#if SC_PLATFORM_WINDOWS
        ::WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), str, static_cast<DWORD>(::strlen(str)), nullptr, nullptr);
        ::OutputDebugStringA(str);
#else
        ::fwrite(str, sizeof(char), ::strlen(str), stdout);
#endif
    }
    static bool printBacktrace(void** backtraceBuffer, size_t backtraceBufferSizeInBytes)
    {
#if SC_PLATFORM_WINDOWS
        const size_t framesToSkip = 3; // Skip the current function and two internal functions
        const USHORT maxFrames    = static_cast<USHORT>(backtraceBufferSizeInBytes / sizeof(void*));
        const USHORT numFrames    = ::CaptureStackBackTrace(framesToSkip, maxFrames, backtraceBuffer, nullptr);
        if (numFrames == 0)
            return false;

        HANDLE process = ::GetCurrentProcess();
        ::SymInitialize(process, nullptr, TRUE);

        char         symbolBuffer[sizeof(SYMBOL_INFO) + 256];
        SYMBOL_INFO* symbol  = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
        symbol->MaxNameLen   = 255;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

        for (USHORT i = 0; i < numFrames; ++i)
        {
            DWORD64 address = (DWORD64)(backtraceBuffer[i]);
            if (::SymFromAddr(process, address, 0, symbol))
            {
                char buffer[512];
                ::snprintf(buffer, sizeof(buffer), "[%u] %s - 0x%p\n", i, symbol->Name, (void*)address);
                printAscii(buffer);
            }
            else
            {
                char buffer[128];
                ::snprintf(buffer, sizeof(buffer), "[%u] ??? - 0x%p\n", i, (void*)address);
                printAscii(buffer);
            }
        }
        ::SymCleanup(process);
        return true;
#else
        const size_t framesToSkip    = 2;
        const size_t framesToCapture = backtraceBufferSizeInBytes / sizeof(void*);
        int          numFrames       = ::backtrace(backtraceBuffer, static_cast<int>(framesToCapture));
        if (framesToSkip > static_cast<size_t>(numFrames))
            return 0;
        numFrames -= framesToSkip;
        if (framesToSkip > 0)
        {
            for (int frame = 0; frame < numFrames; ++frame)
            {
                backtraceBuffer[frame] = backtraceBuffer[static_cast<size_t>(frame) + framesToSkip];
            }
        }
        if (numFrames == 0)
        {
            return false;
        }
        char** strs = ::backtrace_symbols(backtraceBuffer, static_cast<int>(numFrames));
        for (int idx = 0; idx < numFrames; ++idx)
        {
            printAscii(strs[idx]);
            printAscii("\n");
        }
        ::free(strs);
        return true;
#endif
    }
};

void Assert::printBacktrace(const char* expression, bool result, const native_char_t* filename, const char* function,
                            int line)
{
    return printBacktrace(expression, Result(result), filename, function, line);
}

void Assert::printBacktrace(const char* expression, Result result, const native_char_t* filename, const char* function,
                            int line)
{
    char buffer[2048];
#if SC_PLATFORM_WINDOWS
#define NATIVE_PRINT_SPECIFIER "%ws"
#else
#define NATIVE_PRINT_SPECIFIER "%s"
#endif
    ::snprintf(buffer, sizeof(buffer),
               "Assertion failed: (%s)\n"
               "Reason: %s\n"
               "File: " NATIVE_PRINT_SPECIFIER "\n"
               "Function: %s\nLine: %d\n",
               expression, result.message, filename, function, line);
    Internal::printAscii(buffer);
    void* backtraceBuffer[256];
    Internal::printBacktrace(backtraceBuffer, sizeof(backtraceBuffer));
}
} // namespace SC
