// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "FileSystemIterator.h"
#include <string.h> //strlen
#if SC_PLATFORM_WINDOWS
#include "Internal/FileSystemIteratorWindows.inl"
#elif SC_PLATFORM_EMSCRIPTEN
#include "Internal/FileSystemIteratorEmscripten.inl"
#else
#include "Internal/FileSystemIteratorPosix.inl"
#endif

template <>
void SC::OpaqueBuilder<SC::FileSystemIterator::InternalOpaque::Sizes>::construct(Handle& buffer)
{
    new (&buffer.reinterpret_as<Object>(), PlacementNew()) Object();
}
template <>
void SC::OpaqueBuilder<SC::FileSystemIterator::InternalOpaque::Sizes>::destruct(Object& obj)
{
    obj.~Object();
}

SC::FileSystemIterator::~FileSystemIterator() {}

[[nodiscard]] SC::Result SC::FileSystemIterator::init(StringView directory) { return internal.get().init(directory); }

[[nodiscard]] SC::Result SC::FileSystemIterator::enumerateNext()
{
    Result res = internal.get().enumerateNext(currentEntry, options);
    if (not res)
    {
        const StringView message(res.message, ::strlen(res.message), true, StringEncoding::Ascii);
        if (message != "Iteration Finished"_a8)
        {
            errorResult   = res;
            errorsChecked = false;
        }
    }
    return res;
}

[[nodiscard]] SC::Result SC::FileSystemIterator::recurseSubdirectory()
{
    if (options.recursive)
    {
        errorResult   = Result::Error("Cannot recurseSubdirectory() with recursive==true");
        errorsChecked = false;
        return errorResult;
    }
    return internal.get().recurseSubdirectory(currentEntry);
}
