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

struct SC::FileDescriptor : public SC::UniqueTaggedHandleTraits<SC::FileDescriptorTraits>
{
    enum OpenMode
    {
        ReadOnly,
        WriteCreateTruncate,
        WriteAppend,
        ReadAndWrite
    };

    struct OpenOptions
    {
        bool inheritable = false;
        bool blocking    = true;
        bool async       = false;
    };

    [[nodiscard]] ReturnCode open(StringView path, OpenMode mode);
    [[nodiscard]] ReturnCode open(StringView path, OpenMode mode, OpenOptions options);

    [[nodiscard]] ReturnCode setBlocking(bool blocking);
    [[nodiscard]] ReturnCode setInheritable(bool inheritable);
    [[nodiscard]] ReturnCode isInheritable(bool& hasValue) const;

    [[nodiscard]] ReturnCode read(Span<char> data, Span<char>& actuallyRead, uint64_t offset);
    [[nodiscard]] ReturnCode read(Span<char> data, Span<char>& actuallyRead);

    [[nodiscard]] ReturnCode write(Span<const char> data, uint64_t offset);
    [[nodiscard]] ReturnCode write(Span<const char> data);

    enum SeekMode
    {
        SeekStart,
        SeekEnd,
        SeekCurrent,
    };

    [[nodiscard]] ReturnCode seek(SeekMode seekMode, uint64_t offset);

    // TODO: Maybe readUntilEOF and readAppend should go into some other class
    [[nodiscard]] ReturnCode readUntilEOF(Vector<char_t>& destination);
    [[nodiscard]] ReturnCode readUntilEOF(String& destination);

    struct ReadResult
    {
        size_t actuallyRead = 0;
        bool   isEOF        = false;
    };

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