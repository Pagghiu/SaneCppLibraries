// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Function.h"
#include "Test.h"

namespace SC
{
struct FunctionTest;
}

struct SC::FunctionTest : public SC::TestCase
{
    struct TestClass
    {
        TestClass() = default;

        void setValue(int value) { data = value; }
        int  getValue() const { return data; }

        static int freeFunc(int value) { return value + 1; }

      private:
        int data = 0;
    };

    FunctionTest(SC::TestReport& report) : TestCase(report, "FunctionTest")
    {
        using namespace SC;

        if (test_section("bind"))
        {

            TestClass           tc;
            Function<void(int)> setValue;
            Function<int(void)> getValue;
            Function<int(int)>  freeFunc;
            setValue.bind<TestClass, &TestClass::setValue>(&tc);
            getValue.bind<TestClass, &TestClass::getValue>(&tc);
            freeFunc.bind<&TestClass::freeFunc>();
            SC_TEST_EXPECT(getValue() == 0);
            setValue(3);
            SC_TEST_EXPECT(getValue() == 3);
            SC_TEST_EXPECT(freeFunc(2) == 3);
        }
        if (test_section("deduct"))
        {
            TestClass tc;

            auto freeFunc = SC::FunctionDeducer(&TestClass::freeFunc).Bind<&TestClass::freeFunc>();
            auto setValue = SC::FunctionDeducer(&TestClass::setValue).Bind<&TestClass::setValue>(&tc);
            auto getValue = SC_FUNCTION_MEMBER(&TestClass::getValue, &tc);

            Function<int(int)> lambdaFreeFunc = &TestClass::freeFunc;
            Function<int(int)> lambdaCopy;
            Function<int(int)> lambdaMove;
            {
                uint32_t val1 = 1;
                uint32_t val2 = 1;
                uint64_t val3 = 1;

                Function<int(int)> lambda = [=](int value) -> int
                { return static_cast<int>(val1 + val2 + val3 + value); };
                SC_TEST_EXPECT(lambda(2) == 5);

                SC_TEST_EXPECT(freeFunc(23) == 24);
                SC_TEST_EXPECT(getValue() == 0);
                setValue(3);
                SC_TEST_EXPECT(getValue() == 3);
                lambdaCopy = lambda;
                lambdaMove = move(lambda);
            }
            SC_TEST_EXPECT(lambdaCopy(2) == 5);
            SC_TEST_EXPECT(lambdaMove(2) == 5);
        }
    }
};
