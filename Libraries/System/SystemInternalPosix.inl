// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "System.h"

#include "../Foundation/StringConverter.h"
#include "../System/Console.h"

#include <dlfcn.h>    // dlopen
#include <execinfo.h> // backtrace
#include <stdlib.h>   // free

SC::ReturnCode SC::SystemDynamicLibraryTraits::releaseHandle(Handle& handle)
{
    if (handle)
    {
        const int res = ::dlclose(handle);
        return res == 0;
    }
    return true;
}

SC::ReturnCode SC::SystemDynamicLibrary::load(StringView fullPath)
{
    SC_TRY_IF(close());
    SmallString<1024> string = StringEncoding::Native;
    StringConverter   converter(string);
    StringView        fullPathZeroTerminated;
    SC_TRY_IF(converter.convertNullTerminateFastPath(fullPath, fullPathZeroTerminated));
    handle = ::dlopen(fullPathZeroTerminated.getNullTerminatedNative(), RTLD_LAZY);
    if (handle == nullptr)
    {
        return "dlopen failed"_a8;
    }
    return true;
}

SC::ReturnCode SC::SystemDynamicLibrary::getSymbol(StringView symbolName, void*& symbol)
{
    SC_TRY_MSG(isValid(), "Invalid dlsym handle"_a8);
    SmallString<1024> string = StringEncoding::Native;
    StringConverter   converter(string);
    StringView        symbolZeroTerminated;
    SC_TRY_IF(converter.convertNullTerminateFastPath(symbolName, symbolZeroTerminated));
    symbol = ::dlsym(handle, symbolZeroTerminated.getNullTerminatedNative());
    return symbol != nullptr;
}

bool SC::SystemDebug::printBacktrace()
{
    void* backtraceBuffer[100];
    return printBacktrace(backtraceBuffer, sizeof(backtraceBuffer));
}

bool SC::SystemDebug::printBacktrace(void** backtraceBuffer, size_t backtraceBufferSizeInBytes)
{
    const size_t numFrames = captureBacktrace(2, backtraceBuffer, backtraceBufferSizeInBytes, nullptr);
    if (numFrames == 0)
    {
        return false;
    }
    char** strs = backtrace_symbols(backtraceBuffer, static_cast<int>(numFrames));
    for (size_t i = 0; i < numFrames; ++i)
    {
        StringView line(strs[i], strlen(strs[i]), true, StringEncoding::Ascii);
        Console::printNullTerminatedASCII(line);
        Console::printNullTerminatedASCII("\n"_a8);
    }
    // TODO: Fix Backtrace line numbers
    // https://stackoverflow.com/questions/8278691/how-to-fix-backtrace-line-number-error-in-c
    free(strs);
    return true;
}

SC::size_t SC::SystemDebug::captureBacktrace(size_t framesToSkip, void** backtraceBuffer,
                                             size_t backtraceBufferSizeInBytes, uint32_t* hash)
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
            memcpy(&value, backtraceBuffer[i], sizeof(uint32_t));
            computedHash ^= value;
        }
        *hash = computedHash;
    }
    return static_cast<size_t>(numFrames);
}
