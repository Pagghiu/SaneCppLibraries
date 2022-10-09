#include "Memory.h"
#include "Assert.h"
#include "Limits.h"

// system includes
#include <memory.h> // memcmp
#include <stdlib.h> // malloc

void* SC::memoryReallocate(void* memory, SC::size_t numBytes) { return realloc(memory, numBytes); }
void* SC::memoryAllocate(SC::size_t numBytes) { return malloc(numBytes); }
void  SC::memoryRelease(void* allocatedMemory) { return free(allocatedMemory); }

void* operator new(SC::size_t len) { return malloc(len); }
void* operator new[](SC::size_t len) { return malloc(len); }
void  operator delete(void* p) noexcept
{
    if (p != 0) [[likely]]
        free(p);
}
void operator delete[](void* p) noexcept
{
    if (p != 0) [[likely]]
        free(p);
}
void*           __cxa_pure_virtual = 0;
extern "C" int  __cxa_guard_acquire(uint64_t* guard_object) { return 0; }
extern "C" void __cxa_guard_release(uint64_t* guard_object) {}
