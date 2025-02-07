// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Foundation/Assert.inl"
#include "../Foundation/Limits.h"
#include "../Foundation/Memory.h"

// system includes
#include <float.h>  // FLT_MAX / DBL_MAX
#include <stdint.h> // Linux *_{MAX | MIN}

#if SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL
#include <BaseTsd.h>
using ssize_t = SSIZE_T;
#endif

//--------------------------------------------------------------------
// Buffer
//--------------------------------------------------------------------
#include "../Foundation/Buffer.h"
#include "../Foundation/Segment.inl"

void SC::SegmentTrivial::destruct(SegmentHeader&, size_t, size_t) {}

void SC::SegmentTrivial::copyConstructSingle(SegmentHeader& header, size_t offsetBytes, const void* value,
                                             size_t numBytes, size_t valueSize)
{
    if (valueSize == 1)
    {
        int intValue = 0;
        ::memcpy(&intValue, value, 1);
        ::memset(header.getData<char>() + offsetBytes, intValue, numBytes);
    }
    else
    {
        char* data = header.getData<char>();
        for (size_t idx = offsetBytes; idx < offsetBytes + numBytes; idx += valueSize)
        {
            ::memcpy(data + idx, value, valueSize);
        }
    }
}

void SC::SegmentTrivial::copyConstruct(SegmentHeader& header, size_t offsetBytes, const void* src, size_t numBytes)
{
    ::memmove(header.getData<char>() + offsetBytes, src, numBytes);
}

void SC::SegmentTrivial::copyAssign(SegmentHeader& dest, size_t bytesOffset, const void* src, size_t numBytes)
{
    ::memcpy(dest.getData<char>() + bytesOffset, src, numBytes);
}

void SC::SegmentTrivial::copyInsert(SegmentHeader& dest, size_t bytesOffset, const void* src, size_t numBytes)
{
    char* data = dest.getData<char>();
    ::memmove(data + bytesOffset + numBytes, data + bytesOffset, dest.sizeBytes - bytesOffset);
    ::memmove(data + bytesOffset, src, numBytes);
}

void SC::SegmentTrivial::moveConstruct(SegmentHeader& dest, size_t bytesOffset, void* src, size_t numBytes)
{
    ::memcpy(dest.getData<char>() + bytesOffset, src, numBytes);
}

void SC::SegmentTrivial::moveAssign(SegmentHeader& dest, size_t bytesOffset, void* src, size_t numBytes)
{
    ::memcpy(dest.getData<char>() + bytesOffset, src, numBytes);
}

void SC::SegmentTrivial::remove(SegmentHeader& dest, size_t fromBytesOffset, size_t toBytesOffset)
{
    char* data = dest.getData<char>();
    ::memmove(data + fromBytesOffset, data + toBytesOffset, dest.sizeBytes - toBytesOffset);
}

SC::SegmentHeader* SC::SegmentAllocator::allocateNewHeader(size_t newCapacityInBytes)
{
    return reinterpret_cast<SegmentHeader*>(Memory::allocate(sizeof(SegmentHeader) + newCapacityInBytes));
}

SC::SegmentHeader* SC::SegmentAllocator::reallocateExistingHeader(SegmentHeader& src, size_t newCapacityInBytes)
{
    return reinterpret_cast<SegmentHeader*>(Memory::reallocate(&src, sizeof(SegmentHeader) + newCapacityInBytes));
}

void SC::SegmentAllocator::destroyHeader(SegmentHeader& header) { Memory::release(&header); }

namespace SC
{
template struct Segment<SegmentBuffer>;
} // namespace SC

//--------------------------------------------------------------------
// Memory
//--------------------------------------------------------------------
void* SC::Memory::reallocate(void* memory, size_t numBytes) { return ::realloc(memory, numBytes); }
void* SC::Memory::allocate(size_t numBytes) { return ::malloc(numBytes); }
void  SC::Memory::release(void* allocatedMemory) { return ::free(allocatedMemory); }

//--------------------------------------------------------------------
// Standard C++ Library support
//--------------------------------------------------------------------
#if not SC_COMPILER_ENABLE_STD_CPP and (not SC_COMPILER_MSVC or not SC_COMPILER_CLANG_CL)
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
#endif

#if SC_COMPILER_ASAN == 0
void* operator new(size_t len) { return ::malloc(len); }
void* operator new[](size_t len) { return ::malloc(len); }
#endif

void* __cxa_pure_virtual   = 0;
void* __gxx_personality_v0 = 0;

// We're stripping away all locking from static initialization because if one relies on multithreaded order of
// static initialization for your program to function properly, it's a good idea to encourage it to fail/crash
//// so one can fix it with some sane code.
extern "C" int __cxa_guard_acquire(uint64_t* guard_object)
{
    if (*reinterpret_cast<const uint8_t*>(guard_object) != 0)
        return 0;
    return 1;
}

extern "C" void __cxa_guard_release(uint64_t* guard_object) { *reinterpret_cast<uint8_t*>(guard_object) = 1; }
extern "C" void __cxa_guard_abort(uint64_t* guard_object) { SC_COMPILER_UNUSED(guard_object); }
#endif

//--------------------------------------------------------------------
// Limits
//--------------------------------------------------------------------
namespace SC
{
template <typename T, typename U>
struct CheckSameType
{
    static constexpr bool value = false;
};
template <typename T>
struct CheckSameType<T, T>
{
    static constexpr bool value = true;
};

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

static_assert(CheckSameType<uint8_t, ::uint8_t>::value, "uint8_t");
static_assert(CheckSameType<uint16_t, ::uint16_t>::value, "uint16_t");
static_assert(CheckSameType<uint32_t, ::uint32_t>::value, "uint32_t");
static_assert(CheckSameType<uint64_t, ::uint64_t>::value, "uint64_t");

static_assert(CheckSameType<int8_t, ::int8_t>::value, "int8_t");
static_assert(CheckSameType<int16_t, ::int16_t>::value, "int16_t");
static_assert(CheckSameType<int32_t, ::int32_t>::value, "int32_t");
static_assert(CheckSameType<int64_t, ::int64_t>::value, "int64_t");

static_assert(CheckSameType<size_t, ::size_t>::value, "size_t");
static_assert(CheckSameType<ssize_t, ::ssize_t>::value, "size_t");
} // namespace SC
