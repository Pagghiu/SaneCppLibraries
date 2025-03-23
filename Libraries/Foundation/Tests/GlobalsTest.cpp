// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Containers/Array.h"
#include "../../Containers/Vector.h"
#include "../../Foundation/VirtualMemory.h"
#include "../../Testing/Testing.h"
#include "../../Threading/Threading.h"
namespace SC
{
struct GlobalsTest;
} // namespace SC

struct SC::GlobalsTest : public SC::TestCase
{
    GlobalsTest(SC::TestReport& report) : TestCase(report, "GlobalsTest")
    {
        if (test_section("global"))
        {
            alignas(uint64_t) char stackMemory[48] = {0};

            FixedAllocator fixedAllocator = {stackMemory, sizeof(stackMemory)};
            Globals        globals        = {fixedAllocator};
            Globals::pushGlobal(globals);
            SC_TEST_EXPECT((testBuffer<Buffer, SmallBuffer<10>>()));
            Globals::popGlobal();
        }

        if (test_section("thread-local"))
        {
            Thread t1, t2;
            bool   res[2] = {false, false};

            auto threadLocalTest = [&](Thread& thread)
            {
                alignas(uint64_t) char stackMemory[48] = {0};

                FixedAllocator fixedAllocator = {stackMemory, sizeof(stackMemory)};
                Globals        fixedGlobals   = {fixedAllocator};
                Globals::pushThreadLocal(fixedGlobals);
                bool tRes = testBuffer<BufferTL, SmallBufferTL<10>>();
                if (&thread == &t1)
                    res[0] = tRes;
                else
                    res[1] = tRes;
                Globals::popThreadLocal();
            };
            SC_TEST_EXPECT(t1.start(threadLocalTest));
            SC_TEST_EXPECT(t2.start(threadLocalTest));
            SC_TEST_EXPECT(t1.join());
            SC_TEST_EXPECT(t2.join());
            SC_TEST_EXPECT(res[0]);
            SC_TEST_EXPECT(res[1]);
        }

        if (test_section("global virtual"))
        {
            VirtualMemory virtualMemory;
            SC_TEST_EXPECT(virtualMemory.reserve(1024 * 1024)); // 1 MB
            VirtualAllocator virtualAllocator = {virtualMemory};
            Globals          virtualGlobals   = {virtualAllocator};
            Globals::pushGlobal(virtualGlobals);
            SC_TEST_EXPECT((testBuffer<Buffer, SmallBuffer<10>>()));
            SC_TEST_EXPECT(virtualMemory.release());
            Globals::popGlobal();
        }
    }

    template <typename BufferT, typename SmallBufferT>
    [[nodiscard]] static bool testBuffer()
    {
        BufferT buffer1;
        SC_TRY(appendBuffer(buffer1, "Buffer")); // Inserted on the heap
        SmallBufferT buffer2 = buffer1;          // Fits in the inline buffer
        SC_TRY(appendBuffer(buffer1, "1234"));   // Inserted on the heap
        SC_TRY(appendBuffer(buffer2, "2345"));   // Causes full copy to "heap"
        return true;
    }

    // Both BufferTL and SmallBufferTL can be passed as Buffer references
    static bool appendBuffer(Buffer& buffer, Span<const char> data) { return buffer.append(data); }
};

namespace SC
{
void runGlobalsTest(SC::TestReport& report) { GlobalsTest test(report); }
} // namespace SC
