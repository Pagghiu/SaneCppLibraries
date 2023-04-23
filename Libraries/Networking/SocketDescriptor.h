// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Foundation/Opaque.h"

namespace SC
{
#if SC_PLATFORM_WINDOWS
using SocketDescriptorNative                                          = uint64_t; // SOCKET
static constexpr SocketDescriptorNative SocketDescriptorNativeInvalid = ~0ull;    // INVALID_SOCKET
#else
using SocketDescriptorNative                                          = int; // Posix FD
static constexpr SocketDescriptorNative SocketDescriptorNativeInvalid = -1;  // Posix Invalid FD
#endif
ReturnCode SocketDescriptorNativeClose(SocketDescriptorNative&);
struct SocketDescriptorNativeHandle : public UniqueTaggedHandle<SocketDescriptorNative, SocketDescriptorNativeInvalid,
                                                                ReturnCode, SocketDescriptorNativeClose>
{
};
} // namespace SC
