// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "DynamicLibrary.h"

#if SC_PLATFORM_WINDOWS

#include <Windows.h>

SC::ResultPlugin SC::detail::SystemDynamicLibraryDefinition::releaseHandle(Handle& handle)
{
    if (handle)
    {
        static_assert(sizeof(HMODULE) == sizeof(Handle), "sizeof(HMODULE)");
        static_assert(alignof(HMODULE) == alignof(Handle), "alignof(HMODULE)");
        HMODULE module;
        memcpy(&module, &handle, sizeof(HMODULE));
        handle         = nullptr;
        const BOOL res = ::FreeLibrary(module);
        if (res == FALSE)
            return ResultPlugin::withNativeError(PluginError::DynamicLibraryCloseFailed,
                                                 PluginErrorDetail::WindowsDynamicLibraryClose, ::GetLastError());
        return ResultPlugin(true);
    }
    return ResultPlugin(true);
}

SC::ResultPlugin SC::SystemDynamicLibrary::load(StringSpan fullPath)
{
    SC_TRY(close());
    StringPath fullPathZeroTerminated;
    if (not fullPathZeroTerminated.assign(fullPath))
        return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::PathAssign);
    HMODULE module = ::LoadLibraryW(fullPathZeroTerminated.view().getNullTerminatedNative());
    if (module == nullptr)
    {
        return ResultPlugin::withNativeError(PluginError::DynamicLibraryLoadFailed,
                                             PluginErrorDetail::WindowsDynamicLibraryLoad, ::GetLastError());
    }
    memcpy(&handle, &module, sizeof(HMODULE));
    return ResultPlugin(true);
}

SC::ResultPlugin SC::SystemDynamicLibrary::loadSymbol(StringSpan symbolName, void*& symbol) const
{
    if (not isValid())
        return ResultPlugin(PluginError::DynamicLibraryNotLoaded, PluginErrorDetail::WindowsDynamicLibraryGetSymbol);
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
            return ResultPlugin::withNativeError(PluginError::SymbolNameConversionFailed,
                                                 PluginErrorDetail::WindowsSymbolNameConversion, ::GetLastError());
        }
        symbolNullTerminated[numChars] = 0;
    }
    else
    {
        if (symbolName.sizeInBytes() + 1 > sizeof(symbolNullTerminated))
            return ResultPlugin::withRequiredBytes(PluginError::SymbolNameCapacityExceeded,
                                                   PluginErrorDetail::WindowsSymbolNameConversion,
                                                   static_cast<uint32_t>(symbolName.sizeInBytes() + 1));
        ::memcpy(symbolNullTerminated, symbolName.bytesWithoutTerminator(), symbolName.sizeInBytes());
        symbolNullTerminated[symbolName.sizeInBytes()] = 0; // ensure null termination
    }

    HMODULE module;
    memcpy(&module, &handle, sizeof(HMODULE));
    symbol = reinterpret_cast<void*>(::GetProcAddress(module, symbolNullTerminated));
    if (symbol == nullptr)
        return ResultPlugin::withNativeError(PluginError::SymbolNotFound,
                                             PluginErrorDetail::WindowsDynamicLibraryGetSymbol, ::GetLastError());
    return ResultPlugin(true);
}
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX

#include <dlfcn.h> // dlopen

SC::ResultPlugin SC::detail::SystemDynamicLibraryDefinition::releaseHandle(Handle& handle)
{
    if (handle)
    {
        const int res = ::dlclose(handle);
        if (res != 0)
            return ResultPlugin(PluginError::DynamicLibraryCloseFailed, PluginErrorDetail::PosixDynamicLibraryClose);
        return ResultPlugin(true);
    }
    return ResultPlugin(true);
}

SC::ResultPlugin SC::SystemDynamicLibrary::load(StringSpan fullPath)
{
    SC_TRY(close());
    StringPath fullPathZeroTerminated;
    if (not fullPathZeroTerminated.assign(fullPath))
        return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::PathAssign);
    handle = ::dlopen(fullPathZeroTerminated.view().getNullTerminatedNative(), RTLD_LAZY);
    if (handle == nullptr)
    {
        return ResultPlugin(PluginError::DynamicLibraryLoadFailed, PluginErrorDetail::PosixDynamicLibraryLoad);
    }
    return ResultPlugin(true);
}

SC::ResultPlugin SC::SystemDynamicLibrary::loadSymbol(StringSpan symbolName, void*& symbol) const
{
    if (not isValid())
        return ResultPlugin(PluginError::DynamicLibraryNotLoaded, PluginErrorDetail::PosixDynamicLibraryGetSymbol);
    // Using StringPath just to null terminate the symbol name
    StringPath symbolZeroTerminated;
    if (not symbolZeroTerminated.assign(symbolName))
        return ResultPlugin::withRequiredBytes(PluginError::SymbolNameCapacityExceeded,
                                               PluginErrorDetail::PosixSymbolNameAssign,
                                               static_cast<uint32_t>(symbolName.sizeInBytes() + 1));
    symbol = ::dlsym(handle, symbolZeroTerminated.view().getNullTerminatedNative());
    if (symbol == nullptr)
        return ResultPlugin(PluginError::SymbolNotFound, PluginErrorDetail::PosixDynamicLibraryGetSymbol);
    return ResultPlugin(true);
}
#else

SC::ResultPlugin SC::detail::SystemDynamicLibraryDefinition::releaseHandle(Handle&)
{
    return ResultPlugin(PluginError::DynamicLibraryOperationUnsupported);
}

SC::ResultPlugin SC::SystemDynamicLibrary::load(StringSpan)
{
    return ResultPlugin(PluginError::DynamicLibraryOperationUnsupported);
}

SC::ResultPlugin SC::SystemDynamicLibrary::loadSymbol(StringSpan, void*&) const
{
    return ResultPlugin(PluginError::DynamicLibraryOperationUnsupported);
}

#endif
