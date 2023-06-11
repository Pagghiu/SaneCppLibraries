// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "System.h"

SC::ReturnCode SC::SystemDynamicLibraryTraits::releaseHandle(Handle& handle) { return true; }

SC::ReturnCode SC::SystemDynamicLibrary::load(StringView fullPath) { return true; }

SC::ReturnCode SC::SystemDynamicLibrary::getSymbol(StringView symbolName, void*& symbol) { return true; }

bool SC::SystemDirectories::init() { return true; }

bool SC::SystemDebug::printBacktrace() { return true; }

bool SC::SystemDebug::printBacktrace(void** backtraceBuffer, size_t backtraceBufferSizeInBytes)
{
    if (!backtraceBuffer)
        return false;
    return true;
}

SC::size_t SC::SystemDebug::captureBacktrace(size_t framesToSkip, void** backtraceBuffer,
                                             size_t backtraceBufferSizeInBytes, uint32_t* hash)
{
    if (hash)
        *hash = 1;
    if (backtraceBuffer == nullptr)
        return 0;
    return 1;
}
