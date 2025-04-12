// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Foundation/Buffer.h"
#include "Libraries/Testing/Testing.h"
namespace SC
{
struct BufferTest;
} // namespace SC

struct SC::BufferTest : public SC::TestCase
{
    BufferTest(SC::TestReport& report) : TestCase(report, "BufferTest")
    {
        if (test_section("Basic"))
        {
            basic();
        }
        if (test_section("reserve / resizeWithoutInitializing"))
        {
            Buffer buffer;
            SC_TEST_EXPECT(buffer.capacity() == 0);
            SC_TEST_EXPECT(buffer.size() == 0);
            SC_TEST_EXPECT(buffer.reserve(10));
            SC_TEST_EXPECT(buffer.capacity() == 10);
            SC_TEST_EXPECT(buffer.size() == 0);
            SC_TEST_EXPECT(buffer.resizeWithoutInitializing(10));
            SC_TEST_EXPECT(buffer.capacity() == 10);
            SC_TEST_EXPECT(buffer.size() == 10);
            SC_TEST_EXPECT(buffer.reserve(20));
            SC_TEST_EXPECT(buffer.capacity() == 20);
            SC_TEST_EXPECT(buffer.size() == 10);
            SC_TEST_EXPECT(buffer.resizeWithoutInitializing(30));
            SC_TEST_EXPECT(buffer.capacity() == 30);
            SC_TEST_EXPECT(buffer.size() == 30);
        }
        if (test_section("append"))
        {
            Buffer buffer;
            SC_TEST_EXPECT(buffer.append({"ciao"}));
            SC_TEST_EXPECT(buffer.size() == 5);
            SC_TEST_EXPECT(memcmp(buffer.data(), "ciao\0", 5) == 0);
            SC_TEST_EXPECT(buffer.append({"yeah"}));
            SC_TEST_EXPECT(buffer.size() == 10);
            SC_TEST_EXPECT(memcmp(buffer.data(), "ciao\0yeah\0", 10) == 0);
            SC_TEST_EXPECT(buffer.append({"woow"}));
            SC_TEST_EXPECT(buffer.size() == 15);
            SC_TEST_EXPECT(buffer.removeRange(5, 5));
            SC_TEST_EXPECT(buffer.size() == 10);
            SC_TEST_EXPECT(memcmp(buffer.data(), "ciao\0woow\0", 10) == 0);
            SC_TEST_EXPECT(buffer.insert(5, "salve"));
            SC_TEST_EXPECT(memcmp(buffer.data(), "ciao\0salve\0woow\0", 16) == 0);
            SC_TEST_EXPECT(buffer.removeAt(0));
            SC_TEST_EXPECT(buffer[0] == 'i');
        }
        if (test_section("Buffer"))
        {
            Buffer buffer;
            SC_TEST_EXPECT(not buffer.isInline());
            SC_TEST_EXPECT(buffer.size() == 0);
            SC_TEST_EXPECT(buffer.capacity() == 0);
            SC_TEST_EXPECT(buffer.isInline() == false);
            resizeTest(buffer);
            SC_TEST_EXPECT(buffer.isInline() == false);
        }

        if (test_section("SmallBuffer"))
        {
            SmallBuffer<12> buffer;
            SC_TEST_EXPECT(buffer.isInline());
            SC_TEST_EXPECT(buffer.size() == 0);
            SC_TEST_EXPECT(buffer.capacity() == 12);
            SC_TEST_EXPECT(buffer.isInline() == true);
            resizeTest(buffer);
            SC_TEST_EXPECT(buffer.isInline() == true);
        }

        if (test_section("Buffer / SmallBuffer"))
        {
            Buffer          buffer0;
            SmallBuffer<13> buffer1 = buffer0;
            buffer1                 = buffer0;
            SmallBuffer<12> buffer2 = buffer1;
            buffer2                 = buffer1;
        }

        parametricTest<Buffer, 8, 16, SmallBuffer<8>, SmallBuffer<4>, true>();
        parametricTest<Buffer, 8, 16, SmallBuffer<8>, SmallBuffer<4>, false>();
        parametricTest<SmallBuffer<4>, 8, 16, SmallBuffer<8>, Buffer, true>();
        parametricTest<SmallBuffer<4>, 8, 16, SmallBuffer<8>, Buffer, false>();
        parametricTest<SmallBuffer<8>, 8, 16, Buffer, SmallBuffer<4>, true>();
        parametricTest<SmallBuffer<8>, 8, 16, Buffer, SmallBuffer<4>, false>();
    }

    void basic();

    template <typename Buffer1, int Resize1, int Resize2, typename Buffer2, typename Buffer3, bool copy>
    void parametricTest()
    {
        if (test_section("CONSTRUCTOR Buffer1->Buffer2"))
        {
            Buffer1 buffer0;
            SC_TEST_EXPECT(buffer0.resizeWithoutInitializing(Resize1));
            buffer0.clear();
            SC_TEST_EXPECT(buffer0.resize(Resize1, 1));
            auto    size    = buffer0.size();
            Buffer2 buffer1 = copy ? buffer0 : move(buffer0);
            SC_TEST_EXPECT(checkEqual(buffer1, 1, size));
        }

        if (test_section("CONSTRUCTOR2 Buffer1->Buffer3"))
        {
            Buffer1 buffer0;
            SC_TEST_EXPECT(buffer0.resizeWithoutInitializing(Resize1));
            buffer0.clear();
            SC_TEST_EXPECT(buffer0.resize(Resize1, 1));
            auto    size    = buffer0.size();
            Buffer3 buffer1 = copy ? buffer0 : move(buffer0);
            SC_TEST_EXPECT(checkEqual(buffer1, 1, size));
        }

        if (test_section("ASSIGNMENT Buffer1->Buffer2"))
        {
            Buffer1 buffer0;
            SC_TEST_EXPECT(buffer0.resizeWithoutInitializing(Resize1));
            buffer0.clear();
            SC_TEST_EXPECT(buffer0.resize(Resize1, 1));
            auto    size = buffer0.size();
            Buffer2 buffer1;
            buffer1 = copy ? buffer0 : move(buffer0);
            SC_TEST_EXPECT(checkEqual(buffer1, 1, size));
        }

        if (test_section("ASSIGNMENT Buffer1->Buffer3"))
        {
            Buffer1 buffer0;
            SC_TEST_EXPECT(buffer0.resizeWithoutInitializing(Resize1));
            buffer0.clear();
            SC_TEST_EXPECT(buffer0.resize(Resize1, 1));
            auto    size = buffer0.size();
            Buffer3 buffer1;
            buffer1 = copy ? buffer0 : move(buffer0);
            SC_TEST_EXPECT(checkEqual(buffer1, 1, size));
        }

        if (test_section("ASSIGNMENT Buffer1->Buffer2 Resize2"))
        {
            Buffer1 buffer0;
            SC_TEST_EXPECT(buffer0.resizeWithoutInitializing(Resize1));
            buffer0.clear();
            SC_TEST_EXPECT(buffer0.resize(Resize1, 1));
            auto    size = buffer0.size();
            Buffer2 buffer1;
            SC_TEST_EXPECT(buffer1.resizeWithoutInitializing(Resize2));
            SC_TEST_EXPECT(not buffer1.isInline());
            buffer1.clear();
            SC_TEST_EXPECT(buffer1.resize(Resize2, 2));
            buffer1 = copy ? buffer0 : move(buffer0);
            SC_TEST_EXPECT(checkEqual(buffer1, 1, size));
        }

        if (test_section("ASSIGNMENT Buffer1->Buffer3 Resize2"))
        {
            Buffer1 buffer0;
            SC_TEST_EXPECT(buffer0.resizeWithoutInitializing(Resize1));
            buffer0.clear();
            SC_TEST_EXPECT(buffer0.resize(Resize1, 1));
            auto    size = buffer0.size();
            Buffer3 buffer1;
            SC_TEST_EXPECT(buffer1.resizeWithoutInitializing(Resize2));
            buffer1.clear();
            SC_TEST_EXPECT(buffer1.resize(Resize2, 2));
            buffer1 = copy ? buffer0 : move(buffer0);
            SC_TEST_EXPECT(checkEqual(buffer1, 1, size));
        }
    }

    [[nodiscard]] static bool checkEqual(Buffer& buffer, char value, size_t sz)
    {
        if (sz > buffer.size())
            return false;
        char* data = buffer.data();
        for (size_t idx = 0; idx < sz; ++idx)
        {
            if (data[idx] != value)
                return false;
        }
        return true;
    }

    void resizeTest(Buffer& buffer)
    {
        constexpr size_t lower  = 10;
        constexpr size_t middle = 12;
        constexpr size_t higher = 16;
        SC_TEST_EXPECT(not buffer.resizeWithoutInitializing(detail::SegmentHeader::MaxCapacity + 1));
        SC_TEST_EXPECT(buffer.resizeWithoutInitializing(middle));
        constexpr char value1 = 64;
        constexpr char value2 = 32;
        buffer.clear();
        SC_TEST_EXPECT(buffer.resize(middle, value1));
        SC_TEST_EXPECT(buffer.size() == middle);
        SC_TEST_EXPECT(buffer.capacity() == middle);
        SC_TEST_EXPECT(checkEqual(buffer, value1, middle));
        SC_TEST_EXPECT(buffer.resizeWithoutInitializing(lower));
        SC_TEST_EXPECT(buffer.size() == lower);
        SC_TEST_EXPECT(buffer.capacity() == middle);
        SC_TEST_EXPECT(checkEqual(buffer, value1, lower));
        buffer.clear();
        SC_TEST_EXPECT(buffer.resize(lower, value2));
        SC_TEST_EXPECT(buffer.resizeWithoutInitializing(higher));
        SC_TEST_EXPECT(buffer.size() == higher);
        SC_TEST_EXPECT(buffer.capacity() == higher);
        SC_TEST_EXPECT(checkEqual(buffer, value2, lower));
        SC_TEST_EXPECT(buffer.resizeWithoutInitializing(middle));
        SC_TEST_EXPECT(checkEqual(buffer, value2, lower));
        SC_TEST_EXPECT(buffer.capacity() == higher);
        SC_TEST_EXPECT(buffer.shrink_to_fit());
        SC_TEST_EXPECT(buffer.capacity() == middle);
    }
};

namespace SC
{
//! [BufferBasicSnippet]
bool funcRequiringBuffer(Buffer& buffer)
{
    for (size_t idx = 0; idx < buffer.size(); ++idx)
    {
        if (buffer[idx] != 123)
            return false;
    }
    return true;
}

void BufferTest::basic()
{
    Buffer buffer;
    // Allocate 16 bytes
    SC_TEST_EXPECT(buffer.resizeWithoutInitializing(16));

    // Buffer is not inline (it's heap allocated)
    SC_TEST_EXPECT(not buffer.isInline());

    // Fill buffer with a value
    buffer.clear();
    SC_TEST_EXPECT(buffer.resize(buffer.capacity(), 123));
    funcRequiringBuffer(buffer);

    // Declare a buffer with inline capacity of 128 bytes
    SmallBuffer<128> smallBuffer;

    // copy buffer (will not allocate)
    smallBuffer = buffer;

    // smallBuffer is using inline buffer (no heap allocation)
    SC_TEST_EXPECT(smallBuffer.isInline());
    SC_TEST_EXPECT(smallBuffer.size() == 16);
    SC_TEST_EXPECT(smallBuffer.capacity() == 128);

    // SmallBuffer can be passed in place of regular Buffer
    funcRequiringBuffer(smallBuffer);

    SC_TEST_EXPECT(buffer.resizeWithoutInitializing(1024));

    // SmallBuffer now will allocate 1024 bytes
    // by using assignCopy instead of assignment operator
    // caller can check for allocation failure
    SC_TEST_EXPECT(smallBuffer.assign(buffer.toSpanConst()));
    SC_TEST_EXPECT(not smallBuffer.isInline());
    SC_TEST_EXPECT(smallBuffer.size() == 1024);
    SC_TEST_EXPECT(smallBuffer.capacity() == 1024);

    // Allocate 2kb on another buffer
    Buffer buffer2;
    SC_TEST_EXPECT(buffer2.resizeWithoutInitializing(2048));

    // SmallBuffer will "steal" the 2Kb buffer
    smallBuffer = move(buffer2);

    SC_TEST_EXPECT(smallBuffer.size() == 2048);
    SC_TEST_EXPECT(smallBuffer.capacity() == 2048);
    SC_TEST_EXPECT(buffer2.isEmpty());

    // Resize small buffer to its original capacity
    SC_TEST_EXPECT(smallBuffer.resizeWithoutInitializing(128));

    // The heap block is still in use
    SC_TEST_EXPECT(not smallBuffer.isInline());
    SC_TEST_EXPECT(smallBuffer.capacity() == 2048);

    // Shrinking it will restore its original inline buffer
    SC_TEST_EXPECT(smallBuffer.shrink_to_fit());

    // And verify that that's actually true
    SC_TEST_EXPECT(smallBuffer.isInline());
    SC_TEST_EXPECT(smallBuffer.capacity() == 128);
}
//! [BufferBasicSnippet]

} // namespace SC

namespace SC
{
void runBufferTest(SC::TestReport& report) { BufferTest test2(report); }
} // namespace SC
