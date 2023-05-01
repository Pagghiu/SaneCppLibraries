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

struct FileDescriptor;
struct SocketDescriptor;
struct ProcessDescriptor;
struct PipeDescriptor;

#if SC_PLATFORM_WINDOWS

struct FileDescriptorTraits
{
    using Handle                    = void*;      // HANDLE
    static constexpr Handle Invalid = (Handle)-1; // INVALID_HANDLE_VALUE
    static ReturnCode       releaseHandle(Handle& handle);
};

struct SocketDescriptorTraits
{
    using Handle                    = uint64_t; // SOCKET
    static constexpr Handle Invalid = ~0ull;    // INVALID_SOCKET
    static ReturnCode       releaseHandle(Handle& handle);
};

struct ProcessDescriptorTraits
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

struct SocketDescriptorTraits
{
    using Handle                    = int; // fd
    static constexpr Handle Invalid = -1;  // invalid fd
    static ReturnCode       releaseHandle(Handle& handle);
};

struct ProcessDescriptorTraits
{
    using Handle                    = int; // pid_t
    static constexpr Handle Invalid = 0;   // invalid pid_t
    static ReturnCode       releaseHandle(Handle& handle);
};

#endif

} // namespace SC

struct SC::FileDescriptor : public SC::UniqueTaggedHandleTraits<SC::FileDescriptorTraits>
{
    struct ReadResult
    {
        size_t actuallyRead = 0;
        bool   isEOF        = false;
    };

    [[nodiscard]] ReturnCode setBlocking(bool blocking);
    [[nodiscard]] ReturnCode setInheritable(bool inheritable);

    [[nodiscard]] ReturnCode readUntilEOF(Vector<char_t>& destination);
    [[nodiscard]] ReturnCode readUntilEOF(String& destination);

    [[nodiscard]] Result<ReadResult> readAppend(Vector<char>& output, Span<char> fallbackBuffer);

  private:
    struct Internal;
};

struct SC::SocketDescriptor : public UniqueTaggedHandleTraits<SocketDescriptorTraits>
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
    enum IPType
    {
        IPTypeV4,
        IPTypeV6,
    };
    enum Protocol
    {
        ProtocolTcp,
    };
    [[nodiscard]] ReturnCode create(IPType ipType, Protocol protocol, BlockingType blocking = NonBlocking,
                                    InheritableType inheritable = NonInheritable);
    [[nodiscard]] ReturnCode isInheritable(bool& value) const;
    [[nodiscard]] ReturnCode setInheritable(bool value);
    [[nodiscard]] ReturnCode setBlocking(bool value);
};

struct SC::ProcessDescriptor : public UniqueTaggedHandleTraits<ProcessDescriptorTraits>
{
    struct ExitStatus
    {
        Optional<int32_t> status;
    };
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
