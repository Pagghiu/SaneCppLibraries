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

SC::ReturnCode SC::FileDescriptorPipe::close()
{
    SC_TRY_IF(readPipe.close());
    return writePipe.close();
}

SC::ReturnCode SC::FileDescriptorPipe::readUntilEOF(Vector<char_t>& destination)
{
    return readPipe.readUntilEOF(destination);
}
SC::ReturnCode SC::FileDescriptorPipe::readUntilEOF(String& destination) { return readPipe.readUntilEOF(destination); }

SC::ReturnCode SC::FileDescriptor::readUntilEOF(Vector<char_t>& destination)
{
    char buffer[1024];
    SC_TRY_IF(handle.isValid());
    FileDescriptor::ReadResult readResult;
    while (not readResult.isEOF)
    {
        SC_TRY(readResult, readAppend(destination, {buffer, sizeof(buffer)}));
    }
    return true;
}

SC::ReturnCode SC::FileDescriptor::readUntilEOF(String& destination)
{
    SC_TRY_IF(readUntilEOF(destination.data));
    return destination.pushNullTerm();
}
