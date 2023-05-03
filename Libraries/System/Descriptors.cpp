// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Descriptors.h"

#if SC_PLATFORM_WINDOWS
#include "DescriptorsInternalWindows.inl"
#else
#include "DescriptorsInternalPosix.inl"
#endif

#include "../Foundation/String.h"

SC::ReturnCode SC::PipeDescriptor::close()
{
    SC_TRY_IF(readPipe.close());
    return writePipe.close();
}

SC::ReturnCode SC::SocketDescriptor::getAddressFamily(Descriptor::AddressFamily& addressFamily) const
{

    struct sockaddr_in6 socketInfo;
    socklen_t           socketInfoLen = sizeof(socketInfo);

    if (::getsockname(handle, reinterpret_cast<struct sockaddr*>(&socketInfo), &socketInfoLen) == SOCKET_ERROR)
    {
        return "getsockname failed"_a8;
    }
    addressFamily = Descriptor::AddressFamilyFromInt(socketInfo.sin6_family);
    return true;
}

SC::ReturnCode SC::FileDescriptor::readUntilEOF(Vector<char_t>& destination)
{
    char buffer[1024];
    SC_TRY_IF(isValid());
    ReadResult readResult;
    while (not readResult.isEOF)
    {
        SC_TRY(readResult, readAppend(destination, {buffer, sizeof(buffer)}));
    }
    return true;
}

SC::ReturnCode SC::FileDescriptor::readUntilEOF(String& destination)
{
    SC_TRY_IF(readUntilEOF(destination.data));
    return destination.pushNullTerm();
}

SC::Descriptor::AddressFamily SC::Descriptor::AddressFamilyFromInt(int value)
{
    switch (value)
    {
    case AF_INET: return Descriptor::AddressFamilyIPV4;
    case AF_INET6: return Descriptor::AddressFamilyIPV6;
    }
    SC_UNREACHABLE();
}

unsigned char SC::Descriptor::toNative(Descriptor::AddressFamily type)
{
    switch (type)
    {
    case Descriptor::AddressFamilyIPV4: return AF_INET;
    case Descriptor::AddressFamilyIPV6: return AF_INET6;
    }
    SC_UNREACHABLE();
}

SC::Descriptor::SocketType SC::Descriptor::SocketTypeFromInt(int value)
{
    switch (value)
    {
    case SOCK_STREAM: return SocketStream;
    case SOCK_DGRAM: return SocketDgram;
    }
    SC_UNREACHABLE();
}
int SC::Descriptor::toNative(SocketType type)
{
    switch (type)
    {
    case SocketStream: return SOCK_STREAM;
    case SocketDgram: return SOCK_DGRAM;
    }
    SC_UNREACHABLE();
}

SC::Descriptor::ProtocolType SC::Descriptor::ProtocolTypeFromInt(int value)
{
    switch (value)
    {
    case IPPROTO_TCP: return ProtocolTcp;
    case IPPROTO_UDP: return ProtocolUdp;
    }
    SC_UNREACHABLE();
}

int SC::Descriptor::toNative(ProtocolType family)
{
    switch (family)
    {
    case ProtocolTcp: return IPPROTO_TCP;
    case ProtocolUdp: return IPPROTO_UDP;
    }
    SC_UNREACHABLE();
}
