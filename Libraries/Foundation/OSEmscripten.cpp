// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "OS.h"
bool SC::OS::printBacktrace() { return true; }

bool SC::OS::printBacktrace(void** backtraceBuffer, size_t backtraceBufferSizeInBytes)
{
    if (!backtraceBuffer)
        return false;
    return true;
}

SC::size_t SC::OS::captureBacktrace(size_t framesToSkip, void** backtraceBuffer, size_t backtraceBufferSizeInBytes,
                                    uint32_t* hash)
{
    if (hash)
        *hash = 1;
    if (backtraceBuffer == nullptr)
        return 0;
    return 1;
}
