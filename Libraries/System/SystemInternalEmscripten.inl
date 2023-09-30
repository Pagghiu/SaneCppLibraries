// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "System.h"

SC::ReturnCode SC::SystemDynamicLibraryTraits::releaseHandle(Handle& handle) { return ReturnCode(true); }

SC::ReturnCode SC::SystemDynamicLibrary::load(StringView fullPath) { return ReturnCode(true); }

SC::ReturnCode SC::SystemDynamicLibrary::loadSymbol(StringView symbolName, void*& symbol) const
{
    return ReturnCode(true);
}

bool SC::SystemDirectories::init() { return true; }
