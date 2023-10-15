// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "FileSystemWalker.h"
#include <string.h> //strlen
#if SC_PLATFORM_WINDOWS
#include "Internal/FileSystemWalkerWindows.inl"
#elif SC_PLATFORM_EMSCRIPTEN
#include "Internal/FileSystemWalkerEmscripten.inl"
#else
#include "Internal/FileSystemWalkerPosix.inl"
#endif

template <>
void SC::OpaqueFuncs<SC::FileSystemWalker::InternalTraits>::construct(Handle& buffer)
{
    new (&buffer.reinterpret_as<Object>(), PlacementNew()) Object();
}
template <>
void SC::OpaqueFuncs<SC::FileSystemWalker::InternalTraits>::destruct(Object& obj)
{
    obj.~Object();
}

SC::FileSystemWalker::~FileSystemWalker() {}

[[nodiscard]] SC::Result SC::FileSystemWalker::init(StringView directory) { return internal.get().init(directory); }

[[nodiscard]] SC::Result SC::FileSystemWalker::enumerateNext()
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

[[nodiscard]] SC::Result SC::FileSystemWalker::recurseSubdirectory()
{
    if (options.recursive)
    {
        errorResult   = Result::Error("Cannot recurseSubdirectory() with recursive==true");
        errorsChecked = false;
        return errorResult;
    }
    return internal.get().recurseSubdirectory(currentEntry);
}
