// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "FileDescriptor.h"

struct SC::FileDescriptor::ReadResult
{
    size_t actuallyRead = 0;
    bool   isEOF        = false;
};

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

template <typename T>
SC::Result SC::FileDescriptor::readUntilEOFTemplate(Vector<T>& destination)
{
    T buffer[1024];
    SC_TRY(isValid());
    ReadResult             readResult;
    FileDescriptor::Handle fileDescriptor;
    SC_TRY(get(fileDescriptor, Result::Error("FileDescriptor::readAppend - Invalid Handle")));
    while (not readResult.isEOF)
    {
        SC_TRY(Internal::readAppend(fileDescriptor, destination, {buffer, sizeof(buffer)}, readResult));
    }
    return Result(true);
}

SC::Result SC::FileDescriptor::readUntilEOF(Vector<char>& destination) { return readUntilEOFTemplate(destination); }

SC::Result SC::FileDescriptor::readUntilEOF(Vector<uint8_t>& destination) { return readUntilEOFTemplate(destination); }

SC::Result SC::FileDescriptor::readUntilEOF(String& destination)
{
    SC_TRY(StringConverter::popNullTermIfExists(destination.data, destination.encoding));
    SC_TRY(readUntilEOF(destination.data));
    if (destination.isEmpty())
        return Result(true);
    return Result(StringConverter::pushNullTerm(destination.data, destination.encoding));
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
