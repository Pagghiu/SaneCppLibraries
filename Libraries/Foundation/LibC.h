// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/PrimitiveTypes.h"

extern "C"
{
#if SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL
    void* __cdecl memmove(void* dst, const void* src, size_t n);
    void* __cdecl memcpy(void* dst, const void* src, size_t n);
    int __cdecl memcmp(const void* s1, const void* s2, size_t n);
    void* __cdecl memset(void* dst, SC::int32_t c, size_t len);
    [[nodiscard]] void const* __cdecl memchr(const void* ptr, SC::int32_t c, size_t count);
#else
    void* memmove(void* dst, const void* src, SC::size_t n);
    void* memcpy(void* dst, const void* src, SC::size_t n);
    int   memcmp(const void* s1, const void* s2, SC::size_t n);
    void* memset(void* dst, SC::int32_t c, SC::size_t len);
    void* memchr(const void* ptr, SC::int32_t c, SC::size_t count);
#endif
    // string
}
