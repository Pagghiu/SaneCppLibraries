// Copyright (c) 2022-2023, Stefano Cristiano
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
extern "C" void __cxa_guard_abort(SC::uint64_t* guard_object) { SC_UNUSED(guard_object); }
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

// system includes
#include <float.h>  // FLT_MAX / DBL_MAX
#include <stdlib.h> // *_MAX (integer)
#if SC_MSVC
#include <BaseTsd.h>
#include <stdint.h>
typedef SSIZE_T ssize_t;
#else
#include <unistd.h> // ssize_t
#endif
namespace SC
{
static_assert(static_cast<float>(MaxValue()) == FLT_MAX, "static_cast<float>(MaxValue)");
static_assert(static_cast<double>(MaxValue()) == DBL_MAX, "static_cast<double>(MaxValue)");

static_assert(static_cast<uint8_t>(MaxValue()) == UINT8_MAX, "static_cast<uint8_t>(MaxValue)");
static_assert(static_cast<uint16_t>(MaxValue()) == UINT16_MAX, "static_cast<uint16_t>(MaxValue)");
static_assert(static_cast<uint32_t>(MaxValue()) == UINT32_MAX, "static_cast<uint32_t>(MaxValue)");
static_assert(static_cast<uint64_t>(MaxValue()) == UINT64_MAX, "static_cast<uint64_t>(MaxValue)");

static_assert(static_cast<int8_t>(MaxValue()) == INT8_MAX, "static_cast<int8_t>(MaxValue)");
static_assert(static_cast<int16_t>(MaxValue()) == INT16_MAX, "static_cast<int16_t>(MaxValue)");
static_assert(static_cast<int32_t>(MaxValue()) == INT32_MAX, "static_cast<int32_t>(MaxValue)");
static_assert(static_cast<int64_t>(MaxValue()) == INT64_MAX, "static_cast<int64_t>(MaxValue)");

static_assert(IsSame<uint8_t, ::uint8_t>::value, "uint8_t");
static_assert(IsSame<uint16_t, ::uint16_t>::value, "uint16_t");
static_assert(IsSame<uint32_t, ::uint32_t>::value, "uint32_t");
static_assert(IsSame<uint64_t, ::uint64_t>::value, "uint64_t");

static_assert(IsSame<int8_t, ::int8_t>::value, "int8_t");
static_assert(IsSame<int16_t, ::int16_t>::value, "int16_t");
static_assert(IsSame<int32_t, ::int32_t>::value, "int32_t");
static_assert(IsSame<int64_t, ::int64_t>::value, "int64_t");

static_assert(IsSame<size_t, ::size_t>::value, "size_t");
static_assert(IsSame<ssize_t, ::ssize_t>::value, "size_t");
} // namespace SC
