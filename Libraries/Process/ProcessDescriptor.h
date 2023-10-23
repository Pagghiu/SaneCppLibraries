// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Foundation/Opaque.h"
#include "../Foundation/Result.h"

namespace SC
{
struct ProcessDescriptor;

#if SC_PLATFORM_WINDOWS

struct ProcessDescriptorTraits
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

struct ProcessDescriptorTraits
{
    using Handle                    = int; // pid_t
    static constexpr Handle Invalid = 0;   // invalid pid_t
    static Result           releaseHandle(Handle& handle);
};

#endif
} // namespace SC

struct SC::ProcessDescriptor : public UniqueTaggedHandleTraits<ProcessDescriptorTraits>
{
    struct ExitStatus
    {
        int32_t status = -1;
    };
};
