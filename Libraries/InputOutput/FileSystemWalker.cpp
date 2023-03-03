// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "FileSystemWalker.h"
#if SC_PLATFORM_WINDOWS
#include "FileSystemWalkerInternalWindows.inl"
#elif SC_PLATFORM_EMSCRIPTEN
#include "FileSystemWalkerInternalEmscripten.inl"
#else
#include "FileSystemWalkerInternalPosix.inl"
#endif

template <>
void SC::OpaqueFunctions<SC::FileSystemWalker::Internal, SC::FileSystemWalker::InternalSize,
                         SC::FileSystemWalker::InternalAlignment>::construct(Handle& buffer)
{
    new (&buffer.reinterpret_as<Object>(), PlacementNew()) Object();
}

template <>
void SC::OpaqueFunctions<SC::FileSystemWalker::Internal, SC::FileSystemWalker::InternalSize,
                         SC::FileSystemWalker::InternalAlignment>::destruct(Object& obj)
{
    obj.~Internal();
}

SC::FileSystemWalker::~FileSystemWalker()
{
    // You forgot to call FileSystemWalker::checkErrors
    SC_DEBUG_ASSERT(errorsChecked);
}

[[nodiscard]] SC::ReturnCode SC::FileSystemWalker::init(StringView directory) { return internal.get().init(directory); }

[[nodiscard]] SC::ReturnCode SC::FileSystemWalker::enumerateNext()
{
    ReturnCode res = internal.get().enumerateNext(entry, options);
    if (not res)
    {
        if (res.message != "Iteration Finished"_a8)
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
        errorResult   = "Cannot recurseSubdirectory() with recursive==true"_a8;
        errorsChecked = false;
        return errorResult;
    }
    return internal.get().recurseSubdirectory(entry);
}
