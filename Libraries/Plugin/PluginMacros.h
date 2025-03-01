// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Memory.h"
#include "../Foundation/PrimitiveTypes.h"
#include "PluginHash.h"

//
// SC_PLUGIN_EXPORT
//

#if SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL
#define SC_PLUGIN_EXPORT __declspec(dllexport)
#else
#define SC_PLUGIN_EXPORT __attribute__((visibility("default")))
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
    void operator delete(void* p, SC::size_t) noexcept                                                                 \
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
    }                                                                                                                  \
    extern "C" int _fltused = 0;
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
    }                                                                                                                  \
    extern "C" int __cxa_guard_acquire(SC::uint64_t* guard_object)                                                     \
    {                                                                                                                  \
        if (*reinterpret_cast<const SC::uint8_t*>(guard_object) != 0)                                                  \
            return 0;                                                                                                  \
        return 1;                                                                                                      \
    }                                                                                                                  \
    extern "C" void __cxa_guard_release(SC::uint64_t* guard_object)                                                    \
    {                                                                                                                  \
        *reinterpret_cast<SC::uint8_t*>(guard_object) = 1;                                                             \
    }

#endif
#else
#define SC_PLUGIN_LINKER_DEFINITIONS
#endif

//
// SC_PLUGIN_DEFINE
//
#define SC_PLUGIN_DEFINE(PluginName)                                                                                   \
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

#define SC_PLUGIN_EXPORT_INTERFACES(PluginName, ...)                                                                   \
    extern "C" SC_PLUGIN_EXPORT bool PluginName##QueryInterface(PluginName* plugin, SC::uint32_t hash,                 \
                                                                void** pluginInterface)                                \
    {                                                                                                                  \
        return SC::PluginCastInterface<PluginName, __VA_ARGS__>()(plugin, hash, pluginInterface);                      \
    }

namespace SC
{
template <typename PluginClass, typename... InterfaceClasses>
struct PluginCastInterface;

template <typename PluginClass>
struct PluginCastInterface<PluginClass>
{
    bool operator()(PluginClass*, uint32_t, void**) { return false; }
};

template <typename PluginClass, typename InterfaceClass, typename... InterfaceClasses>
struct PluginCastInterface<PluginClass, InterfaceClass, InterfaceClasses...>
{
    bool operator()(PluginClass* plugin, uint32_t hash, void** pluginInterface)
    {
        if (hash == InterfaceClass::InterfaceHash)
        {
            *pluginInterface = static_cast<InterfaceClass*>(plugin);
            return true;
        }
        return PluginCastInterface<PluginClass, InterfaceClasses...>()(plugin, hash, pluginInterface);
    }
};
} // namespace SC
