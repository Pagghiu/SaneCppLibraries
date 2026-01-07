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
#if SC_PLATFORM_WINDOWS && SC_CONFIGURATION_DEBUG
#pragma warning(push)
#pragma warning(disable : 4566) // malloc_dbg macro uses __FILE__ instead of a wide version
#endif
void* operator new(size_t len) { return ::malloc(len); }
void* operator new[](size_t len) { return ::malloc(len); }
#if SC_PLATFORM_WINDOWS && SC_CONFIGURATION_DEBUG
#pragma warning(pop)
#endif
#endif

void* __cxa_pure_virtual   = 0;
void* __gxx_personality_v0 = 0;

using guard_type = long long int;
// We're stripping away all locking from static initialization because if one relies on multithreaded order of
// static initialization for your program to function properly, it's a good idea to encourage it to fail/crash
// so one can fix it with some sane code.
extern "C" int __cxa_guard_acquire(guard_type* guard_object)
{
    if (*reinterpret_cast<const uint8_t*>(guard_object) != 0)
        return 0;
    return 1;
}

extern "C" void __cxa_guard_release(guard_type* guard_object) { *reinterpret_cast<uint8_t*>(guard_object) = 1; }
extern "C" void __cxa_guard_abort(guard_type* guard_object) { SC_COMPILER_UNUSED(guard_object); }
#if SC_PLATFORM_LINUX
extern "C" int __cxa_thread_atexit(void (*func)(), void* obj, void* dso_symbol)
{
    int __cxa_thread_atexit_impl(void (*)(), void*, void*);
    return __cxa_thread_atexit_impl(func, obj, dso_symbol);
}
#endif
