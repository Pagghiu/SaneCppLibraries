// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Extra/Deprecated/Libraries/SerializationBinaryTypeErased/SerializationBinaryTypeErased.h"
#include "Libraries/ContainersReflection/ContainersReflection.h"
#include "Libraries/ContainersReflection/MemoryReflection.h"
#include "Libraries/Testing/Testing.h"
#include "Tests/Libraries/SerializationBinary/SerializationSuiteTest.h"
#include <memory.h> // memset

namespace SC
{
struct SerializationBinaryTypeErasedTest;

}
struct SC::SerializationBinaryTypeErasedTest : public SC::SerializationSuiteTest::SerializationTest
{
    SerializationBinaryTypeErasedTest(SC::TestReport& report)
        : SerializationTest(report, "SerializationBinaryTypeErasedTest")
    {
        runSameVersionTests<SerializationBinaryTypeErased, SerializationBinaryTypeErased>();
        runVersionedTests<SerializationBinaryTypeErased, SerializationBinaryTypeErased, Reflection::SchemaTypeErased>();
        if (test_section("ArrayAccess lookup bounds"))
        {
            arrayAccessLookupBounds();
        }
    }

    void arrayAccessLookupBounds()
    {
        using Access = detail::SerializationBinaryTypeErasedArrayAccess;
        struct Spy
        {
            uint32_t         mutableGet = 0;
            mutable uint32_t constGet   = 0;

            uint32_t initializedResize   = 0;
            uint32_t uninitializedResize = 0;
        };
        Access::VectorVTable entries[2];
        for (uint32_t index = 0; index < 2; ++index)
        {
            entries[index].linkID         = index + 1;
            entries[index].getSegmentSpan = [](Reflection::TypeInfo, Span<char> object, Span<char>& output)
            {
                ++reinterpret_cast<Spy*>(object.data())->mutableGet;
                output = object;
                return true;
            };
            entries[index].getSegmentSpanConst =
                [](Reflection::TypeInfo, Span<const char> object, Span<const char>& output)
            {
                ++reinterpret_cast<const Spy*>(object.data())->constGet;
                output = object;
                return true;
            };
            entries[index].resize = [](Span<char> object, Reflection::TypeInfo, uint64_t, Access::DropExcessItems)
            {
                ++reinterpret_cast<Spy*>(object.data())->initializedResize;
                return true;
            };
            entries[index].resizeWithoutInitialize =
                [](Span<char> object, Reflection::TypeInfo, uint64_t, Access::DropExcessItems)
            {
                ++reinterpret_cast<Spy*>(object.data())->uninitializedResize;
                return true;
            };
        }
        const auto checkLookup = [&](size_t count, uint32_t linkID, bool expected)
        {
            Access access;
            access.vectorVtable = {entries, count};
            Spy              spy;
            const auto       object      = Span<char>::reinterpret_object(spy);
            const auto       constObject = Span<const char>::reinterpret_object(spy);
            Span<char>       output;
            Span<const char> constOutput;
            const uint32_t   expectedCalls = expected ? 1 : 0;

            SC_TEST_EXPECT(access.getSegmentSpan(linkID, {}, object, output) == expected);
            SC_TEST_EXPECT(spy.mutableGet == expectedCalls);
            SC_TEST_EXPECT(output.data() == (expected ? object.data() : nullptr));
            SC_TEST_EXPECT(access.getSegmentSpan(linkID, {}, constObject, constOutput) == expected);
            SC_TEST_EXPECT(spy.constGet == expectedCalls);
            SC_TEST_EXPECT(constOutput.data() == (expected ? constObject.data() : nullptr));
            SC_TEST_EXPECT(access.resize(linkID, object, {}, 0, Access::Initialize::Yes, Access::DropExcessItems::No) ==
                           expected);
            SC_TEST_EXPECT(spy.initializedResize == expectedCalls);
            SC_TEST_EXPECT(spy.uninitializedResize == 0);
            SC_TEST_EXPECT(access.resize(linkID, object, {}, 0, Access::Initialize::No, Access::DropExcessItems::No) ==
                           expected);
            SC_TEST_EXPECT(spy.uninitializedResize == expectedCalls);
            SC_TEST_EXPECT(spy.initializedResize == expectedCalls);
        };
        checkLookup(0, 1, false); // Empty span with valid physical backing.
        checkLookup(1, 1, true);  // First entry is permitted.
        // The matching spare entry is valid physical storage but excluded from the logical span.
        // Old byte-count bounds dispatch it and stop there, so the regression needs no physical OOB read.
        checkLookup(1, 2, false);
        checkLookup(2, 2, true); // Last permitted entry, after skipping the first.
    }
};

namespace SC
{
void runSerializationBinaryTypeErasedTest(SC::TestReport& report) { SerializationBinaryTypeErasedTest test(report); }
} // namespace SC
