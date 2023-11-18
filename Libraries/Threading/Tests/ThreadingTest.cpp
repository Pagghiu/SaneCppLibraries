// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../Threading.h"
#include "../../Testing/Testing.h"

namespace SC
{
struct ThreadingTest;
}

struct SC::ThreadingTest : public SC::TestCase
{
    ThreadingTest(SC::TestReport& report) : TestCase(report, "ThreadingTest")
    {
        if (test_section("Thread"))
        {
            bool   threadCalled = false;
            Thread defaultInit;
            SC_TEST_EXPECT(not defaultInit.join());
            SC_TEST_EXPECT(not defaultInit.detach());
            Thread thread;
            Thread thread2 = move(thread);
            SC_TEST_EXPECT(not thread.join());
            SC_TEST_EXPECT(not thread.detach());
            Action lambda = [&] { threadCalled = true; };
            SC_TEST_EXPECT(thread2.start("test thread", &lambda));
            Thread thread3 = move(thread2);
            SC_TEST_EXPECT(not thread2.join());
            SC_TEST_EXPECT(not thread2.detach());
            SC_TEST_EXPECT(thread3.join());
            SC_TEST_EXPECT(not thread3.detach());
            SC_TEST_EXPECT(threadCalled);
        }
    }
};

namespace SC
{
void runThreadingTest(SC::TestReport& report) { ThreadingTest test(report); }
} // namespace SC
