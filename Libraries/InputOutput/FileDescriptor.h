// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/MovableHandle.h"
#include "../Foundation/Result.h"
#include "../Foundation/Vector.h"
namespace SC
{
struct FileDescriptor;
struct FileDescriptorPipe;
#if SC_PLATFORM_WINDOWS
using FileNativeDescriptor = void*;
ReturnCode FileNativeDescriptorCloseWindows(const FileNativeDescriptor&);
using FileNativeMovableHandle = MovableHandle<FileNativeDescriptor, (FileNativeDescriptor)((long long)-1), ReturnCode,
                                              FileNativeDescriptorCloseWindows>;
#else
using FileNativeDescriptor = int;
ReturnCode FileNativeDescriptorClosePosix(const FileNativeDescriptor&);
using FileNativeMovableHandle = MovableHandle<FileNativeDescriptor, -1, ReturnCode, FileNativeDescriptorClosePosix>;
#endif
struct FileDescriptorWindows;
struct FileDescriptorPosix;
} // namespace SC

struct SC::FileDescriptorWindows
{
    SC::FileDescriptor&      fileDescriptor;
    [[nodiscard]] ReturnCode disableInherit();
};
struct SC::FileDescriptorPosix
{
    SC::FileDescriptor&      fileDescriptor;
    [[nodiscard]] ReturnCode setCloseOnExec();
    [[nodiscard]] ReturnCode redirect(FileNativeDescriptor fds);

    static FileNativeDescriptor getStandardInputFDS();
    static FileNativeDescriptor getStandardOutputFDS();
    static FileNativeDescriptor getStandardErrorFDS();
};

struct SC::FileDescriptor : public FileNativeMovableHandle
{
    struct ReadResult
    {
        size_t actuallyRead = 0;
        bool   isEOF        = false;
    };
    [[nodiscard]] Result<ReadResult> readAppend(Vector<char>& output, Span<char> fallbackBuffer);

    FileDescriptorPosix   posix() { return {*this}; }
    FileDescriptorWindows windows() { return {*this}; }
};

struct SC::FileDescriptorPipe
{
    FileDescriptor readPipe;
    FileDescriptor writePipe;

    [[nodiscard]] ReturnCode createPipe();
};
