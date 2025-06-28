// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/Assert.h"
#include "../../Memory/Globals.h"
#include "../../Memory/Memory.h"
#include "SortedAllocations.inl"
#if SC_PLATFORM_WINDOWS && SC_CONFIGURATION_DEBUG
#define _CRTDBG_MAP_ALLOC
#ifdef _malloca
#undef _malloca
#endif
#include <crtdbg.h>
#endif
#include <stdlib.h>

struct SC::Globals::Internal
{
    struct DefaultAllocator : public MemoryAllocator
    {
        // If needed all allocations (address, length) are being tracked by this sorted array.
        // This enables erroring out when Segments placed in the space managed by other allocators try
        // to request an allocation from this one
        SortedAllocations* allocations = nullptr;

        ~DefaultAllocator()
        {
            if (allocations)
            {
                ::free(allocations);
            }
        }

        void reserveForSortedAllocations(size_t memorySize)
        {
            allocations = reinterpret_cast<SortedAllocations*>(::malloc(memorySize));
            SortedAllocations::init(allocations, memorySize);
        }

        virtual void* allocateImpl(const void*, size_t numBytes, size_t alignment) override
        {
            (void)alignment; // TODO: Enforce alignment
            void* result = ::malloc(numBytes);
            if (result != nullptr and allocations != nullptr)
            {
                if (not allocations->insertSorted({result, numBytes}))
                {
                    ::free(result);
                    return nullptr;
                }
            }
            return result;
        }

        virtual void* reallocateImpl(void* memory, size_t numBytes) override
        {
            if (allocations != nullptr)
            {
                if (not allocations->removeSorted(memory))
                {
                    return nullptr;
                }
            }
            void* result = ::realloc(memory, numBytes);
            if (allocations != nullptr)
            {
                (void)allocations->insertSorted({result, numBytes});
            }
            return result;
        }

        virtual void releaseImpl(void* memory) override
        {
            if (memory != nullptr and allocations != nullptr)
            {
                SC_ASSERT_RELEASE(allocations->removeSorted(memory));
            }
            return ::free(memory);
        }
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

void SC::Globals::init(Type type, GlobalSettings settings)
{
    if (settings.ownershipTrackingBytes > sizeof(SortedAllocations))
    {
        Internal::DefaultAllocator& defaultAllocator = Internal::getStatic(type).defaultAllocator;
        defaultAllocator.reserveForSortedAllocations(settings.ownershipTrackingBytes);
    }
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
        Globals* current = globals.current;
        current->prev    = nullptr;
        globals.current  = prev;
        return current;
    }
    return nullptr;
}

SC::Globals& SC::Globals::get(Type type) { return *Internal::getStatic(type).current; }
