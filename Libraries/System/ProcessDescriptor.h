// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Opaque.h"
#include "../Foundation/Optional.h"
#include "../Foundation/Result.h"

namespace SC
{
struct ProcessID
{
    int32_t pid = 0;
};

struct ProcessExitStatus
{
    Optional<int32_t> status;
};
#if SC_PLATFORM_WINDOWS
using ProcessNative                                 = void*;   // HANDLE
static constexpr ProcessNative ProcessNativeInvalid = nullptr; // INVALID_HANDLE_VALUE
#else
using ProcessNative                                 = int; // pid_t
static constexpr ProcessNative ProcessNativeInvalid = 0;
#endif
ReturnCode ProcessNativeHandleClose(ProcessNative& handle);
struct ProcessNativeHandle
    : public UniqueTaggedHandle<ProcessNative, ProcessNativeInvalid, ReturnCode, &ProcessNativeHandleClose>
{
};
} // namespace SC
