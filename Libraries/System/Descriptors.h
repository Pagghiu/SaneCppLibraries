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

struct Descriptor;
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

struct SC::Descriptor
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
    enum AddressFamily
    {
        AddressFamilyIPV4,
        AddressFamilyIPV6,
    };
    [[nodiscard]] static AddressFamily AddressFamilyFromInt(int value);
    [[nodiscard]] static unsigned char toNative(AddressFamily family);

    enum SocketType
    {
        SocketStream,
        SocketDgram
    };
    [[nodiscard]] static SocketType SocketTypeFromInt(int value);
    [[nodiscard]] static int        toNative(SocketType family);

    enum ProtocolType
    {
        ProtocolTcp,
        ProtocolUdp,
    };
    [[nodiscard]] static ProtocolType ProtocolTypeFromInt(int value);
    [[nodiscard]] static int          toNative(ProtocolType family);
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

    [[nodiscard]] ReturnCode readUntilEOF(Vector<char_t>& destination);
    [[nodiscard]] ReturnCode readUntilEOF(String& destination);

    [[nodiscard]] Result<ReadResult> readAppend(Vector<char>& output, Span<char> fallbackBuffer);

  private:
    struct Internal;
};

struct SC::SocketDescriptor : public UniqueTaggedHandleTraits<SocketDescriptorTraits>
{
    [[nodiscard]] ReturnCode create(Descriptor::AddressFamily   addressFamily,
                                    Descriptor::SocketType      socketType  = Descriptor::SocketStream,
                                    Descriptor::ProtocolType    protocol    = Descriptor::ProtocolTcp,
                                    Descriptor::BlockingType    blocking    = Descriptor::Blocking,
                                    Descriptor::InheritableType inheritable = Descriptor::NonInheritable);
    [[nodiscard]] ReturnCode isInheritable(bool& value) const;
    [[nodiscard]] ReturnCode setInheritable(bool value);
    [[nodiscard]] ReturnCode setBlocking(bool value);
    [[nodiscard]] ReturnCode getAddressFamily(Descriptor::AddressFamily& addressFamily) const;
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
