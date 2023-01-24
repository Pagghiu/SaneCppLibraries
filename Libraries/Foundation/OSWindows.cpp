// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "OS.h"
#include "Path.h"
#include "SmallVector.h"
#include "StringConverter.h"

#include <Windows.h>

extern SC::OSPaths globalPaths;

bool SC::OSPaths::init()
{
    SmallVector<wchar_t, MAX_PATH> buffer;

    int numChars;
    int tries = 0;
    do
    {
        SC_TRY_IF(buffer.resizeWithoutInitializing(buffer.size() + MAX_PATH));
        // Is returned null terminated
        numChars = GetModuleFileNameW(0L, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (tries++ >= 10)
        {
            return false;
        }
    } while (numChars == buffer.size() && GetLastError() == ERROR_INSUFFICIENT_BUFFER);

    SC_TRY_IF(buffer.resizeWithoutInitializing(numChars + 1));
    SC_TRY_IF(buffer[numChars] == 0);

    StringView utf16executable = StringView(reinterpret_cast<const char*>(buffer.data()),
                                            (buffer.size() - 1) * sizeof(wchar_t), true, StringEncoding::Utf16);

    SmallVector<char_t, MAX_PATH> utf8Buffer;
    const char_t*                 nullTerminatedUTF8;
    SC_TRY_IF(StringConverter::toNullTerminatedUTF8(utf16executable, utf8Buffer, &nullTerminatedUTF8));
    globalPaths.executableFile = StringView(nullTerminatedUTF8, utf8Buffer.size() - 1, true, StringEncoding::Utf8);
    globalPaths.applicationRootDirectory = Path::Windows::dirname(globalPaths.executableFile.view());
    return true;
}

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
