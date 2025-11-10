// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/PrimitiveTypes.h"

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
/// @brief Atomic variables (only for `int` and `bool` for now).
/// @n
/// Example:
/// @code{.cpp}
/// Atomic<bool> test = true;
///
/// SC_TEST_EXPECT(test.load());
/// test.exchange(false);
/// SC_TEST_EXPECT(not test.load());
/// @endcode
template <typename T>
struct Atomic;

template <>
struct SC_COMPILER_EXPORT Atomic<int32_t>
{
    Atomic(int32_t value = 0);

    int32_t fetch_add(int32_t val, memory_order mem = memory_order_seq_cst);
    int32_t fetch_sub(int32_t val, memory_order mem = memory_order_seq_cst);
    int32_t load(memory_order mem = memory_order_seq_cst) const;
    void    store(int32_t desired, memory_order mem = memory_order_seq_cst);
    int32_t exchange(int32_t desired, memory_order mem = memory_order_seq_cst);

    bool compare_exchange_weak(int32_t& expected, int32_t desired, memory_order success, memory_order failure);
    bool compare_exchange_strong(int32_t& expected, int32_t desired, memory_order success, memory_order failure);
    bool compare_exchange_weak(int32_t& expected, int32_t desired, memory_order mem = memory_order_seq_cst);
    bool compare_exchange_strong(int32_t& expected, int32_t desired, memory_order mem = memory_order_seq_cst);

    // Operators
    operator int32_t() const;
    int32_t operator=(int32_t desired);
    int32_t operator++();
    int32_t operator++(int);
    int32_t operator--();
    int32_t operator--(int);

  private:
    volatile int32_t value;
};

template <>
struct SC_COMPILER_EXPORT Atomic<bool>
{
    Atomic(bool value = false);

    bool exchange(bool desired, memory_order mem = memory_order_seq_cst);
    void store(bool desired, memory_order mem = memory_order_seq_cst);
    bool load(memory_order mem = memory_order_seq_cst) const;

    bool compare_exchange_weak(bool& expected, bool desired, memory_order success, memory_order failure);
    bool compare_exchange_strong(bool& expected, bool desired, memory_order success, memory_order failure);
    bool compare_exchange_weak(bool& expected, bool desired, memory_order mem = memory_order_seq_cst);
    bool compare_exchange_strong(bool& expected, bool desired, memory_order mem = memory_order_seq_cst);

    // Operators
    operator bool() const;
    bool operator=(bool desired);

  private:
    volatile bool value;
};

} // namespace SC
