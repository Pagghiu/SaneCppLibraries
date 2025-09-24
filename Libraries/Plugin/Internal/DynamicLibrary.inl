// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Memory/String.h"
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
    StringPath fullPathZeroTerminated;
    SC_TRY(fullPathZeroTerminated.assign(fullPath));
    HMODULE module = ::LoadLibraryW(fullPathZeroTerminated.view().getNullTerminatedNative());
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
    char symbolNullTerminated[512];
    if (symbolName.getEncoding() == StringEncoding::Utf16)
    {
        // use widechartomulti byte conversion
        const int numChars =
            WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<const wchar_t*>(symbolName.bytesWithoutTerminator()),
                                static_cast<int>(symbolName.sizeInBytes()), symbolNullTerminated,
                                static_cast<int>(sizeof(symbolNullTerminated) - 1), nullptr, nullptr);
        if (numChars == 0)
        {
            return Result::Error("SystemDynamicLibrary::loadSymbol - WideCharToMultiByte failed");
        }
        symbolNullTerminated[numChars] = 0;
    }
    else
    {
        if (symbolName.sizeInBytes() + 1 > sizeof(symbolNullTerminated))
            return Result::Error("SystemDynamicLibrary::loadSymbol - symbol name too long");
        ::memcpy(symbolNullTerminated, symbolName.bytesWithoutTerminator(), symbolName.sizeInBytes());
        symbolNullTerminated[symbolName.sizeInBytes()] = 0; // ensure null termination
    }

    HMODULE module;
    memcpy(&module, &handle, sizeof(HMODULE));
    symbol = reinterpret_cast<void*>(::GetProcAddress(module, symbolNullTerminated));
    return Result(symbol != nullptr);
}
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX

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
    StringPath fullPathZeroTerminated;
    SC_TRY(fullPathZeroTerminated.assign(fullPath));
    handle = ::dlopen(fullPathZeroTerminated.view().getNullTerminatedNative(), RTLD_LAZY);
    if (handle == nullptr)
    {
        return Result::Error("dlopen failed");
    }
    return Result(true);
}

SC::Result SC::SystemDynamicLibrary::loadSymbol(StringView symbolName, void*& symbol) const
{
    SC_TRY_MSG(isValid(), "Invalid dlsym handle");
    // Using StringPath just to null terminate the symbol name
    StringPath symbolZeroTerminated;
    SC_TRY(symbolZeroTerminated.assign(symbolName));
    symbol = ::dlsym(handle, symbolZeroTerminated.view().getNullTerminatedNative());
    return Result(symbol != nullptr);
}
#else

SC::Result SC::detail::SystemDynamicLibraryDefinition::releaseHandle(Handle&) { return Result(false); }

SC::Result SC::SystemDynamicLibrary::load(StringView) { return Result(false); }

SC::Result SC::SystemDynamicLibrary::loadSymbol(StringView, void*&) const { return Result(false); }

#endif
