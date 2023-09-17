// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../Testing/Test.h"
#include "Function.h"

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
            Function<int(void)> getValueCopy = getValue;
            Function<int(void)> getValueMove = move(getValue);
            SC_TEST_EXPECT(not getValue.isValid()); // it has been moved
            SC_TEST_EXPECT(getValueCopy() == 3);
            SC_TEST_EXPECT(getValueMove() == 3);
            SC_TEST_EXPECT(freeFunc(2) == 3);
        }
        if (test_section("deduct"))
        {
            TestClass tc;

            auto freeFunc = SC::FunctionDeducer(&TestClass::freeFunc).Bind<&TestClass::freeFunc>();
            auto setValue = SC::FunctionDeducer(&TestClass::setValue).Bind<&TestClass::setValue>(&tc);
            auto getValue = SC_FUNCTION_MEMBER(&TestClass::getValue, &tc);

            Function<int(int)> lambdaFreeFunc  = &TestClass::freeFunc;
            Function<int(int)> lambdaFreeFunc2 = lambdaFreeFunc;        // Copy Construct
            Function<int(int)> lambdaFreeFunc3 = move(lambdaFreeFunc2); // Move Construct
            Function<int(int)> lambdaCopy;
            Function<int(int)> lambdaMove;
            {
                uint32_t val1 = 1;
                uint32_t val2 = 1;
                uint64_t val3 = 1;

                Function<int(int)> lambda = [=](int value) -> int
                { return static_cast<int>(val1 + val2 + val3 + static_cast<uint32_t>(value)); };
                SC_TEST_EXPECT(lambda(2) == 5);
                auto               func    = [](int) -> int { return 1; };
                Function<int(int)> lambda2 = func;
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
        if (test_section("reference"))
        {
            // Try assigning a lanbda with a reference
            Function<void(int&)> incrementWithReference = [](int& val) { val += 1; };

            int val = 0;
            incrementWithReference(val);
            SC_TEST_EXPECT(val == 1);

            // Upcast lambda with const reference to non const reference
            Function<void(int&)> constReference = [&](const int& val) { SC_TEST_EXPECT(val == 1); };
            constReference(val);
        }
    }
};
