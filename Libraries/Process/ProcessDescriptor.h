// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Foundation/Opaque.h"
#include "../Foundation/Optional.h"
#include "../Foundation/Result.h"

namespace SC
{
struct ProcessDescriptor;

#if SC_PLATFORM_WINDOWS

struct ProcessDescriptorTraits
{
    using Handle                    = void*;      // HANDLE
    static constexpr Handle Invalid = (Handle)-1; // INVALID_HANDLE_VALUE
    static ReturnCode       releaseHandle(Handle& handle);
};

#else

struct ProcessDescriptorTraits
{
    using Handle                    = int; // pid_t
    static constexpr Handle Invalid = 0;   // invalid pid_t
    static ReturnCode       releaseHandle(Handle& handle);
};

#endif
} // namespace SC

struct SC::ProcessDescriptor : public UniqueTaggedHandleTraits<ProcessDescriptorTraits>
{
    struct ExitStatus
    {
        Optional<int32_t> status;
    };
};
