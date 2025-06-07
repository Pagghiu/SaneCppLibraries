// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "File.h"

#if SC_PLATFORM_WINDOWS
#include "Internal/FileWindows.inl"
#else
#include "Internal/FilePosix.inl"
#endif

//-------------------------------------------------------------------------------------------------------
// FileDescriptor
//-------------------------------------------------------------------------------------------------------
SC::Result SC::FileDescriptor::openForWriteToDevNull()
{
#if SC_PLATFORM_WINDOWS
    SC_TRY(File(*this).open("NUL", FileOpen::Append));
#else
    SC_TRY(File(*this).open("/dev/null", FileOpen::Append));
#endif
    return Result(true);
}

SC::Result SC::FileDescriptor::write(Span<const uint8_t> data, uint64_t offset)
{
    return write({reinterpret_cast<const char*>(data.data()), data.sizeInBytes()}, offset);
}

SC::Result SC::FileDescriptor::read(Span<uint8_t> data, Span<uint8_t>& actuallyRead)
{
    Span<char> readBytes;
    SC_TRY(read({reinterpret_cast<char*>(data.data()), data.sizeInBytes()}, readBytes));
    actuallyRead = {reinterpret_cast<uint8_t*>(readBytes.data()), readBytes.sizeInBytes()};
    return Result(true);
}

SC::Result SC::FileDescriptor::read(Span<uint8_t> data, Span<uint8_t>& actuallyRead, uint64_t offset)
{
    Span<char> readBytes;
    SC_TRY(read({reinterpret_cast<char*>(data.data()), data.sizeInBytes()}, readBytes, offset));
    actuallyRead = {reinterpret_cast<uint8_t*>(readBytes.data()), readBytes.sizeInBytes()};
    return Result(true);
}

SC::Result SC::FileDescriptor::write(Span<const uint8_t> data)
{
    return write({reinterpret_cast<const char*>(data.data()), data.sizeInBytes()});
}

//-------------------------------------------------------------------------------------------------------
// File
//-------------------------------------------------------------------------------------------------------
SC::Result SC::File::readUntilEOFTemplate(Buffer& destination)
{
    char buffer[1024];
    SC_TRY(fd.isValid());
    ReadResult             readResult;
    FileDescriptor::Handle fileDescriptor;
    SC_TRY(fd.get(fileDescriptor, Result::Error("FileDescriptor::readAppend - Invalid Handle")));
    while (not readResult.isEOF)
    {
        SC_TRY(Internal::readAppend<char>(fileDescriptor, destination, {buffer, sizeof(buffer)}, readResult));
    }
    return Result(true);
}

SC::Result SC::File::readUntilEOF(Buffer& destination) { return readUntilEOFTemplate(destination); }

SC::Result SC::File::readUntilEOF(String& destination)
{
    (void)StringConverter::popNullTermIfNotEmpty(destination.data, destination.encoding);
    SC_TRY(readUntilEOF(destination.data));
    if (destination.isEmpty())
        return Result(true);
    return Result(StringConverter::pushNullTerm(destination.data, destination.encoding));
}

//-------------------------------------------------------------------------------------------------------
// PipeDescriptor
//-------------------------------------------------------------------------------------------------------
SC::Result SC::PipeDescriptor::close()
{
    SC_TRY(readPipe.close());
    return writePipe.close();
}
