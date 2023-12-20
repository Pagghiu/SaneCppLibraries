// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
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
void SC::FileSystemIterator::InternalOpaque::construct(Handle& buffer)
{
    new (&buffer.reinterpret_as<Object>(), PlacementNew()) Object();
}
template <>
void SC::FileSystemIterator::InternalOpaque::destruct(Object& obj)
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
        const StringView message = StringView::fromNullTerminated(res.message, StringEncoding::Ascii);
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
