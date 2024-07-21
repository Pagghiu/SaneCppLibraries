// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Foundation/Result.h"
#include "../Foundation/UniqueHandle.h"

namespace SC
{
struct SC_COMPILER_EXPORT ProcessDescriptor;
namespace detail
{
#if SC_PLATFORM_WINDOWS

struct SC_COMPILER_EXPORT ProcessDescriptorDefinition
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

struct SC_COMPILER_EXPORT ProcessDescriptorDefinition
{
    using Handle = int; // pid_t
    static Result releaseHandle(Handle& handle);

    static constexpr Handle Invalid = 0; // invalid pid_t
};

#endif
} // namespace detail
} // namespace SC

/// @brief Wraps an OS Process descriptor
struct SC::ProcessDescriptor : public UniqueHandle<detail::ProcessDescriptorDefinition>
{
    struct ExitStatus
    {
        int32_t status = -1;
    };
};
