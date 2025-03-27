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
            alignas(uint64_t) char stackMemory[48 + sizeof(Buffer) + sizeof(SmallBuffer<10>)] = {0};

            FixedAllocator fixedAllocator = {stackMemory, sizeof(stackMemory)};
            Globals        globals        = {fixedAllocator};
            Globals::push(Globals::Global, globals);
            SC_TEST_EXPECT((testBuffer<Buffer, SmallBuffer<10>>(Globals::Global)));
            Globals::pop(Globals::Global);
        }

        if (test_section("thread-local"))
        {
            Thread t1, t2;
            bool   res[2] = {false, false};

            auto threadLocalTest = [&](Thread& thread)
            {
                alignas(uint64_t) char stackMemory[48 + sizeof(BufferTL) + sizeof(SmallBufferTL<10>)] = {0};
                // Every new thread can initialize its set of globals too if different from defaults
                Globals::init(Globals::ThreadLocal, {1024}); // Available memory for ownership tracker
                FixedAllocator fixedAllocator = {stackMemory, sizeof(stackMemory)};
                Globals        fixedGlobals   = {fixedAllocator};
                Globals::push(Globals::ThreadLocal, fixedGlobals);
                bool tRes = testBuffer<BufferTL, SmallBufferTL<10>>(Globals::ThreadLocal);
                if (&thread == &t1)
                    res[0] = tRes;
                else
                    res[1] = tRes;
                Globals::pop(Globals::ThreadLocal);
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
            Globals::push(Globals::Global, virtualGlobals);
            SC_TEST_EXPECT((testBuffer<Buffer, SmallBuffer<10>>(Globals::Global)));
            SC_TEST_EXPECT(virtualMemory.release());
            Globals::pop(Globals::Global);
        }
    }

    template <typename BufferT, typename SmallBufferT>
    [[nodiscard]] static bool testBuffer(Globals::Type globalsType)
    {
        Globals&      globals = Globals::get(globalsType);
        BufferT&      buffer1 = *globals.allocator.allocate<BufferT>();
        SmallBufferT& buffer2 = *globals.allocator.allocate<SmallBufferT>(buffer1);
        SC_TRY(appendBuffer(buffer1, "Buffer")); // Inserted on the heap
        SC_TRY(appendBuffer(buffer1, "1234"));   // Inserted on the heap
        SC_TRY(appendBuffer(buffer2, "2345"));   // Causes full copy to "heap"

        // Let's create a fixed allocator that has enough space to append more to buffer
        // but that will fail because the original buffer memory location doesn't belong
        // to this allocator. The owner parameter of allocate is used to filter it out.
        char           fixedBuffer[128];
        FixedAllocator fixedAllocator = {fixedBuffer, sizeof(fixedBuffer)};
        Globals        fixedGlobals   = {fixedAllocator};
        Globals::push(globalsType, fixedGlobals);
        SC_TRY(not appendBuffer(buffer2, "FAILURE")); // MUST fail
        Globals::pop(globalsType);

        // Let's now try to restore the "default allocator" (using malloc)
        // This allocator also keeps track of all of its allocations and it will refuse
        // to extend this buffer because of the "owner" parameter memory address not
        // belonging to any of the allocations it knows have been produced by itself.
        Globals* defaultGlobal = Globals::pop(globalsType);
        SC_TRY(not appendBuffer(buffer2, "FAILURE")); // MUST fail
        Globals::push(globalsType, *defaultGlobal);
        return true;
    }

    // Both BufferTL and SmallBufferTL can be passed as Buffer references
    static bool appendBuffer(Buffer& buffer, Span<const char> data) { return buffer.append(data); }
};

namespace SC
{
void runGlobalsTest(SC::TestReport& report) { GlobalsTest test(report); }
} // namespace SC
