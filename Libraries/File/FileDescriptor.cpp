// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "FileDescriptor.h"

#if SC_PLATFORM_WINDOWS
#include "Internal/FileDescriptorWindows.inl"
#else
#include "Internal/FileDescriptorPosix.inl"
#endif

#include "../Strings/String.h"

SC::Result SC::PipeDescriptor::close()
{
    SC_TRY(readPipe.close());
    return writePipe.close();
}

SC::Result SC::FileDescriptor::open(StringView path, OpenMode mode) { return open(path, mode, OpenOptions()); }

SC::Result SC::FileDescriptor::readUntilEOF(Vector<char>& destination)
{
    char buffer[1024];
    SC_TRY(isValid());
    ReadResult readResult;
    while (not readResult.isEOF)
    {
        SC_TRY(readAppend(destination, {buffer, sizeof(buffer)}, readResult));
    }
    return Result(true);
}

SC::Result SC::FileDescriptor::readUntilEOF(String& destination)
{
    SC_TRY(readUntilEOF(destination.data));
    if (destination.isEmpty())
        return Result(true);
    return Result(StringConverter::pushNullTerm(destination.data, destination.encoding));
}
