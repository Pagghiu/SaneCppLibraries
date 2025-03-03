// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/Compiler.h"
#include <stdlib.h> // malloc / free
#if SC_COMPILER_ASAN == 0
void operator delete(void* p) noexcept
{
    if (p != 0)
        SC_LANGUAGE_LIKELY { ::free(p); }
}
void operator delete[](void* p) noexcept
{
    if (p != 0)
        SC_LANGUAGE_LIKELY { ::free(p); }
}
void operator delete(void* p, size_t) noexcept
{
    if (p != 0)
        SC_LANGUAGE_LIKELY { ::free(p); }
}
void operator delete[](void* p, size_t) noexcept
{
    if (p != 0)
        SC_LANGUAGE_LIKELY { ::free(p); }
}

void* operator new(size_t len) { return ::malloc(len); }
void* operator new[](size_t len) { return ::malloc(len); }
#endif

void* __cxa_pure_virtual   = 0;
void* __gxx_personality_v0 = 0;

// We're stripping away all locking from static initialization because if one relies on multithreaded order of
// static initialization for your program to function properly, it's a good idea to encourage it to fail/crash
// so one can fix it with some sane code.
extern "C" int __cxa_guard_acquire(uint64_t* guard_object)
{
    if (*reinterpret_cast<const uint8_t*>(guard_object) != 0)
        return 0;
    return 1;
}

extern "C" void __cxa_guard_release(uint64_t* guard_object) { *reinterpret_cast<uint8_t*>(guard_object) = 1; }
extern "C" void __cxa_guard_abort(uint64_t* guard_object) { SC_COMPILER_UNUSED(guard_object); }
