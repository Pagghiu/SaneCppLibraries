// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/Assert.h"
#if SC_PLATFORM_EMSCRIPTEN
#include <emscripten.h>
#elif SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else

#if SC_PLATFORM_APPLE
#include <TargetConditionals.h>
#endif

#include <execinfo.h> // backtrace
#include <unistd.h>   // _exit
#endif

#include <limits.h> // INT_MAX
#include <stdio.h>  // fwrite
#include <stdlib.h> // free
#include <string.h> // strlen

#if SC_PLATFORM_EMSCRIPTEN
void SC::Assert::exit(int code) { ::emscripten_force_exit(code); }
#else
void SC::Assert::exit(int code) { ::_exit(code); }
#endif

void SC::Assert::printAscii(const char* str)
{
    if (str == nullptr)
        return;

#if SC_PLATFORM_WINDOWS
    ::WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), str, static_cast<DWORD>(::strlen(str)), nullptr, nullptr);
    // TODO: We should limit the string sent to OutputDebugStringA to numBytes
    ::OutputDebugStringA(str);
#else
    ::fwrite(str, sizeof(char), ::strlen(str), stdout);
#endif
}

#if SC_PLATFORM_EMSCRIPTEN or SC_PLATFORM_WINDOWS

bool SC::Assert::printBacktrace() { return true; }

bool SC::Assert::printBacktrace(void** backtraceBuffer, size_t backtraceBufferSizeInBytes)
{
    SC_COMPILER_UNUSED(backtraceBufferSizeInBytes);
    if (!backtraceBuffer)
        return false;
    return true;
}

SC::size_t SC::Assert::captureBacktrace(size_t framesToSkip, void** backtraceBuffer, size_t backtraceBufferSizeInBytes,
                                        uint32_t* hash)
{
    SC_COMPILER_UNUSED(framesToSkip);
    SC_COMPILER_UNUSED(backtraceBufferSizeInBytes);
    if (hash)
        *hash = 1;
    if (backtraceBuffer == nullptr)
        return 0;
    return 1;
}

#else

bool SC::Assert::printBacktrace()
{
    void* backtraceBuffer[100];
    return printBacktrace(backtraceBuffer, sizeof(backtraceBuffer));
}

bool SC::Assert::printBacktrace(void** backtraceBuffer, size_t backtraceBufferSizeInBytes)
{
    const size_t numFrames = captureBacktrace(2, backtraceBuffer, backtraceBufferSizeInBytes, nullptr);
    if (numFrames == 0)
    {
        return false;
    }
    char** strs = ::backtrace_symbols(backtraceBuffer, static_cast<int>(numFrames));
    for (size_t i = 0; i < numFrames; ++i)
    {
        printAscii(strs[i]);
        printAscii("\n");
    }
    // TODO: Fix Backtrace line numbers
    // https://stackoverflow.com/questions/8278691/how-to-fix-backtrace-line-number-error-in-c
    ::free(strs);
    return true;
}

SC::size_t SC::Assert::captureBacktrace(size_t framesToSkip, void** backtraceBuffer, size_t backtraceBufferSizeInBytes,
                                        uint32_t* hash)
{
    const size_t framesToCapture = backtraceBufferSizeInBytes / sizeof(void*);
    if (framesToCapture > INT_MAX or (backtraceBuffer == nullptr))
    {
        return 0;
    }
    // This signature maps 1 to 1 with windows CaptureStackBackTrace, at some
    // point we will allow framesToSkip > 0 and compute has
    int numFrames = ::backtrace(backtraceBuffer, static_cast<int>(framesToCapture));
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
    if (hash)
    {
        uint32_t computedHash = 0;
        // TODO: Compute a proper hash
        for (int i = 0; i < numFrames; ++i)
        {
            uint32_t value;
            ::memcpy(&value, backtraceBuffer[i], sizeof(uint32_t));
            computedHash ^= value;
        }
        *hash = computedHash;
    }
    return static_cast<size_t>(numFrames);
}

#endif

void SC::Assert::print(const char* expression, const char* filename, const char* functionName, int lineNumber)
{
    // Here we're explicitly avoiding usage of StringFormat to avoid dynamic allocation
    printAscii("Assertion failed: (");
    printAscii(expression);
    printAscii(")\nFile: ");
    printAscii(filename);
    printAscii("\nFunction: ");
    printAscii(functionName);
    printAscii("\nLine: ");
    char buffer[50];
    ::snprintf(buffer, sizeof(buffer), "%d", lineNumber);
    printAscii(buffer);
    printAscii("\n");
}
