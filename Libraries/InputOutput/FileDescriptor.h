// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Result.h"
#include "../Foundation/Vector.h"
namespace SC
{
struct FileDescriptor;
struct FileDescriptorPipe;
#if SC_PLATFORM_WINDOWS
using FileNativeDescriptor = void*;
#else
using FileNativeDescriptor = int;
#endif
} // namespace SC

struct SC::FileDescriptor
{
    static constexpr FileNativeDescriptor InvalidFDS = (FileNativeDescriptor)((long long)-1);

    FileDescriptor()                                       = default;
    FileDescriptor(const FileDescriptor& other)            = delete;
    FileDescriptor& operator=(const FileDescriptor& other) = delete;
    FileDescriptor(FileDescriptor&& other);
    FileDescriptor& operator=(FileDescriptor&& other);
    ~FileDescriptor();

    [[nodiscard]] bool assign(FileNativeDescriptor newFileDescriptor);
    [[nodiscard]] bool close();

    struct ReadResult
    {
        size_t actuallyRead = 0;
        bool   isEOF        = false;
    };
    [[nodiscard]] Result<ReadResult> readAppend(Vector<char>& output, Span<char> fallbackBuffer);
#if SC_PLATFORM_WINDOWS
    [[nodiscard]] bool       setCloseOnExec() { return true; }
    [[nodiscard]] ReturnCode disableInherit();
#else
    [[nodiscard]] bool       setCloseOnExec();
    [[nodiscard]] ReturnCode disableInherit() { return true; }
#endif
    [[nodiscard]] ReturnCode redirect(int fds);

    FileNativeDescriptor getRawFileDescriptor() const { return fileDescriptor; }

    static int getStandardInputFDS();
    static int getStandardOutputFDS();
    static int getStandardErrorFDS();

    bool isValid() const { return fileDescriptor != InvalidFDS; }
    void resetAsInvalid() { fileDescriptor = InvalidFDS; }

  private:
    FileNativeDescriptor fileDescriptor = InvalidFDS;
};

struct SC::FileDescriptorPipe
{
    FileDescriptor readPipe;
    FileDescriptor writePipe;

    [[nodiscard]] ReturnCode createPipe();
    [[nodiscard]] bool       setCloseOnExec() { return readPipe.setCloseOnExec() && writePipe.setCloseOnExec(); }
};
