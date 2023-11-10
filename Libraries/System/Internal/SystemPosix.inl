// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../System.h"

#include "../../Strings/StringConverter.h"
#include "../../System/Console.h"

#include <dlfcn.h> // dlopen

SC::Result SC::SystemDynamicLibraryDefinition::releaseHandle(Handle& handle)
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
