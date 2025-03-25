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
        DefaultAllocator defaultAllocator;

        Globals  globals = {defaultAllocator};
        Globals* current = &globals;
    };

    static StaticGlobals& getStatic(Globals::Type type)
    {
        // clang-format off
        if (type == Globals::Global) SC_LANGUAGE_LIKELY
        {
            static StaticGlobals staticGlobals;
            return staticGlobals;
        }
        else
        {
            static thread_local StaticGlobals staticThreadLocals;
            return staticThreadLocals;
        }
        // clang-format on
    }
};

void SC::Globals::init(Type type)
{
    (void)type;
#if SC_PLATFORM_WINDOWS && SC_CONFIGURATION_DEBUG
    if (type == Type::Global)
    {
        ::_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
        // ::_CrtSetBreakAlloc();
    }
#endif
}

SC::Globals* SC::Globals::push(Type type, Globals& newGlobals)
{
    auto& globals   = Internal::getStatic(type);
    newGlobals.prev = globals.current;
    globals.current = &newGlobals;
    return newGlobals.prev;
}

SC::Globals* SC::Globals::pop(Type type)
{
    auto&    globals = Internal::getStatic(type);
    Globals* prev    = globals.current->prev;
    if (prev != nullptr)
    {
        globals.current->prev = nullptr;
        globals.current       = prev;
    }
    return prev;
}

SC::Globals& SC::Globals::get(Type type) { return *Internal::getStatic(type).current; }
