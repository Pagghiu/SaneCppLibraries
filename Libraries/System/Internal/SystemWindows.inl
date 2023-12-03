// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../../Containers/SmallVector.h"
#include "../../FileSystem/Path.h"
#include "../../Strings/StringBuilder.h"
#include "../../Strings/StringConverter.h"
#include "../../Threading/Atomic.h"
#include "../System.h"

#include <Windows.h>
#pragma comment(lib, "Ws2_32.lib")

SC::Result SC::detail::SystemDynamicLibraryDefinition::releaseHandle(Handle& handle)
{
    if (handle)
    {
        static_assert(sizeof(HMODULE) == sizeof(Handle), "sizeof(HMODULE)");
        static_assert(alignof(HMODULE) == alignof(Handle), "alignof(HMODULE)");
        HMODULE module;
        memcpy(&module, &handle, sizeof(HMODULE));
        handle         = nullptr;
        const BOOL res = ::FreeLibrary(module);
        return Result(res == TRUE);
    }
    return Result(true);
}

SC::Result SC::SystemDynamicLibrary::load(StringView fullPath)
{
    SC_TRY(close());
    SmallString<1024> string = StringEncoding::Native;
    StringConverter   converter(string);
    StringView        fullPathZeroTerminated;
    SC_TRY(converter.convertNullTerminateFastPath(fullPath, fullPathZeroTerminated));
    HMODULE module = ::LoadLibraryW(fullPathZeroTerminated.getNullTerminatedNative());
    if (module == nullptr)
    {
        return Result::Error("LoadLibraryW failed");
    }
    memcpy(&handle, &module, sizeof(HMODULE));
    return Result(true);
}

SC::Result SC::SystemDynamicLibrary::loadSymbol(StringView symbolName, void*& symbol) const
{
    SC_TRY_MSG(isValid(), "Invalid GetProcAddress handle");
    SmallString<1024> string = StringEncoding::Ascii;
    StringConverter   converter(string);
    StringView        symbolZeroTerminated;
    SC_TRY(converter.convertNullTerminateFastPath(symbolName, symbolZeroTerminated));
    HMODULE module;
    memcpy(&module, &handle, sizeof(HMODULE));
    symbol = reinterpret_cast<void*>(::GetProcAddress(module, symbolZeroTerminated.bytesIncludingTerminator()));
    return Result(symbol != nullptr);
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
        SC_TRY(buffer.resizeWithoutInitializing(buffer.size() + MAX_PATH));
        // Is returned null terminated
        numChars = GetModuleFileNameW(0L, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (tries++ >= 10)
        {
            return false;
        }
    } while (numChars == buffer.size() && GetLastError() == ERROR_INSUFFICIENT_BUFFER);

    SC_TRY(buffer.resizeWithoutInitializing(numChars + 1));
    SC_TRY(buffer[numChars] == 0);

    StringView utf16executable = StringView(Span<const wchar_t>(buffer.data(), (buffer.size() - 1)), true);

    // TODO: SystemDirectories::init - We must also convert to utf8 because dirname will not work on non utf8 or ascii
    // text assigning directly the SmallString inside StringNative will copy as is instad of converting utf16 to utf8
    executableFile = ""_u8;
    StringBuilder builder(executableFile);
    SC_TRY(builder.append(utf16executable));
    applicationRootDirectory = Path::dirname(executableFile.view(), Path::AsWindows);
    return Result(true);
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

SC::Result SC::SystemFunctions::initNetworking()
{
    if (isNetworkingInited() == false)
    {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        {
            return Result::Error("WSAStartup failed");
        }
        Internal::get().networkingInited.exchange(true);
    }
    return Result(true);
}

SC::Result SC::SystemFunctions::shutdownNetworking()
{
    WSACleanup();
    Internal::get().networkingInited.exchange(false);
    return Result(true);
}
