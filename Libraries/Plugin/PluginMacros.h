// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Base/Memory.h"
#include "../Foundation/Base/Types.h"

//
// SC_PLUGIN_EXPORT
//

#if SC_COMPILER_MSVC
#define SC_PLUGIN_EXPORT __declspec(dllexport)
#else
#define SC_PLUGIN_EXPORT
#endif

#if SC_PLUGIN_LIBRARY
#define SC_PLUGIN_LINKER_OPERATORS                                                                                     \
    void* operator new(SC::size_t len)                                                                                 \
    {                                                                                                                  \
        return SC::Memory::allocate(len);                                                                              \
    }                                                                                                                  \
    void* operator new[](SC::size_t len)                                                                               \
    {                                                                                                                  \
        return SC::Memory::allocate(len);                                                                              \
    }                                                                                                                  \
    void operator delete(void* p, SC::uint64_t) noexcept                                                               \
    {                                                                                                                  \
        if (p != 0)                                                                                                    \
        {                                                                                                              \
            SC::Memory::release(p);                                                                                    \
        }                                                                                                              \
    }                                                                                                                  \
    void operator delete(void* p) noexcept                                                                             \
    {                                                                                                                  \
        if (p != 0)                                                                                                    \
        {                                                                                                              \
            SC::Memory::release(p);                                                                                    \
        }                                                                                                              \
    }

#if SC_COMPILER_MSVC
#define SC_PLUGIN_LINKER_DEFINITIONS                                                                                   \
    SC_PLUGIN_LINKER_OPERATORS                                                                                         \
    extern "C" void __CxxFrameHandler4()                                                                               \
    {                                                                                                                  \
    }                                                                                                                  \
    extern "C" void __CxxFrameHandler3()                                                                               \
    {                                                                                                                  \
    }                                                                                                                  \
    int __stdcall DllMain(void*, unsigned int, void*)                                                                  \
    {                                                                                                                  \
        return 1;                                                                                                      \
    }
#else
// Cannot use builtin like __builtin_bzero as they will generate an infinite loop
// We also use inline asm to disable optimizations
// See: https://nullprogram.com/blog/2023/02/15/
// TODO: Check if we can link libc without a sysroot on macOS to get rid of these
#define SC_PLUGIN_LINKER_DEFINITIONS                                                                                   \
    SC_PLUGIN_LINKER_OPERATORS                                                                                         \
    extern "C" void bzero(void* s, SC::size_t n)                                                                       \
    {                                                                                                                  \
        unsigned char* p = reinterpret_cast<unsigned char*>(s);                                                        \
        while (n-- > 0)                                                                                                \
        {                                                                                                              \
            *p++ = 0;                                                                                                  \
        }                                                                                                              \
    }                                                                                                                  \
    extern "C" int memcmp(const void* s1, const void* s2, SC::size_t n)                                                \
    {                                                                                                                  \
        const unsigned char* p1 = reinterpret_cast<const unsigned char*>(s1);                                          \
        const unsigned char* p2 = reinterpret_cast<const unsigned char*>(s2);                                          \
        while (n--)                                                                                                    \
        {                                                                                                              \
            if (*p1 < *p2)                                                                                             \
            {                                                                                                          \
                return -1;                                                                                             \
            }                                                                                                          \
            else if (*p1 > *p2)                                                                                        \
            {                                                                                                          \
                return 1;                                                                                              \
            }                                                                                                          \
        }                                                                                                              \
        return 0;                                                                                                      \
    }

#endif
#else
#define SC_PLUGIN_LINKER_DEFINITIONS
#endif

//
// SC_DEFINE_PLUGIN
//
#define SC_DEFINE_PLUGIN(PluginName)                                                                                   \
    SC_PLUGIN_LINKER_DEFINITIONS                                                                                       \
    extern "C" SC_PLUGIN_EXPORT bool PluginName##Init(PluginName*& instance)                                           \
    {                                                                                                                  \
        instance = new PluginName();                                                                                   \
        return instance->init();                                                                                       \
    }                                                                                                                  \
                                                                                                                       \
    extern "C" SC_PLUGIN_EXPORT bool PluginName##Close(PluginName* instance)                                           \
    {                                                                                                                  \
        auto res = instance->close();                                                                                  \
        delete instance;                                                                                               \
        return res;                                                                                                    \
    }
