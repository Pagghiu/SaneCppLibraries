// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../../Testing/Testing.h"
#include "../UniqueHandle.h"

namespace SC
{
struct OpaqueTest;
}

struct SC::OpaqueTest : public SC::TestCase
{
    struct HandleDefinition
    {
        using Handle                    = int;
        static constexpr Handle Invalid = -1;
        static bool&            getDeleteCalled()
        {
            static bool deleteCalled = false;
            return deleteCalled;
        }
        static bool releaseHandle(int&)
        {
            getDeleteCalled() = true;
            return true;
        }
    };

    OpaqueTest(SC::TestReport& report) : TestCase(report, "OpaqueTest")
    {
        using namespace SC;
        if (test_section("UniqueHandle"))
        {
            HandleDefinition::getDeleteCalled() = false;
            UniqueHandle<HandleDefinition> myInt;
            {
                SC_TEST_EXPECT(not HandleDefinition::getDeleteCalled());
                SC_TEST_EXPECT(not myInt.isValid());
                SC_TEST_EXPECT(myInt.assign(1));
                SC_TEST_EXPECT(not HandleDefinition::getDeleteCalled());
                SC_TEST_EXPECT(myInt.isValid());
            }
            SC_TEST_EXPECT(myInt.close());
            SC_TEST_EXPECT(HandleDefinition::getDeleteCalled());
            HandleDefinition::getDeleteCalled() = false;
            myInt.detach();
            SC_TEST_EXPECT(not myInt.isValid());
            decltype(myInt) myInt2 = 12;
            SC_TEST_EXPECT(myInt2.isValid());
            SC_TEST_EXPECT(not HandleDefinition::getDeleteCalled());
            int handleValue;
            SC_TEST_EXPECT(myInt2.get(handleValue, false));
            SC_TEST_EXPECT(handleValue == 12);
            SC_TEST_EXPECT(myInt2.close());
            SC_TEST_EXPECT(not myInt2.get(handleValue, false));
            SC_TEST_EXPECT(HandleDefinition::getDeleteCalled());
        }
    }
};

namespace SC
{
void runOpaqueTest(SC::TestReport& report) { OpaqueTest test(report); }
} // namespace SC
