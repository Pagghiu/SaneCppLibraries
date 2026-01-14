// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Threading/Atomic.h"
#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif
#if _MSC_VER
extern "C"
{
    char __stdcall _InterlockedCompareExchange8(char volatile* Destination, char Exchange, char Comparand);
#if defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
    char __stdcall _InterlockedExchange8_acq(char volatile* Target, char Value);
    char __stdcall _InterlockedExchange8_nf(char volatile* Target, char Value);
    char __stdcall _InterlockedExchange8_rel(char volatile* Target, char Value);
    char __stdcall _InterlockedCompareExchange8_acq(char volatile* Destination, char Exchange, char Comparand);
    char __stdcall _InterlockedCompareExchange8_nf(char volatile* Destination, char Exchange, char Comparand);
    char __stdcall _InterlockedCompareExchange8_rel(char volatile* Destination, char Exchange, char Comparand);
#endif
    void    __dmb(unsigned int _Type);
    void    __iso_volatile_store8(volatile __int8*, __int8);
    void    __iso_volatile_store32(volatile __int32*, __int32);
    __int8  __iso_volatile_load8(const volatile __int8*);
    __int32 __iso_volatile_load32(const volatile __int32*);
    void    _ReadWriteBarrier(void);
} // extern "C"
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

#ifdef __clang__
#define SC_COMPILER_MSVC_RESTORE_DEPRECATED_WARNING _Pragma("clang diagnostic pop")
#elif defined(__CUDACC__) || defined(__INTEL_COMPILER)
#define SC_COMPILER_MSVC_RESTORE_DEPRECATED_WARNING __pragma(warning(pop))
#else
#define SC_COMPILER_MSVC_RESTORE_DEPRECATED_WARNING _Pragma("warning(pop)")
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
    case memory_order_seq_cst:                                                                                         \
    case memory_order_release:                                                                                         \
    case memory_order_acq_rel:                                                                                         \
    default: SC_COMPILER_MSVC_COMPILER_MEMORY_BARRIER(); break;                                                        \
    }

#define SC_COMPILER_MSVC_ATOMIC_STORE_VERIFY_MEMORY_ORDER(_Order_var)                                                  \
    switch (_Order_var)                                                                                                \
    {                                                                                                                  \
    case memory_order_relaxed: break;                                                                                  \
    case memory_order_release:                                                                                         \
    case memory_order_acq_rel:                                                                                         \
    case memory_order_seq_cst:                                                                                         \
    case memory_order_consume:                                                                                         \
    case memory_order_acquire:                                                                                         \
    default: SC_COMPILER_MSVC_COMPILER_MEMORY_BARRIER(); break;                                                        \
    }
#endif

namespace SC
{
#if _MSC_VER

memory_order msvc_select_cas_order(memory_order success, memory_order failure)
{
    // Promote to acq_rel if release success is combined with acquire/consume failure.
    if (success == memory_order_release && (failure == memory_order_acquire || failure == memory_order_consume))
    {
        return memory_order_acq_rel;
    }
    // Otherwise, the success order is sufficient, because for all other cases
    // the load part of the RMW is at least as strong as the failure load.
    // (e.g. acquire, acq_rel, seq_cst success imply an acquire load).
    return success;
}

#endif

Atomic<int32_t>::Atomic(int32_t value) : value(value) {}

int32_t Atomic<int32_t>::fetch_add(int32_t val, memory_order mem)
{
#if _MSC_VER
#if defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
    switch (mem)
    {
    case memory_order_relaxed: return _InterlockedExchangeAdd_nf(reinterpret_cast<volatile long*>(&value), val);
    case memory_order_acquire: return _InterlockedExchangeAdd_acq(reinterpret_cast<volatile long*>(&value), val);
    case memory_order_release: return _InterlockedExchangeAdd_rel(reinterpret_cast<volatile long*>(&value), val);
    case memory_order_acq_rel:
    case memory_order_seq_cst:
    default: // consume is treated as acquire
        return _InterlockedExchangeAdd(reinterpret_cast<volatile long*>(&value), val);
    }
#else
    (void)mem;
    return _InterlockedExchangeAdd(reinterpret_cast<volatile long*>(&value), val);
#endif
#else
    return __atomic_fetch_add(&value, val, mem);
#endif
}

int32_t Atomic<int32_t>::fetch_sub(int32_t val, memory_order mem)
{
#if _MSC_VER
#if defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
    switch (mem)
    {
    case memory_order_relaxed: return _InterlockedExchangeAdd_nf(reinterpret_cast<volatile long*>(&value), -val);
    case memory_order_acquire: return _InterlockedExchangeAdd_acq(reinterpret_cast<volatile long*>(&value), -val);
    case memory_order_release: return _InterlockedExchangeAdd_rel(reinterpret_cast<volatile long*>(&value), -val);
    case memory_order_acq_rel:
    case memory_order_seq_cst:
    default: // consume is treated as acquire
        return _InterlockedExchangeAdd(reinterpret_cast<volatile long*>(&value), -val);
    }
#else
    (void)mem;
    return _InterlockedExchangeAdd(reinterpret_cast<volatile long*>(&value), -val);
#endif
#else
    return __atomic_fetch_sub(&value, val, mem);
#endif
}

int32_t Atomic<int32_t>::load(memory_order mem) const
{
#if _MSC_VER
    if (mem == memory_order_seq_cst)
    {
        return _InterlockedOr(reinterpret_cast<volatile long*>(const_cast<volatile int32_t*>(&value)), 0);
    }
    const int32_t res = __iso_volatile_load32(reinterpret_cast<const volatile int32_t*>(&value));
    SC_COMPILER_MSVC_ATOMIC_LOAD_VERIFY_MEMORY_ORDER(mem);
    return res;
#else
    int32_t res;
    __atomic_load(&value, &res, mem);
    return res;
#endif
}

void Atomic<int32_t>::store(int32_t desired, memory_order mem)
{
#if _MSC_VER
    if (mem == memory_order_seq_cst)
    {
        (void)_InterlockedExchange(reinterpret_cast<volatile long*>(&value), desired);
    }
    else
    {
        SC_COMPILER_MSVC_ATOMIC_STORE_VERIFY_MEMORY_ORDER(mem);
        __iso_volatile_store32(reinterpret_cast<volatile int*>(&value), desired);
    }
#else
    __atomic_store_n(&value, desired, mem);
#endif
}

int32_t Atomic<int32_t>::exchange(int32_t desired, memory_order mem)
{
#if _MSC_VER
#if defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
    switch (mem)
    {
    case memory_order_relaxed: return _InterlockedExchange_nf(reinterpret_cast<volatile long*>(&value), desired);
    case memory_order_acquire: return _InterlockedExchange_acq(reinterpret_cast<volatile long*>(&value), desired);
    case memory_order_release: return _InterlockedExchange_rel(reinterpret_cast<volatile long*>(&value), desired);
    case memory_order_acq_rel:
    case memory_order_seq_cst:
    default: // consume is treated as acquire
        return _InterlockedExchange(reinterpret_cast<volatile long*>(&value), desired);
    }
#else
    (void)mem;
    return _InterlockedExchange(reinterpret_cast<volatile long*>(&value), desired);
#endif
#else
    return __atomic_exchange_n(&value, desired, mem);
#endif
}

bool Atomic<int32_t>::compare_exchange_weak(int32_t& expected, int32_t desired, memory_order success,
                                            memory_order failure)
{
    return compare_exchange_strong(expected, desired, success, failure);
}

bool Atomic<int32_t>::compare_exchange_strong(int32_t& expected, int32_t desired, memory_order success,
                                              memory_order failure)
{
#if _MSC_VER
    const memory_order order = msvc_select_cas_order(success, failure);
#if defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
    long previous;
    switch (order)
    {
    case memory_order_relaxed:
        previous = _InterlockedCompareExchange_nf(reinterpret_cast<volatile long*>(&value), desired, expected);
        break;
    case memory_order_acquire:
    case memory_order_consume:
        previous = _InterlockedCompareExchange_acq(reinterpret_cast<volatile long*>(&value), desired, expected);
        break;
    case memory_order_release:
        previous = _InterlockedCompareExchange_rel(reinterpret_cast<volatile long*>(&value), desired, expected);
        break;
    case memory_order_acq_rel:
    case memory_order_seq_cst:
    default: previous = _InterlockedCompareExchange(reinterpret_cast<volatile long*>(&value), desired, expected); break;
    }
#else
    (void)order;
    const long previous = _InterlockedCompareExchange(reinterpret_cast<volatile long*>(&value), desired, expected);
#endif
    if (previous == expected)
    {
        return true;
    }
    expected = previous;
    return false;
#else
    return __atomic_compare_exchange_n(&value, &expected, desired, false, success, failure);
#endif
}

bool Atomic<int32_t>::compare_exchange_weak(int32_t& expected, int32_t desired, memory_order mem)
{
    memory_order failure = mem;
    if (failure == memory_order_release)
    {
        failure = memory_order_relaxed;
    }
    if (failure == memory_order_acq_rel)
    {
        failure = memory_order_acquire;
    }
    return compare_exchange_weak(expected, desired, mem, failure);
}

bool Atomic<int32_t>::compare_exchange_strong(int32_t& expected, int32_t desired, memory_order mem)
{
    memory_order failure = mem;
    if (failure == memory_order_release)
    {
        failure = memory_order_relaxed;
    }
    if (failure == memory_order_acq_rel)
    {
        failure = memory_order_acquire;
    }
    return compare_exchange_strong(expected, desired, mem, failure);
}

Atomic<int32_t>::operator int32_t() const { return load(); }
int32_t Atomic<int32_t>::operator=(int32_t desired)
{
    store(desired);
    return desired;
}
int32_t Atomic<int32_t>::operator++() { return fetch_add(1) + 1; }
int32_t Atomic<int32_t>::operator++(int) { return fetch_add(1); }
int32_t Atomic<int32_t>::operator--() { return fetch_sub(1) - 1; }
int32_t Atomic<int32_t>::operator--(int) { return fetch_sub(1); }

Atomic<bool>::Atomic(bool value) : value(value) {}

bool Atomic<bool>::exchange(bool desired, memory_order mem)
{
#if _MSC_VER
#if defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
    switch (mem)
    {
    case memory_order_relaxed:
        return static_cast<bool>(_InterlockedExchange8_nf(reinterpret_cast<volatile char*>(&value), desired));
    case memory_order_acquire:
    case memory_order_consume:
        return static_cast<bool>(_InterlockedExchange8_acq(reinterpret_cast<volatile char*>(&value), desired));
    case memory_order_release:
        return static_cast<bool>(_InterlockedExchange8_rel(reinterpret_cast<volatile char*>(&value), desired));
    case memory_order_acq_rel:
    case memory_order_seq_cst:
    default: return static_cast<bool>(_InterlockedExchange8(reinterpret_cast<volatile char*>(&value), desired));
    }
#else
    (void)mem;
    return static_cast<bool>(_InterlockedExchange8(reinterpret_cast<volatile char*>(&value), desired));
#endif
#else
    return __atomic_exchange_n(&value, desired, mem);
#endif
}

void Atomic<bool>::store(bool desired, memory_order mem)
{
#if _MSC_VER
    if (mem == memory_order_seq_cst)
    {
        (void)_InterlockedExchange8(reinterpret_cast<volatile char*>(&value), desired);
    }
    else
    {
        SC_COMPILER_MSVC_ATOMIC_STORE_VERIFY_MEMORY_ORDER(mem);
        __iso_volatile_store8(reinterpret_cast<volatile __int8*>(&value), desired);
    }
#else
    __atomic_store_n(&value, desired, mem);
#endif
}

bool Atomic<bool>::load(memory_order mem) const
{
#if _MSC_VER
    if (mem == memory_order_seq_cst)
    {
        return static_cast<bool>(
            _InterlockedOr8(reinterpret_cast<volatile char*>(const_cast<volatile bool*>(&value)), 0));
    }
    const char res = __iso_volatile_load8(reinterpret_cast<const volatile char*>(&value));
    SC_COMPILER_MSVC_ATOMIC_LOAD_VERIFY_MEMORY_ORDER(mem);
    return static_cast<bool>(res);
#else
    bool res;
    __atomic_load(&value, &res, mem);
    return res;
#endif
}

bool Atomic<bool>::compare_exchange_weak(bool& expected, bool desired, memory_order success, memory_order failure)
{
    return compare_exchange_strong(expected, desired, success, failure);
}

bool Atomic<bool>::compare_exchange_strong(bool& expected, bool desired, memory_order success, memory_order failure)
{
#if _MSC_VER
    const memory_order order         = msvc_select_cas_order(success, failure);
    const char         expected_char = expected;
#if defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
    char previous;
    switch (order)
    {
    case memory_order_relaxed:
        previous = _InterlockedCompareExchange8_nf(reinterpret_cast<volatile char*>(&value), desired, expected_char);
        break;
    case memory_order_acquire:
    case memory_order_consume:
        previous = _InterlockedCompareExchange8_acq(reinterpret_cast<volatile char*>(&value), desired, expected_char);
        break;
    case memory_order_release:
        previous = _InterlockedCompareExchange8_rel(reinterpret_cast<volatile char*>(&value), desired, expected_char);
        break;
    case memory_order_acq_rel:
    case memory_order_seq_cst:
    default:
        previous = _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(&value), desired, expected_char);
        break;
    }
#else
    (void)order;
    const char previous =
        _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(&value), desired, expected_char);
#endif
    if (previous == expected_char)
    {
        return true;
    }
    expected = static_cast<bool>(previous);
    return false;
#else
    return __atomic_compare_exchange_n(&value, &expected, desired, false, success, failure);
#endif
}

bool Atomic<bool>::compare_exchange_weak(bool& expected, bool desired, memory_order mem)
{
    memory_order failure = mem;
    if (failure == memory_order_release)
    {
        failure = memory_order_relaxed;
    }
    if (failure == memory_order_acq_rel)
    {
        failure = memory_order_acquire;
    }
    return compare_exchange_weak(expected, desired, mem, failure);
}

bool Atomic<bool>::compare_exchange_strong(bool& expected, bool desired, memory_order mem)
{
    memory_order failure = mem;
    if (failure == memory_order_release)
    {
        failure = memory_order_relaxed;
    }
    if (failure == memory_order_acq_rel)
    {
        failure = memory_order_acquire;
    }
    return compare_exchange_strong(expected, desired, mem, failure);
}

Atomic<bool>::operator bool() const { return load(); }
bool Atomic<bool>::operator=(bool desired)
{
    store(desired);
    return desired;
}

} // namespace SC

#if _MSC_VER
#undef SC_COMPILER_MSVC_ATOMIC_LOAD_VERIFY_MEMORY_ORDER
#undef SC_COMPILER_MSVC_ATOMIC_STORE_VERIFY_MEMORY_ORDER
#undef SC_COMPILER_MSVC_DISABLE_DEPRECATED_WARNING
#undef SC_COMPILER_MSVC_RESTORE_DEPRECATED_WARNING
#undef SC_COMPILER_MSVC_COMPILER_BARRIER
#undef SC_COMPILER_MSVC_MEMORY_BARRIER
#undef SC_COMPILER_MSVC_COMPILER_MEMORY_BARRIER
#endif
