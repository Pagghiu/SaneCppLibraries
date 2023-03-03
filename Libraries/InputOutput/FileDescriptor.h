// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Opaque.h"
#include "../Foundation/Result.h"
#include "../Foundation/Vector.h"
namespace SC
{
struct FileDescriptor;
struct FileDescriptorPipe;
struct FileDescriptorWindows;
struct FileDescriptorPosix;

#if SC_PLATFORM_WINDOWS
using FileDescriptorNative                                        = void*;   // HANDLE
static constexpr FileDescriptorNative FileDescriptorNativeInvalid = nullptr; // INVALID_HANDLE_VALUE
#else
using FileDescriptorNative                                        = int; // Posix FD
static constexpr FileDescriptorNative FileDescriptorNativeInvalid = -1;  // Posix Invalid FD
#endif
ReturnCode FileDescriptorNativeClose(FileDescriptorNative&);
struct FileDescriptorNativeHandle : public UniqueTaggedHandle<FileDescriptorNative, FileDescriptorNativeInvalid,
                                                              ReturnCode, FileDescriptorNativeClose>
{
};

} // namespace SC

// TODO: Figure out if these operations should be abstracted or put into internal per platform implementation
struct SC::FileDescriptorWindows
{
    SC::FileDescriptor&      fileDescriptor;
    [[nodiscard]] ReturnCode disableInherit();
};

struct SC::FileDescriptorPosix
{
    SC::FileDescriptor&      fileDescriptor;
    [[nodiscard]] ReturnCode setCloseOnExec();
    [[nodiscard]] ReturnCode redirect(int fds);
};

struct SC::FileDescriptor
{
    struct ReadResult
    {
        size_t actuallyRead = 0;
        bool   isEOF        = false;
    };
    [[nodiscard]] Result<ReadResult> readAppend(Vector<char>& output, Span<char> fallbackBuffer);

    FileDescriptorPosix   posix() { return {*this}; }
    FileDescriptorWindows windows() { return {*this}; }

    FileDescriptorNativeHandle handle;
};

struct SC::FileDescriptorPipe
{
    FileDescriptor readPipe;
    FileDescriptor writePipe;

    [[nodiscard]] ReturnCode createPipe();
};
