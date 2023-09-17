// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../Testing/Test.h"
#include "Opaque.h"

namespace SC
{
struct OpaqueTest;
}

struct SC::OpaqueTest : public SC::TestCase
{
    struct MyTraits
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
        if (test_section("UniqueTaggedHandle"))
        {
            MyTraits::getDeleteCalled() = false;
            UniqueTaggedHandleTraits<MyTraits> myInt;
            {
                SC_TEST_EXPECT(not MyTraits::getDeleteCalled());
                SC_TEST_EXPECT(not myInt.isValid());
                SC_TEST_EXPECT(myInt.assign(1));
                SC_TEST_EXPECT(not MyTraits::getDeleteCalled());
                SC_TEST_EXPECT(myInt.isValid());
            }
            SC_TEST_EXPECT(myInt.close());
            SC_TEST_EXPECT(MyTraits::getDeleteCalled());
            MyTraits::getDeleteCalled() = false;
            myInt.detach();
            SC_TEST_EXPECT(not myInt.isValid());
            decltype(myInt) myInt2 = 12;
            SC_TEST_EXPECT(myInt2.isValid());
            SC_TEST_EXPECT(not MyTraits::getDeleteCalled());
            int handleValue;
            SC_TEST_EXPECT(myInt2.get(handleValue, false));
            SC_TEST_EXPECT(handleValue == 12);
            SC_TEST_EXPECT(myInt2.close());
            SC_TEST_EXPECT(not myInt2.get(handleValue, false));
            SC_TEST_EXPECT(MyTraits::getDeleteCalled());
        }
    }
};
