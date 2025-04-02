// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/VirtualMemory.h"
#include "../../Strings/String.h"
#include "../../Testing/Testing.h"
#include "../Vector.h"

namespace SC
{
struct GlobalsContainerTest;
}

struct SC::GlobalsContainerTest : public SC::TestCase
{
    GlobalsContainerTest(SC::TestReport& report) : TestCase(report, "GlobalsContainerTest")
    {
        using namespace SC;
        if (test_section("global virtual"))
        {
            virtualGlobal();
        }
    }

    void virtualGlobal();
};

void SC::GlobalsContainerTest::virtualGlobal()
{
    VirtualMemory virtualMemory;
    SC_TEST_EXPECT(virtualMemory.reserve(1024 * 1024)); // 1 MB
    VirtualAllocator virtualAllocator = {virtualMemory};
    Globals          virtualGlobals   = {virtualAllocator};
    Globals::push(Globals::ThreadLocal, virtualGlobals);
    VectorTL<char>*         v1 = Globals::get(Globals::ThreadLocal).allocator.create<VectorTL<char>>();
    SmallVectorTL<char, 5>* v2 = Globals::get(Globals::ThreadLocal).allocator.create<SmallVectorTL<char, 5>>();
    SC_TEST_EXPECT(v1->append({"SALVE"}));
    SC_TEST_EXPECT(v2->append({"SALVE"}));
    SC_TEST_EXPECT(v2->append({"SALVE2"}));
    SC_TEST_EXPECT(virtualMemory.release());
    Globals::pop(Globals::ThreadLocal);
}

namespace SC
{
void runGlobalsContainerTest(SC::TestReport& report) { GlobalsContainerTest test(report); }
} // namespace SC
