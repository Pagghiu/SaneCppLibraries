// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/PrimitiveTypes.h"

#if SC_PLATFORM_LINUX
#if defined(__has_include)
#if __has_include(<features.h>)
#include <features.h>
#endif
#endif
#endif

#if SC_PLATFORM_WINDOWS
#define SC_LIBC_CDECL __cdecl
#else
#define SC_LIBC_CDECL
#endif

#if SC_COMPILER_ENABLE_STD_CPP || SC_LANGUAGE_EXCEPTIONS ||                                                            \
    (SC_PLATFORM_WINDOWS && not SC_COMPILER_MSVC and not SC_COMPILER_CLANG_CL)
#include <memory.h>
#endif

#if defined(__cplusplus)

#if SC_COMPILER_MSVC
extern "C"
{
    void* SC_LIBC_CDECL       memcpy(void* dst, const void* src, SC::size_t len);
    void* SC_LIBC_CDECL       memmove(void* dst, const void* src, SC::size_t len);
    int SC_LIBC_CDECL         memcmp(const void* lhs, const void* rhs, SC::size_t len);
    void* SC_LIBC_CDECL       memset(void* dst, int value, SC::size_t len);
    const void* SC_LIBC_CDECL memchr(const void* ptr, int value, SC::size_t len);
    SC::size_t SC_LIBC_CDECL  strlen(const char* str);
    SC::size_t SC_LIBC_CDECL  wcslen(const wchar_t* str);
}
#endif

extern "C++"
{
    template <typename T1, typename T2>
    inline void* memcpy(T1* dst, const T2* src, SC::size_t n)
    {
#if SC_COMPILER_CLANG || SC_COMPILER_GCC
        return __builtin_memcpy(static_cast<void*>(dst), static_cast<const void*>(src), n);
#else
        return ::memcpy(static_cast<void*>(dst), static_cast<const void*>(src), n);
#endif
    }

    template <typename T1, typename T2>
    inline void* memmove(T1* dst, const T2* src, SC::size_t n)
    {
#if SC_COMPILER_CLANG || SC_COMPILER_GCC
        return __builtin_memmove(static_cast<void*>(dst), static_cast<const void*>(src), n);
#else
        return ::memmove(static_cast<void*>(dst), static_cast<const void*>(src), n);
#endif
    }

    template <typename T1, typename T2>
    inline int memcmp(const T1* s1, const T2* s2, SC::size_t n)
    {
#if SC_COMPILER_CLANG || SC_COMPILER_GCC
        return __builtin_memcmp(static_cast<const void*>(s1), static_cast<const void*>(s2), n);
#else
        return ::memcmp(static_cast<const void*>(s1), static_cast<const void*>(s2), n);
#endif
    }

    template <typename T>
    inline void* memset(T* dst, int c, SC::size_t n)
    {
#if SC_COMPILER_CLANG || SC_COMPILER_GCC
        return __builtin_memset(static_cast<void*>(dst), c, n);
#else
        return ::memset(static_cast<void*>(dst), c, n);
#endif
    }

    template <typename T>
    inline const T* memchr(const T* s, int c, SC::size_t n)
    {
#if SC_COMPILER_CLANG || SC_COMPILER_GCC
        return static_cast<const T*>(__builtin_memchr(static_cast<const void*>(s), c, n));
#else
        return static_cast<const T*>(::memchr(static_cast<const void*>(s), c, n));
#endif
    }

    template <typename T>
    inline T* memchr(T* s, int c, SC::size_t n)
    {
        return const_cast<T*>(::memchr(static_cast<const T*>(s), c, n));
    }

    template <typename T>
    inline SC::size_t strlen(const T* s)
    {
#if SC_COMPILER_CLANG || SC_COMPILER_GCC
        return __builtin_strlen(reinterpret_cast<const char*>(s));
#else
        return ::strlen(reinterpret_cast<const char*>(s));
#endif
    }

#if SC_PLATFORM_WINDOWS
    template <typename T>
    inline SC::size_t wcslen(const T* s)
    {
#if SC_COMPILER_CLANG || SC_COMPILER_GCC
        return __builtin_wcslen(reinterpret_cast<const wchar_t*>(s));
#else
        return ::wcslen(reinterpret_cast<const wchar_t*>(s));
#endif
    }
#endif
}

#else

extern "C"
{
#if SC_PLATFORM_WINDOWS
    void* SC_LIBC_CDECL      memmove(void* dst, const void* src, SC::size_t n);
    void* SC_LIBC_CDECL      memcpy(void* dst, const void* src, SC::size_t n);
    int SC_LIBC_CDECL        memcmp(const void* s1, const void* s2, SC::size_t n);
    void* SC_LIBC_CDECL      memset(void* dst, int c, SC::size_t len);
    void* SC_LIBC_CDECL      memchr(const void* s, int c, SC::size_t n);
    SC::size_t SC_LIBC_CDECL strlen(const char* s);
    SC::size_t SC_LIBC_CDECL wcslen(const wchar_t* s);
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX || SC_PLATFORM_EMSCRIPTEN
    void*      memmove(void* dst, const void* src, SC::size_t n);
    void*      memcpy(void* dst, const void* src, SC::size_t n);
    int        memcmp(const void* s1, const void* s2, SC::size_t n);
    void*      memset(void* dst, int c, SC::size_t len);
    void*      memchr(const void* s, int c, SC::size_t n);
    SC::size_t strlen(const char* s);
#else
#error "Unsupported platform"
#endif
}

#endif

#undef SC_LIBC_CDECL
