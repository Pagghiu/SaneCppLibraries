// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Memory.h"
#include "Assert.h"
#include "Language.h"
#include "Limits.h"

// system includes
#include <memory.h> // memcmp
#include <stdlib.h> // malloc

void* SC::memoryReallocate(void* memory, SC::size_t numBytes) { return realloc(memory, numBytes); }
void* SC::memoryAllocate(SC::size_t numBytes) { return malloc(numBytes); }
void  SC::memoryRelease(void* allocatedMemory) { return free(allocatedMemory); }

#ifndef SC_ENABLE_STD_CPP_LIBRARY
#error "SC_ENABLE_STD_CPP_LIBRARY must be defined to either 0 or 1"
#endif

#if SC_MSVC
void* operator new(size_t len) { return malloc(len); }
void* operator new[](size_t len) { return malloc(len); }
#else
void* operator new(SC::size_t len) { return malloc(len); }
void* operator new[](SC::size_t len) { return malloc(len); }
#if !SC_ENABLE_STD_CPP_LIBRARY
void* __cxa_pure_virtual   = 0;
void* __gxx_personality_v0 = 0;

// helper functions for getting/setting flags in guard_object
static bool initializerHasRun(SC::uint64_t* guard_object) { return (*((SC::uint8_t*)guard_object) != 0); }

static void setInitializerHasRun(SC::uint64_t* guard_object) { *((SC::uint8_t*)guard_object) = 1; }

// We're stripping away all locking from static initialization because if one relies on multithreaded order of
// static initialization for your program to function properly, it's a good idea to encourage it to fail/crash
// so one can fix it with some sane code.
extern "C" int __cxa_guard_acquire(SC::uint64_t* guard_object)
{
    if (initializerHasRun(guard_object))
        return 0;
    return 1;
}

extern "C" void __cxa_guard_release(SC::uint64_t* guard_object) { setInitializerHasRun(guard_object); }
extern "C" void __cxa_guard_abort(SC::uint64_t* guard_object) {}
#endif
#endif

void operator delete(void* p) noexcept
{
    if (p != 0)
        SC_LIKELY { free(p); }
}
void operator delete[](void* p) noexcept
{
    if (p != 0)
        SC_LIKELY { free(p); }
}
