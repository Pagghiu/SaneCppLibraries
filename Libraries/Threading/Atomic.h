// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Language/Types.h"

#if _MSC_VER
extern "C"
{
    long    _InterlockedExchangeAdd(long volatile* Addend, long Value);
    char    _InterlockedExchange8(char volatile* Target, char Value);
    void    __dmb(unsigned int _Type);
    void    __iso_volatile_store8(volatile __int8*, __int8);
    __int8  __iso_volatile_load8(const volatile __int8*);
    __int32 __iso_volatile_load32(const volatile __int32*);
    void    _ReadWriteBarrier(void);

#ifndef SC_COMPILER_MSVC_DISABLE_DEPRECATED_WARNING
#ifdef __clang__
#define SC_COMPILER_MSVC_DISABLE_DEPRECATED_WARNING                                                                    \
    _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
#elif defined(__CUDACC__) || defined(__INTEL_COMPILER)
#define SC_COMPILER_MSVC_DISABLE_DEPRECATED_WARNING                                                                    \
    __pragma(warning(push)) __pragma(warning(disable : 4996)) // was declared deprecated
#else                                                         // vvv MSVC vvv
#define SC_COMPILER_MSVC_DISABLE_DEPRECATED_WARNING                                                                    \
    _Pragma("warning(push)") _Pragma("warning(disable : 4996)") // was declared deprecated
#endif                                                          // ^^^ MSVC ^^^
#endif                                                          // SC_COMPILER_MSVC_DISABLE_DEPRECATED_WARNING

#ifndef SC_COMPILER_MSVC_RESTORE_DEPRECATED_WARNING
#ifdef __clang__
#define SC_COMPILER_MSVC_RESTORE_DEPRECATED_WARNING _Pragma("clang diagnostic pop")
#elif defined(__CUDACC__) || defined(__INTEL_COMPILER)
#define SC_COMPILER_MSVC_RESTORE_DEPRECATED_WARNING __pragma(warning(pop))
#else
#define SC_COMPILER_MSVC_RESTORE_DEPRECATED_WARNING _Pragma("warning(pop)")
#endif
#endif

#define SC_COMPILER_MSVC_COMPILER_BARRIER()                                                                            \
    SC_COMPILER_MSVC_DISABLE_DEPRECATED_WARNING _ReadWriteBarrier() SC_COMPILER_MSVC_RESTORE_DEPRECATED_WARNING

#if defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
#define SC_COMPILER_MSVC_MEMORY_BARRIER()          __dmb(0xB)
#define SC_COMPILER_MSVC_COMPILER_MEMORY_BARRIER() SC_COMPILER_MSVC_MEMORY_BARRIER()
#elif defined(_M_IX86) || defined(_M_X64)
#define SC_COMPILER_MSVC_COMPILER_MEMORY_BARRIER() SC_COMPILER_MSVC_COMPILER_BARRIER()
#else
#error Unsupported hardware
#endif

#define SC_COMPILER_MSVC_ATOMIC_LOAD_VERIFY_MEMORY_ORDER(_Order_var)                                                   \
    switch (_Order_var)                                                                                                \
    {                                                                                                                  \
    case memory_order_relaxed: break;                                                                                  \
    case memory_order_consume:                                                                                         \
    case memory_order_acquire:                                                                                         \
    case memory_order_seq_cst: SC_COMPILER_MSVC_COMPILER_MEMORY_BARRIER(); break;                                      \
    case memory_order_release:                                                                                         \
    case memory_order_acq_rel:                                                                                         \
    default: break;                                                                                                    \
    }
}
#endif

namespace SC
{
#if _MSC_VER
typedef enum memory_order
{
    memory_order_relaxed,
    memory_order_consume,
    memory_order_acquire,
    memory_order_release,
    memory_order_acq_rel,
    memory_order_seq_cst
} memory_order;
#else
typedef enum memory_order
{
    memory_order_relaxed = __ATOMIC_RELAXED,
    memory_order_consume = __ATOMIC_CONSUME,
    memory_order_acquire = __ATOMIC_ACQUIRE,
    memory_order_release = __ATOMIC_RELEASE,
    memory_order_acq_rel = __ATOMIC_ACQ_REL,
    memory_order_seq_cst = __ATOMIC_SEQ_CST
} memory_order;

#endif
template <typename T>
struct Atomic;

template <>
struct Atomic<int32_t>
{
    Atomic(int32_t value) : value(value) {}

    int32_t fetch_add(int32_t val)
    {
#if _MSC_VER
        int32_t res;
        res = _InterlockedExchangeAdd(reinterpret_cast<volatile long*>(&value), val);
        return res;
#else
        return __atomic_fetch_add(&value, val, __ATOMIC_SEQ_CST);
#endif
    }

    int32_t load() const
    {
        int32_t res;
#if _MSC_VER
        res = __iso_volatile_load32(reinterpret_cast<volatile const int*>(&value));
        SC_COMPILER_MSVC_COMPILER_MEMORY_BARRIER();
#else
        __atomic_load(&value, &res, __ATOMIC_SEQ_CST);
#endif
        return res;
    }

    int32_t load(memory_order mem) const
    {
        int32_t res;
#if _MSC_VER
        res = __iso_volatile_load32(reinterpret_cast<volatile const int*>(&value));
        SC_COMPILER_MSVC_ATOMIC_LOAD_VERIFY_MEMORY_ORDER(mem);
#else
        __atomic_load(&value, &res, mem);
#endif
        return res;
    }

  private:
    volatile int32_t value;
};

template <>
struct Atomic<bool>
{
    Atomic(bool value) : value(value) {}

    bool exchange(bool desired)
    {
#if _MSC_VER
        return static_cast<bool>(_InterlockedExchange8(reinterpret_cast<volatile char*>(&value), desired));
#else
        bool res;
        __atomic_exchange(&value, &desired, &res, __ATOMIC_SEQ_CST);
        return res;
#endif
    }

    bool load() const
    {
#if _MSC_VER
        char res = __iso_volatile_load8(reinterpret_cast<volatile const char*>(&value));
        SC_COMPILER_MSVC_COMPILER_MEMORY_BARRIER();
        return reinterpret_cast<bool&>(res);
#else
        bool res;
        __atomic_load(&value, &res, __ATOMIC_SEQ_CST);
        return res;
#endif
    }

    bool load(memory_order mem) const
    {
#if _MSC_VER
        char res = __iso_volatile_load8(reinterpret_cast<volatile const char*>(&value));
        SC_COMPILER_MSVC_ATOMIC_LOAD_VERIFY_MEMORY_ORDER(mem);
        return reinterpret_cast<bool&>(res);
#else
        bool res;
        __atomic_load(&value, &res, mem);
        return res;
#endif
    }

  private:
    volatile bool value;
};

} // namespace SC

#undef SC_COMPILER_MSVC_ATOMIC_LOAD_VERIFY_MEMORY_ORDER
#undef SC_COMPILER_MSVC_DISABLE_DEPRECATED_WARNING
#undef SC_COMPILER_MSVC_RESTORE_DEPRECATED_WARNING
#undef SC_COMPILER_MSVC_COMPILER_BARRIER
#undef SC_COMPILER_MSVC_MEMORY_BARRIER
#undef SC_COMPILER_MSVC_COMPILER_MEMORY_BARRIER
