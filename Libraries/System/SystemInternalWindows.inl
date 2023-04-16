// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../Foundation/Path.h"
#include "../Foundation/SmallVector.h"
#include "../Foundation/StringBuilder.h"
#include "System.h"

#include <Windows.h>

bool SC::SystemDirectories::init()
{
    // TODO: OsPaths::init() for Windows is messy. Tune the API to improve writing software like this.
    // Reason is because it's handy counting in wchars but we can't do it with StringNative.
    // Additionally we must convert to utf8 at the end otherwise path::dirname will not work
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

    StringView utf16executable = StringView(Span<const wchar_t>(buffer.data(), (buffer.size() - 1) * sizeof(wchar_t)),
                                            true, StringEncoding::Utf16);

    // TODO: SystemDirectories::init - We must also convert to utf8 because dirname will not work on non utf8 or ascii
    // text assigning directly the SmallString inside StringNative will copy as is instad of converting utf16 to utf8
    executableFile = ""_u8;
    StringBuilder builder(executableFile);
    SC_TRY_IF(builder.append(utf16executable));
    applicationRootDirectory = Path::Windows::dirname(executableFile.view());
    return true;
}

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
