// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "FileDescriptor.h"

#if SC_PLATFORM_WINDOWS
#include "FileDescriptorInternalWindows.inl"
#else
#include "FileDescriptorInternalPosix.inl"
#endif

#include "../Foundation/String.h"

SC::ReturnCode SC::PipeDescriptor::close()
{
    SC_TRY_IF(readPipe.close());
    return writePipe.close();
}

SC::ReturnCode SC::FileDescriptor::open(StringView path, OpenMode mode) { return open(path, mode, OpenOptions()); }

SC::ReturnCode SC::FileDescriptor::readUntilEOF(Vector<char_t>& destination)
{
    char buffer[1024];
    SC_TRY_IF(isValid());
    ReadResult readResult;
    while (not readResult.isEOF)
    {
        SC_TRY(readResult, readAppend(destination, {buffer, sizeof(buffer)}));
    }
    return true;
}

SC::ReturnCode SC::FileDescriptor::readUntilEOF(String& destination)
{
    SC_TRY_IF(readUntilEOF(destination.data));
    return StringConverter::pushNullTerm(destination.data, destination.encoding);
}
