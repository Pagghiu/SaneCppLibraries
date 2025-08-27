// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Threading/Atomic.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/Threading.h"

namespace SC
{
struct AtomicTest;
}

struct SC::AtomicTest : public SC::TestCase
{
    AtomicTest(SC::TestReport& report) : TestCase(report, "AtomicTest")
    {
        if (test_section("Atomic<int32_t> single-threaded"))
        {
            testAtomicInt32SingleThreaded();
        }
        if (test_section("Atomic<bool> single-threaded"))
        {
            testAtomicBoolSingleThreaded();
        }
        if (test_section("Atomic multi-threaded"))
        {
            testAtomicMultiThreaded();
        }
    }

    void testAtomicInt32SingleThreaded();
    void testAtomicBoolSingleThreaded();
    void testAtomicMultiThreaded();
};

void SC::AtomicTest::testAtomicInt32SingleThreaded()
{
    // Constructor and initial value
    Atomic<int32_t> i(0);
    SC_TEST_EXPECT(i.load() == 0);

    // store / load
    i.store(1, memory_order_relaxed);
    SC_TEST_EXPECT(i.load(memory_order_relaxed) == 1);
    i.store(2, memory_order_release);
    SC_TEST_EXPECT(i.load(memory_order_acquire) == 2);

    // operator= and conversion
    i = 3;
    SC_TEST_EXPECT(i == 3);

    // exchange
    int32_t prev = i.exchange(4);
    SC_TEST_EXPECT(prev == 3);
    SC_TEST_EXPECT(i == 4);
    prev = i.exchange(5, memory_order_acq_rel);
    SC_TEST_EXPECT(prev == 4);
    SC_TEST_EXPECT(i == 5);

    // compare_exchange_strong
    int32_t expected = 5;
    SC_TEST_EXPECT(i.compare_exchange_strong(expected, 6));
    SC_TEST_EXPECT(i == 6);
    SC_TEST_EXPECT(expected == 5); // Should not be modified on success

    expected = 5; // Wrong expected value
    SC_TEST_EXPECT(not i.compare_exchange_strong(expected, 7));
    SC_TEST_EXPECT(i == 6);
    SC_TEST_EXPECT(expected == 6); // Should be modified to current value on failure

    // compare_exchange_strong with memory orders
    expected = 6;
    SC_TEST_EXPECT(i.compare_exchange_strong(expected, 7, memory_order_release, memory_order_relaxed));
    SC_TEST_EXPECT(i == 7);
    SC_TEST_EXPECT(expected == 6);

    // compare_exchange_weak
    expected = 7;
    // weak can fail spuriously, so we loop
    while (not i.compare_exchange_weak(expected, 8))
    {
        SC_TEST_EXPECT(expected == 7); // On spurious failure, expected is not modified
    }
    SC_TEST_EXPECT(i == 8);
    SC_TEST_EXPECT(expected == 7);

    expected = 7; // Wrong expected value
    SC_TEST_EXPECT(not i.compare_exchange_weak(expected, 9));
    SC_TEST_EXPECT(i == 8);
    SC_TEST_EXPECT(expected == 8);

    // fetch_add / fetch_sub
    SC_TEST_EXPECT(i.fetch_add(2) == 8);
    SC_TEST_EXPECT(i == 10);
    SC_TEST_EXPECT(i.fetch_add(2, memory_order_relaxed) == 10);
    SC_TEST_EXPECT(i == 12);
    SC_TEST_EXPECT(i.fetch_sub(3) == 12);
    SC_TEST_EXPECT(i == 9);
    SC_TEST_EXPECT(i.fetch_sub(3, memory_order_relaxed) == 9);
    SC_TEST_EXPECT(i == 6);

    // operators
    SC_TEST_EXPECT(i++ == 6); // post-increment
    SC_TEST_EXPECT(i == 7);
    SC_TEST_EXPECT(++i == 8); // pre-increment
    SC_TEST_EXPECT(i == 8);
    SC_TEST_EXPECT(i-- == 8); // post-decrement
    SC_TEST_EXPECT(i == 7);
    SC_TEST_EXPECT(--i == 6); // pre-decrement
    SC_TEST_EXPECT(i == 6);
}

void SC::AtomicTest::testAtomicBoolSingleThreaded()
{
    // Constructor and initial value
    Atomic<bool> b(false);
    SC_TEST_EXPECT(b.load() == false);

    // store / load
    b.store(true);
    SC_TEST_EXPECT(b.load() == true);
    b.store(false, memory_order_relaxed);
    SC_TEST_EXPECT(b.load(memory_order_relaxed) == false);
    b.store(true, memory_order_release);
    SC_TEST_EXPECT(b.load(memory_order_acquire) == true);

    // operator= and conversion
    b = false;
    SC_TEST_EXPECT(b == false);

    // exchange
    bool b_prev = b.exchange(true);
    SC_TEST_EXPECT(b_prev == false);
    SC_TEST_EXPECT(b == true);
    b_prev = b.exchange(false, memory_order_acq_rel);
    SC_TEST_EXPECT(b_prev == true);
    SC_TEST_EXPECT(b == false);

    // compare_exchange_strong
    bool b_expected = true;
    b_expected      = false;
    SC_TEST_EXPECT(b.compare_exchange_strong(b_expected, true));
    SC_TEST_EXPECT(b == true);
    SC_TEST_EXPECT(b_expected == false); // Should not be modified on success

    b_expected = false; // Wrong expected value
    SC_TEST_EXPECT(not b.compare_exchange_strong(b_expected, false));
    SC_TEST_EXPECT(b == true);
    SC_TEST_EXPECT(b_expected == true); // Should be modified to current value on failure

    // compare_exchange_strong with memory orders
    b_expected = true;
    SC_TEST_EXPECT(b.compare_exchange_strong(b_expected, false, memory_order_release, memory_order_relaxed));
    SC_TEST_EXPECT(b == false);
    SC_TEST_EXPECT(b_expected == true);

    // compare_exchange_weak
    b_expected = false;
    // weak can fail spuriously, so we loop
    while (not b.compare_exchange_weak(b_expected, true))
    {
        SC_TEST_EXPECT(b_expected == false); // On spurious failure, expected is not modified
    }
    SC_TEST_EXPECT(b == true);
    SC_TEST_EXPECT(b_expected == false);

    b_expected = false; // Wrong expected value
    SC_TEST_EXPECT(not b.compare_exchange_weak(b_expected, false));
    SC_TEST_EXPECT(b == true);
    SC_TEST_EXPECT(b_expected == true);
}

void SC::AtomicTest::testAtomicMultiThreaded()
{
    // Test fetch_add
    {
        Atomic<int32_t> counter(0);
        Thread          threads[4];
        for (auto& thread : threads)
        {
            SC_TEST_EXPECT(thread.start(
                [&](Thread&)
                {
                    for (int k = 0; k < 1000; ++k)
                    {
                        counter.fetch_add(1, memory_order_relaxed);
                    }
                }));
        }
        for (auto& thread : threads)
        {
            SC_TEST_EXPECT(thread.join());
        }
        SC_TEST_EXPECT(counter.load() == 4000);
    }

    // Test compare_exchange_strong
    {
        Atomic<int32_t> value(0);
        Atomic<int32_t> successes(0);
        Thread          threads[4];
        for (auto& thread : threads)
        {
            SC_TEST_EXPECT(thread.start(
                [&](Thread&)
                {
                    int32_t expected = 0;
                    if (value.compare_exchange_strong(expected, 1))
                    {
                        successes.fetch_add(1);
                    }
                }));
        }
        for (auto& thread : threads)
        {
            SC_TEST_EXPECT(thread.join());
        }
        SC_TEST_EXPECT(value.load() == 1);
        SC_TEST_EXPECT(successes.load() == 1);
    }

    // Test store(release) / load(acquire) ordering
    {
        struct Context
        {
            Atomic<bool> flag = false;
            int          data = 0;
        } ctx;
        Thread producer;
        SC_TEST_EXPECT(producer.start(
            [this, &ctx](Thread&)
            {
                ctx.data = 42;                              // 1. Write data
                ctx.flag.store(true, memory_order_release); // 2. Set flag, making data visible
            }));

        Thread consumer;
        SC_TEST_EXPECT(consumer.start(
            [this, &ctx](Thread&)
            {
                while (not ctx.flag.load(memory_order_acquire)) // 3. Wait for flag
                {
                    // spin
                }
                SC_TEST_EXPECT(ctx.data == 42); // 4. Read data, must be 42
            }));

        SC_TEST_EXPECT(producer.join());
        SC_TEST_EXPECT(consumer.join());
    }
}

namespace SC
{
void runAtomicTest(SC::TestReport& report) { AtomicTest test(report); }
} // namespace SC
