// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Opaque.h"
#include "Test.h"

namespace SC
{
struct OpaqueTest;
}

struct SC::OpaqueTest : public SC::TestCase
{
    struct MyDeleter
    {
        static bool& getDeleteCalled()
        {
            static bool deleteCalled = false;
            return deleteCalled;
        }
        static bool deleteHandle(int& sad)
        {
            getDeleteCalled() = true;
            return true;
        }
    };
    OpaqueTest(SC::TestReport& report) : TestCase(report, "OpaqueTest")
    {
        using namespace SC;
        if (test_section("OpaqueUniqueTaggedHandle"))
        {
            MyDeleter::getDeleteCalled() = false;
            OpaqueUniqueTaggedHandle<int, -1, bool, &MyDeleter::deleteHandle> myInt;
            {
                SC_TEST_EXPECT(not MyDeleter::getDeleteCalled());
                SC_TEST_EXPECT(not myInt);
                SC_TEST_EXPECT(myInt.assign(1));
                SC_TEST_EXPECT(not MyDeleter::getDeleteCalled());
                SC_TEST_EXPECT(myInt);
            }
            SC_TEST_EXPECT(myInt.close());
            SC_TEST_EXPECT(MyDeleter::getDeleteCalled());
            MyDeleter::getDeleteCalled() = false;
            myInt.detach();
            SC_TEST_EXPECT(not myInt);
            decltype(myInt) myInt2 = 12;
            SC_TEST_EXPECT(myInt2);
            SC_TEST_EXPECT(not MyDeleter::getDeleteCalled());
            int handleValue;
            SC_TEST_EXPECT(myInt2.get(handleValue, false));
            SC_TEST_EXPECT(handleValue == 12);
            SC_TEST_EXPECT(myInt2.close());
            SC_TEST_EXPECT(not myInt2.get(handleValue, false));
            SC_TEST_EXPECT(MyDeleter::getDeleteCalled());
        }
    }
};
