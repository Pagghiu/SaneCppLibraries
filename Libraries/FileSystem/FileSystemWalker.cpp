// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "FileSystemWalker.h"
#include <string.h> //strlen
#if SC_PLATFORM_WINDOWS
#include "FileSystemWalkerInternalWindows.inl"
#elif SC_PLATFORM_EMSCRIPTEN
#include "FileSystemWalkerInternalEmscripten.inl"
#else
#include "FileSystemWalkerInternalPosix.inl"
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

[[nodiscard]] SC::ReturnCode SC::FileSystemWalker::init(StringView directory) { return internal.get().init(directory); }

[[nodiscard]] SC::ReturnCode SC::FileSystemWalker::enumerateNext()
{
    ReturnCode res = internal.get().enumerateNext(currentEntry, options);
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

[[nodiscard]] SC::ReturnCode SC::FileSystemWalker::recurseSubdirectory()
{
    if (options.recursive)
    {
        errorResult   = ReturnCode::Error("Cannot recurseSubdirectory() with recursive==true");
        errorsChecked = false;
        return errorResult;
    }
    return internal.get().recurseSubdirectory(currentEntry);
}
