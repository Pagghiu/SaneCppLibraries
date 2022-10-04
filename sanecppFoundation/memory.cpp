#include "memory.h"
#include "assert.h"
#include "limits.h"

// system includes
#include <memory.h> // memcmp
#include <stdlib.h> // malloc

void* sanecpp::memoryReallocate(void* memory, sanecpp::size_t numBytes) { return realloc(memory, numBytes); }
void* sanecpp::memoryAllocate(sanecpp::size_t numBytes) { return malloc(numBytes); }
void  sanecpp::memoryRelease(void* allocatedMemory) { return free(allocatedMemory); }

void* operator new(sanecpp::size_t len) { return malloc(len); }
void* operator new[](sanecpp::size_t len) { return malloc(len); }
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
