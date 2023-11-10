// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Result.h"
#include "../Foundation/Span.h"
#include "../Foundation/UniqueHandle.h"
#include "../Strings/StringView.h"

namespace SC
{
struct String;
template <typename T>
struct Vector;

struct DescriptorFlags;
struct FileDescriptor;
struct PipeDescriptor;

#if SC_PLATFORM_WINDOWS

struct FileDescriptorDefinition
{
    using Handle = void*; // HANDLE
    static Result releaseHandle(Handle& handle);
#ifdef __clang__
    static constexpr void* Invalid = __builtin_constant_p(-1) ? (void*)-1 : (void*)-1; // INVALID_HANDLE_VALUE
#else
    static constexpr void* Invalid = (void*)-1; // INVALID_HANDLE_VALUE
#endif
};

#else

struct FileDescriptorDefinition
{
    using Handle = int; // fd
    static Result releaseHandle(Handle& handle);

    static constexpr Handle Invalid = -1; // invalid fd
};

#endif

} // namespace SC

struct SC::FileDescriptor : public SC::UniqueHandle<SC::FileDescriptorDefinition>
{
    using UniqueHandle::UniqueHandle;
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

    [[nodiscard]] Result open(StringView path, OpenMode mode);
    [[nodiscard]] Result open(StringView path, OpenMode mode, OpenOptions options);

    [[nodiscard]] Result setBlocking(bool blocking);
    [[nodiscard]] Result setInheritable(bool inheritable);
    [[nodiscard]] Result isInheritable(bool& hasValue) const;

    [[nodiscard]] Result read(Span<char> data, Span<char>& actuallyRead, uint64_t offset);
    [[nodiscard]] Result read(Span<char> data, Span<char>& actuallyRead);

    [[nodiscard]] Result write(Span<const char> data, uint64_t offset);
    [[nodiscard]] Result write(Span<const char> data);

    enum SeekMode
    {
        SeekStart,
        SeekEnd,
        SeekCurrent,
    };

    [[nodiscard]] Result seek(SeekMode seekMode, uint64_t offset);

    // TODO: Maybe readUntilEOF and readAppend should go into some other class
    [[nodiscard]] Result readUntilEOF(Vector<char>& destination);
    [[nodiscard]] Result readUntilEOF(String& destination);

    struct ReadResult
    {
        size_t actuallyRead = 0;
        bool   isEOF        = false;
    };

    [[nodiscard]] Result readAppend(Vector<char>& output, Span<char> fallbackBuffer, ReadResult& result);

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
    [[nodiscard]] Result createPipe(InheritableReadFlag readFlag, InheritableWriteFlag writeFlag);
    [[nodiscard]] Result close();
};
