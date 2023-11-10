// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Foundation/AlignedStorage.h"
#include "../Foundation/Result.h"

namespace SC
{
struct ProcessDescriptor;

#if SC_PLATFORM_WINDOWS

struct ProcessDescriptorDefinition
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

struct ProcessDescriptorDefinition
{
    using Handle = int; // pid_t
    static Result releaseHandle(Handle& handle);

    static constexpr Handle Invalid = 0; // invalid pid_t
};

#endif
} // namespace SC

struct SC::ProcessDescriptor : public UniqueHandle<ProcessDescriptorDefinition>
{
    struct ExitStatus
    {
        int32_t status = -1;
    };
};
