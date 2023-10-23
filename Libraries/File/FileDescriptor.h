// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Opaque.h"
#include "../Foundation/Result.h"
#include "../Foundation/Span.h"
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

struct FileDescriptorTraits
{
    using Handle = void*; // HANDLE
#ifdef __clang__
    static constexpr void* Invalid = __builtin_constant_p(-1) ? (void*)-1 : (void*)-1; // INVALID_HANDLE_VALUE
#else
    static constexpr void* Invalid = (void*)-1; // INVALID_HANDLE_VALUE
#endif
    static Result releaseHandle(Handle& handle);
};

#else

struct FileDescriptorTraits
{
    using Handle                    = int; // fd
    static constexpr Handle Invalid = -1;  // invalid fd
    static Result           releaseHandle(Handle& handle);
};

#endif

} // namespace SC

struct SC::FileDescriptor : public SC::UniqueTaggedHandleTraits<SC::FileDescriptorTraits>
{
    using UniqueTaggedHandleTraits::UniqueTaggedHandleTraits;
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
