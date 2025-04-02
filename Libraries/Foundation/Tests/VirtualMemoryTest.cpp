// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../VirtualMemory.h"
#include "../../Testing/Testing.h"
#include "../Deferred.h"
#include "../Memory.h"

namespace SC
{
struct VirtualMemoryTest;
}

struct SC::VirtualMemoryTest : public SC::TestCase
{
    VirtualMemoryTest(SC::TestReport& report) : TestCase(report, "VirtualMemoryTest")
    {
        using namespace SC;
        if (test_section("virtual"))
        {
            virtualMemory();
        }
    }

    void virtualMemory();
};

void SC::VirtualMemoryTest::virtualMemory()
{
    //! [VirtualMemorySnippet]
    // This test uses two pages initially and just one page later
    // On Windows and Linux default Page size is typically 4Kb
    // On macOS default page size is typically 16 Kb
    const size_t moreThanOnePageSize = VirtualMemory::getPageSize() + 1024;
    const size_t lessThanOnePageSize = VirtualMemory::getPageSize() - 1024;
    SC_TEST_EXPECT(lessThanOnePageSize > 0); // sanity check just in case

    void* reference = Memory::allocate(moreThanOnePageSize, 1);
    memset(reference, 1, moreThanOnePageSize);
    auto releaseLater = MakeDeferred([&] { Memory::release(reference); });

    VirtualMemory virtualMemory;

    // Reserve 2 pages of virtual memory
    SC_TEST_EXPECT(virtualMemory.reserve(2 * VirtualMemory::getPageSize()));

    // Request to use less than one page of virtual memory
    SC_TEST_EXPECT(virtualMemory.commit(lessThanOnePageSize));
    char* memory = static_cast<char*>(virtualMemory.memory);

    // Check that memory is writable and fill it with 1
    memset(memory, 1, lessThanOnePageSize);

    // Let's now extend this block from one to two pages
    SC_TEST_EXPECT(virtualMemory.commit(moreThanOnePageSize));

    // Fill the "newly committed" pages with 1
    memset(memory + lessThanOnePageSize, 1, moreThanOnePageSize - lessThanOnePageSize);

    // Make sure that previously reserved address is stable
    SC_TEST_EXPECT(memory == virtualMemory.memory);

    // Check that all allocated bytes are addressable and contain expected pattern
    SC_TEST_EXPECT(memcmp(memory, reference, moreThanOnePageSize) == 0);

    // Now let's de-commit everything but the first page
    SC_TEST_EXPECT(virtualMemory.shrink(lessThanOnePageSize));

    // Address should stay stable
    SC_TEST_EXPECT(memory == virtualMemory.memory);
    SC_TEST_EXPECT(memcmp(memory, reference, lessThanOnePageSize) == 0);

    // Decommit everything (not really needed if we're going to release() soon)
    SC_TEST_EXPECT(virtualMemory.shrink(0));
    SC_TEST_EXPECT(memory == virtualMemory.memory);

    // Finally release (don't forget, VirtualMemory has no destructor!)
    SC_TEST_EXPECT(virtualMemory.release());
    SC_TEST_EXPECT(virtualMemory.memory == nullptr);
    //! [VirtualMemorySnippet]
}

namespace SC
{
void runVirtualMemoryTest(SC::TestReport& report) { VirtualMemoryTest test(report); }
} // namespace SC
