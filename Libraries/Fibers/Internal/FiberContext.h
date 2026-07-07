// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../../Common/PlatformMacrosInstructionSet.h"
#include "../../Common/PlatformMacrosType.h"
#include "../../Common/Result.h"
#include "../Fibers.h"

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace SC
{
using FiberContextEntry = void (*)(void*);

#if SC_PLATFORM_WINDOWS
#if SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
struct FiberContextPlatform
{
    CONTEXT      context;
    volatile int restoring = 0;
};
#if SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL
#pragma warning(pop)
#endif
#elif SC_PLATFORM_INTEL && SC_PLATFORM_64_BIT
struct FiberContextPlatform
{
    void* rsp = nullptr;
    void* rbp = nullptr;
    void* rbx = nullptr;
    void* r12 = nullptr;
    void* r13 = nullptr;
    void* r14 = nullptr;
    void* r15 = nullptr;
};
#elif SC_PLATFORM_ARM64
struct FiberContextPlatform
{
    void* sp  = nullptr;
    void* x19 = nullptr;
    void* x20 = nullptr;
    void* x21 = nullptr;
    void* x22 = nullptr;
    void* x23 = nullptr;
    void* x24 = nullptr;
    void* x25 = nullptr;
    void* x26 = nullptr;
    void* x27 = nullptr;
    void* x28 = nullptr;
    void* x29 = nullptr;
    void* x30 = nullptr;

    uint64_t d8  = 0;
    uint64_t d9  = 0;
    uint64_t d10 = 0;
    uint64_t d11 = 0;
    uint64_t d12 = 0;
    uint64_t d13 = 0;
    uint64_t d14 = 0;
    uint64_t d15 = 0;
};
#else
#error "Unsupported Fibers context platform"
#endif

#if SC_PLATFORM_WINDOWS && (SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL)
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
struct SC_FIBERS_EXPORT FiberContext
{
    FiberContextPlatform platform;
    const void*          stackBottom   = nullptr;
    size_t               stackSize     = 0;
    void*                asanFakeStack = nullptr;
    bool                 initialized   = false;
};
#if SC_PLATFORM_WINDOWS && (SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL)
#pragma warning(pop)
#endif

struct SC_FIBERS_EXPORT FiberContextOperations
{
    static Result captureCurrent(FiberContext& context);
    static Result create(FiberContext& context, Span<char> stack, FiberContextEntry entry, void* userData);
    static void   switchTo(FiberContext& from, FiberContext& to);
};
} // namespace SC
