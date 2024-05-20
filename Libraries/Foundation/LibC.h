// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/PrimitiveTypes.h"
#if SC_COMPILER_ENABLE_STD_CPP || SC_LANGUAGE_EXCEPTIONS
#include <memory.h>
#else
#if SC_PLATFORM_WINDOWS
extern "C"
{
    void* __cdecl memmove(void* dst, const void* src, size_t n);
    void* __cdecl memcpy(void* dst, const void* src, size_t n);
    int __cdecl memcmp(const void* s1, const void* s2, size_t n);
    void* __cdecl memset(void* dst, SC::int32_t c, size_t len);
    [[nodiscard]] void const* __cdecl memchr(const void* ptr, SC::int32_t c, size_t count);
}
#elif SC_PLATFORM_APPLE
extern "C"
{
    void* memmove(void* dst, const void* src, SC::size_t n);
    void* memcpy(void* dst, const void* src, SC::size_t n);
    int   memcmp(const void* s1, const void* s2, SC::size_t n);
    void* memset(void* dst, SC::int32_t c, SC::size_t len);
    void* memchr(const void* ptr, SC::int32_t c, SC::size_t count);
}
#elif SC_PLATFORM_LINUX
extern "C"
{
    void* memmove(void* dst, const void* src, SC::size_t n);
    void* memcpy(void* dst, const void* src, SC::size_t n);
    int   memcmp(const void* s1, const void* s2, SC::size_t n);
    void* memset(void* dst, SC::int32_t c, SC::size_t len);
}
extern "C++"
{
    extern const void* memchr(const void* __s, int __c, SC::size_t __n) __asm("memchr");
}
#elif SC_PLATFORM_EMSCRIPTEN
#else
#error "Unsupported platform"
#endif
#endif // SC_SAFE_INCLUDES
