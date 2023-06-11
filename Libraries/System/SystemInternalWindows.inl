// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../FileSystem/Path.h"
#include "../Foundation/SmallVector.h"
#include "../Foundation/StringBuilder.h"
#include "../Foundation/StringConverter.h"
#include "../Threading/Atomic.h"
#include "System.h"

#include <Windows.h>
#pragma comment(lib, "Ws2_32.lib")

SC::ReturnCode SC::SystemDynamicLibraryTraits::releaseHandle(Handle& handle)
{
    if (handle)
    {
        static_assert(sizeof(HMODULE) == sizeof(Handle), "sizeof(HMODULE)");
        static_assert(alignof(HMODULE) == alignof(Handle), "alignof(HMODULE)");
        HMODULE module;
        memcpy(&module, &handle, sizeof(HMODULE));
        handle         = nullptr;
        const BOOL res = ::FreeLibrary(module);
        return res == TRUE;
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
    HMODULE module = ::LoadLibraryW(fullPathZeroTerminated.getNullTerminatedNative());
    if (module == nullptr)
    {
        return "LoadLibraryW failed"_a8;
    }
    memcpy(&handle, &module, sizeof(HMODULE));
    return true;
}

SC::ReturnCode SC::SystemDynamicLibrary::getSymbol(StringView symbolName, void*& symbol)
{
    SC_TRY_MSG(isValid(), "Invalid GetProcAddress handle"_a8);
    SmallString<1024> string = StringEncoding::Ascii;
    StringConverter   converter(string);
    StringView        symbolZeroTerminated;
    SC_TRY_IF(converter.convertNullTerminateFastPath(symbolName, symbolZeroTerminated));
    HMODULE module;
    memcpy(&module, &handle, sizeof(HMODULE));
    symbol = ::GetProcAddress(module, symbolZeroTerminated.bytesIncludingTerminator());
    return symbol != nullptr;
}

bool SC::SystemDirectories::init()
{
    // TODO: OsPaths::init() for Windows is messy. Tune the API to improve writing software like this.
    // Reason is because it's handy counting in wchars but we can't do it with StringNative.
    // Additionally we must convert to utf8 at the end otherwise path::dirname will not work
    SmallVector<wchar_t, MAX_PATH> buffer;

    size_t numChars;
    int    tries = 0;
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
    SC_UNUSED(backtraceBufferSizeInBytes);
    if (!backtraceBuffer)
        return false;
    return true;
}

SC::size_t SC::SystemDebug::captureBacktrace(size_t framesToSkip, void** backtraceBuffer,
                                             size_t backtraceBufferSizeInBytes, uint32_t* hash)
{
    SC_UNUSED(framesToSkip);
    SC_UNUSED(backtraceBufferSizeInBytes);
    if (hash)
        *hash = 1;
    if (backtraceBuffer == nullptr)
        return 0;
    return 1;
}

struct SC::SystemFunctions::Internal
{
    Atomic<bool> networkingInited = false;

    static Internal& get()
    {
        static Internal internal;
        return internal;
    }
};

bool SC::SystemFunctions::isNetworkingInited() { return Internal::get().networkingInited.load(); }

SC::ReturnCode SC::SystemFunctions::initNetworking()
{
    if (isNetworkingInited() == false)
    {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        {
            return "WSAStartup failed"_a8;
        }
        Internal::get().networkingInited.exchange(true);
    }
    return true;
}

SC::ReturnCode SC::SystemFunctions::shutdownNetworking()
{
    WSACleanup();
    Internal::get().networkingInited.exchange(false);
    return true;
}
