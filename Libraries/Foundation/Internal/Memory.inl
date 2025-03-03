// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/Memory.h"

void* SC::Memory::reallocate(void* memory, size_t numBytes) { return ::realloc(memory, numBytes); }
void* SC::Memory::allocate(size_t numBytes) { return ::malloc(numBytes); }
void  SC::Memory::release(void* allocatedMemory) { return ::free(allocatedMemory); }
#if SC_PLATFORM_WINDOWS && SC_CONFIGURATION_DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>
void SC::Memory::registerGlobals()
{
    ::_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    // ::_CrtSetBreakAlloc();
}
#else
void SC::Memory::registerGlobals() {}
#endif
