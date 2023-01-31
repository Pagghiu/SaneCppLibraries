// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Console.h"
#include "Limits.h"

#include <execinfo.h> // backtrace
#include <memory.h>   // memcpy
#include <stdlib.h>   // free

bool SC::OS::printBacktrace()
{
    void* backtraceBuffer[100];
    return printBacktrace(backtraceBuffer, sizeof(backtraceBuffer));
}

bool SC::OS::printBacktrace(void** backtraceBuffer, size_t backtraceBufferSizeInBytes)
{
    const size_t numFrames = captureBacktrace(2, backtraceBuffer, backtraceBufferSizeInBytes, nullptr);
    if (numFrames == 0)
    {
        return false;
    }
    char** strs = backtrace_symbols(backtraceBuffer, static_cast<int>(numFrames));
    for (size_t i = 0; i < numFrames; ++i)
    {
        Console::print(StringView(strs[i], strlen(strs[i]), true, StringEncoding::Utf8));
        Console::print("\n");
    }
    // TODO: Fix Backtrace line numbers
    // https://stackoverflow.com/questions/8278691/how-to-fix-backtrace-line-number-error-in-c
    free(strs);
    return true;
}

SC::size_t SC::OS::captureBacktrace(size_t framesToSkip, void** backtraceBuffer, size_t backtraceBufferSizeInBytes,
                                    uint32_t* hash)
{
    const size_t   framesToCapture = backtraceBufferSizeInBytes / sizeof(void*);
    constexpr auto maxVal          = static_cast<size_t>(static_cast<int>(MaxValue()));
    if (framesToCapture > maxVal || (backtraceBuffer == nullptr))
    {
        return 0;
    }
    // This signature maps 1 to 1 with windows CaptureStackBackTrace, at some
    // point we will allow framesToSkip > 0 and compute has
    int numFrames = backtrace(backtraceBuffer, static_cast<int>(framesToCapture));
    numFrames -= framesToSkip;
    if (framesToSkip > 0)
    {
        for (int i = 0; i < numFrames; ++i)
        {
            backtraceBuffer[i] = backtraceBuffer[i + framesToSkip];
        }
    }
    if (hash)
    {
        uint32_t computedHash = 0;
        // TODO: Compute a proper hash
        for (int i = 0; i < numFrames; ++i)
        {
            uint32_t value;
            memcpy(&value, backtraceBuffer[i], sizeof(uint32_t));
            computedHash ^= value;
        }
        *hash = computedHash;
    }
    return numFrames;
}
