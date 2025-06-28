// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Containers/Vector.h"
#include "Libraries/Containers/VectorMap.h"
#include "Libraries/Containers/VectorSet.h"
#include "Libraries/Memory/VirtualMemory.h"
#include "Libraries/Strings/String.h"
#include "Libraries/Testing/Testing.h"

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
        if (test_section("virtual memory dump"))
        {
            virtualMemoryDump();
        }
    }

    void virtualGlobal();
    void virtualMemoryDump();
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

void SC::GlobalsContainerTest::virtualMemoryDump()
{
    //! [GlobalContainerVirtualMemoryDumpSnippet]
    // -----------------------------------------------------------------------------
    // Example showing how to dump and restore a complex struct to a flat buffer.
    // SC::Segment based containers use relative pointers to make this possible.
    // DO NOT use this approach when versioning is needed, that means needing to
    // de-serialize after adding, removing or moving fields in the structure.
    // In such cases consider using SC::SerializationBinary (versioned reflection).
    // -----------------------------------------------------------------------------
    struct NestedStruct
    {
        VectorMap<String, int> someMap;
        VectorSet<int>         someSet;
    };
    struct ComplexStruct
    {
        Vector<String> someStrings;
        int            someField = 0;
        String         singleString;
        NestedStruct   nestedStruct;
    };

    Buffer memoryDump;

    // Setup a Virtual Memory allocator with the max upper memory bound
    VirtualMemory virtualMemory;
    SC_TEST_EXPECT(virtualMemory.reserve(1024 * 1024)); // 1MB is enough here
    VirtualAllocator allocator = {virtualMemory};
    Globals          globals   = {allocator};

    // Make the allocator current before creating a ComplexStruct
    Globals::push(Globals::Global, globals);
    ComplexStruct& object = *allocator.create<ComplexStruct>();
    object.someField      = 42;
    object.singleString   = "ASDF";
    object.someStrings    = {"First", "Second"};
    SC_TEST_EXPECT(object.nestedStruct.someSet.insert(213));
    SC_TEST_EXPECT(object.nestedStruct.someMap.insertIfNotExists({"1", 1}));

    // Save used bytes to memoryDump, checking that one page has been committed
    Span<const void> memory = {allocator.data(), allocator.size()};
    SC_TEST_EXPECT(virtualMemory.committedBytes == virtualMemory.getPageSize());
    SC_TEST_EXPECT(memory.sizeInBytes() < virtualMemory.getPageSize());
    SC_TEST_EXPECT(memory.data() == &object);
    SC_TEST_EXPECT((size_t(memoryDump.data()) % alignof(ComplexStruct)) == 0);
    Globals::pop(Globals::Global);

    // Dump AFTER Globals::pop, using default allocator, and release virtual memory
    SC_TEST_EXPECT(memoryDump.append(memory));
    SC_TEST_EXPECT(virtualMemory.release());

    // -----------------------------------------------------------------------------
    // Obtain a read-only view over ComplexStruct by re-interpreting the memory dump
    // NOTE: There's no need to call ComplexStruct destructor at end of scope
    // WARN: start_lifetime_as obtains a ComplexStruct with proper lifetime.
    // It works on all tested compilers (debug and release) but it's not technically
    // UB-free as ComplexStruct is not implicit-lifetime.
    // -----------------------------------------------------------------------------
    const Span<const void> span     = memoryDump.toSpanConst();
    const ComplexStruct&   readonly = *span.start_lifetime_as<const ComplexStruct>();
    SC_TEST_EXPECT(readonly.someField == 42);
    SC_TEST_EXPECT(readonly.singleString == "ASDF");
    SC_TEST_EXPECT(readonly.someStrings[0] == "First");
    SC_TEST_EXPECT(readonly.someStrings[1] == "Second");
    SC_TEST_EXPECT(readonly.someStrings.size() == 2);
    SC_TEST_EXPECT(readonly.nestedStruct.someSet.size() == 1);
    SC_TEST_EXPECT(readonly.nestedStruct.someSet.contains(213));
    SC_TEST_EXPECT(*readonly.nestedStruct.someMap.get("1") == 1);

    // -----------------------------------------------------------------------------
    // To modify the struct again, copy the read-only view to a new object.
    // A Fixed or Virtual allocator can be used here to group sparse allocations in
    // a nice single contiguous buffer, before dumping it again to disk or network.
    // -----------------------------------------------------------------------------
    ComplexStruct modifiable = readonly;
    SC_TEST_EXPECT(modifiable.someStrings[0] == "First");
    modifiable.someStrings[0] = "First modified";
    SC_TEST_EXPECT(modifiable.someStrings[0] == "First modified");
    //! [GlobalContainerVirtualMemoryDumpSnippet]
}

namespace SC
{
void runGlobalsContainerTest(SC::TestReport& report) { GlobalsContainerTest test(report); }
} // namespace SC
