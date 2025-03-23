// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/Globals.h"
#if SC_PLATFORM_WINDOWS && SC_CONFIGURATION_DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>
#endif

struct SC::Globals::Internal
{
    struct DefaultAllocator : public MemoryAllocator
    {
        virtual void* allocateImpl(size_t numBytes) override { return ::malloc(numBytes); }
        virtual void* reallocateImpl(void* memory, size_t numBytes) override { return ::realloc(memory, numBytes); }
        virtual void  releaseImpl(void* memory) override { return ::free(memory); }
    };
    struct StaticGlobals
    {
        DefaultAllocator allocator;
        Globals          globals = {allocator};
        Globals*         current = &globals;
    };

    static Globals* getOrSet(bool threadLocal, Globals* globals = nullptr)
    {
        static StaticGlobals staticGlobals;
        if (threadLocal)
        {
            static thread_local StaticGlobals staticThreadLocals = staticGlobals;
            if (globals != nullptr)
            {
                staticThreadLocals.current = globals;
            }
            return staticThreadLocals.current;
        }
        else
        {
            if (globals != nullptr)
            {
                staticGlobals.current = globals;
            }
            return staticGlobals.current;
        }
    }

    static void setThreadLocal(Globals& globals) { Internal::getOrSet(true, &globals); }

    static void setGlobal(Globals& globals) { Internal::getOrSet(false, &globals); }
};

void SC::Globals::registerGlobals()
{
#if SC_PLATFORM_WINDOWS && SC_CONFIGURATION_DEBUG
    ::_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    // ::_CrtSetBreakAlloc();
#endif
}

void SC::Globals::pushGlobal(Globals& globals)
{
    Globals& current = getGlobal();
    globals.prev     = &current;
    Internal::setGlobal(globals);
}

void SC::Globals::popGlobal()
{
    Globals& current = getGlobal();
    if (current.prev != nullptr)
    {
        Internal::setGlobal(*current.prev);
        current.prev = nullptr;
    }
}

void SC::Globals::pushThreadLocal(Globals& globals)
{
    Globals& current = getThreadLocal();
    globals.prev     = &current;
    Internal::setThreadLocal(globals);
}

void SC::Globals::popThreadLocal()
{
    Globals& current = getThreadLocal();
    if (current.prev != nullptr)
    {
        Internal::setThreadLocal(*current.prev);
        current.prev = nullptr;
    }
}

SC::Globals& SC::Globals::getThreadLocal() { return *Internal::getOrSet(true); }
SC::Globals& SC::Globals::getGlobal() { return *Internal::getOrSet(false); }
