// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Strings/SmallString.h"
#include "DynamicLibrary.h"

#if SC_PLATFORM_WINDOWS

#include <Windows.h>

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
#elif SC_PLATFORM_APPLE

#include <dlfcn.h> // dlopen

SC::Result SC::detail::SystemDynamicLibraryDefinition::releaseHandle(Handle& handle)
{
    if (handle)
    {
        const int res = ::dlclose(handle);
        return Result(res == 0);
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
    handle = ::dlopen(fullPathZeroTerminated.getNullTerminatedNative(), RTLD_LAZY);
    if (handle == nullptr)
    {
        return Result::Error("dlopen failed");
    }
    return Result(true);
}

SC::Result SC::SystemDynamicLibrary::loadSymbol(StringView symbolName, void*& symbol) const
{
    SC_TRY_MSG(isValid(), "Invalid dlsym handle");
    SmallString<1024> string = StringEncoding::Native;
    StringConverter   converter(string);
    StringView        symbolZeroTerminated;
    SC_TRY(converter.convertNullTerminateFastPath(symbolName, symbolZeroTerminated));
    symbol = ::dlsym(handle, symbolZeroTerminated.getNullTerminatedNative());
    return Result(symbol != nullptr);
}
#else

SC::Result SC::detail::SystemDynamicLibraryDefinition::releaseHandle(Handle& handle) { return Result(true); }

SC::Result SC::SystemDynamicLibrary::load(StringView fullPath) { return Result(true); }

SC::Result SC::SystemDynamicLibrary::loadSymbol(StringView symbolName, void*& symbol) const { return Result(true); }

#endif
