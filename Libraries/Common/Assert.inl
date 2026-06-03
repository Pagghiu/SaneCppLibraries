// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

// Intentionally no #pragma once / include guard.
// This file is source material for private implementation namespaces.
// Each including library must get its own copy, especially in single-file amalgamations.
//
// Required include context: declare an assert provider with SC_DECLARE_ASSERT_PROVIDER before including this fragment.
// Optional defines before this file: SC_ASSERT_PROVIDER, SC_ASSERT_NAMESPACE_BEGIN, SC_ASSERT_NAMESPACE_END.
#if SC_PLATFORM_LINUX
#if defined(__has_include)
#if __has_include(<features.h>)
#include <features.h>
#endif
#endif
#endif

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#if SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL
#pragma comment(lib, "Dbghelp.lib")
#endif
#include <DbgHelp.h> // For stack trace symbol resolution
#else
#if !SC_PLATFORM_LINUX || defined(__GLIBC__)
#include <execinfo.h> // backtrace
#endif
#include <string.h> // strlen
#include <unistd.h> // _exit
#endif

#include <stdio.h>  // fwrite (posix), snprintf (windows)
#include <stdlib.h> // free (posix), _exit (windows)

#ifndef SC_ASSERT_NAMESPACE_BEGIN
#define SC_ASSERT_NAMESPACE_BEGIN                                                                                      \
    namespace SC                                                                                                       \
    {
#define SC_ASSERT_NAMESPACE_END }
#endif
#ifndef SC_ASSERT_PROVIDER
#define SC_ASSERT_PROVIDER Assert
#endif

SC_ASSERT_NAMESPACE_BEGIN
void SC_ASSERT_PROVIDER::exit(int code) { ::_exit(code); }

struct SC_ASSERT_PROVIDER::Internal
{
    static void printAscii(const char* str)
    {
#if SC_PLATFORM_WINDOWS
        HANDLE      handle       = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD       numWritten   = 0;
        const DWORD numBytes     = static_cast<DWORD>(::strlen(str));
        const bool  wroteConsole = handle != nullptr && handle != INVALID_HANDLE_VALUE &&
                                  ::WriteConsoleA(handle, str, numBytes, &numWritten, nullptr);
        if (not wroteConsole)
        {
            ::fwrite(str, sizeof(char), numBytes, stdout);
            ::fflush(stdout);
        }
        ::OutputDebugStringA(str);
#else
        ::fwrite(str, sizeof(char), ::strlen(str), stdout);
        ::fflush(stdout);
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
#if SC_PLATFORM_LINUX && !defined(__GLIBC__)
        (void)(backtraceBuffer);
        (void)(backtraceBufferSizeInBytes);
        printAscii("Backtrace unavailable on this Linux libc/runtime.\n");
        return false;
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
#endif
    }
};

void SC_ASSERT_PROVIDER::printBacktrace(const char* expression, bool result, const native_char_t* filename,
                                        const char* function, int line)
{
    return printBacktrace(expression, Result(result), filename, function, line);
}

void SC_ASSERT_PROVIDER::printBacktrace(const char* expression, Result result, const native_char_t* filename,
                                        const char* function, int line)
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
#undef NATIVE_PRINT_SPECIFIER
    Internal::printAscii(buffer);
    void* backtraceBuffer[256];
    Internal::printBacktrace(backtraceBuffer, sizeof(backtraceBuffer));
}
SC_ASSERT_NAMESPACE_END

#ifndef SC_ASSERT_KEEP_NAMESPACE_MACROS
#undef SC_ASSERT_NAMESPACE_BEGIN
#undef SC_ASSERT_NAMESPACE_END
#endif
#ifndef SC_ASSERT_KEEP_PROVIDER
#undef SC_ASSERT_PROVIDER
#endif
