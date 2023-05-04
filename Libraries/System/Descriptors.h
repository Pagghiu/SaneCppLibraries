// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Opaque.h"
#include "../Foundation/Optional.h"
#include "../Foundation/Result.h"

namespace SC
{
struct String;
template <typename T>
struct Vector;

struct DescriptorFlags;
struct FileDescriptor;
struct PipeDescriptor;

#if SC_PLATFORM_WINDOWS

struct FileDescriptorTraits
{
    using Handle                    = void*;      // HANDLE
    static constexpr Handle Invalid = (Handle)-1; // INVALID_HANDLE_VALUE
    static ReturnCode       releaseHandle(Handle& handle);
};

#else

struct FileDescriptorTraits
{
    using Handle                    = int; // fd
    static constexpr Handle Invalid = -1;  // invalid fd
    static ReturnCode       releaseHandle(Handle& handle);
};

#endif

} // namespace SC

struct SC::DescriptorFlags
{
    enum BlockingType
    {
        NonBlocking,
        Blocking
    };
    enum InheritableType
    {
        NonInheritable,
        Inheritable
    };
};

struct SC::FileDescriptor : public SC::UniqueTaggedHandleTraits<SC::FileDescriptorTraits>
{
    struct ReadResult
    {
        size_t actuallyRead = 0;
        bool   isEOF        = false;
    };

    [[nodiscard]] ReturnCode setBlocking(bool blocking);
    [[nodiscard]] ReturnCode setInheritable(bool inheritable);
    [[nodiscard]] ReturnCode isInheritable(bool& hasValue) const;

    [[nodiscard]] ReturnCode readUntilEOF(Vector<char_t>& destination);
    [[nodiscard]] ReturnCode readUntilEOF(String& destination);

    [[nodiscard]] Result<ReadResult> readAppend(Vector<char>& output, Span<char> fallbackBuffer);

  private:
    struct Internal;
};

struct SC::PipeDescriptor
{
    enum InheritableReadFlag
    {
        ReadInheritable,
        ReadNonInheritable
    };
    enum InheritableWriteFlag
    {
        WriteInheritable,
        WriteNonInheritable
    };
    FileDescriptor readPipe;
    FileDescriptor writePipe;

    /// Creates a Pipe. Default is non-inheritable / blocking
    [[nodiscard]] ReturnCode createPipe(InheritableReadFlag readFlag, InheritableWriteFlag writeFlag);
    [[nodiscard]] ReturnCode close();
};
