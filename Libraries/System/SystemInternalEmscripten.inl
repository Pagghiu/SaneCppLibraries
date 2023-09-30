// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "System.h"

SC::Result SC::SystemDynamicLibraryTraits::releaseHandle(Handle& handle) { return Result(true); }

SC::Result SC::SystemDynamicLibrary::load(StringView fullPath) { return Result(true); }

SC::Result SC::SystemDynamicLibrary::loadSymbol(StringView symbolName, void*& symbol) const { return Result(true); }

bool SC::SystemDirectories::init() { return true; }
