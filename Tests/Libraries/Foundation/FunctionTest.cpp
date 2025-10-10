// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Foundation/Function.h"
#include "Libraries/Foundation/Assert.h"
#include "Libraries/Testing/Testing.h"

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
        static int freeFunc2(int value) { return value - 1; }

      private:
        int data = 0;
    };

    struct Functor
    {
        int data = 0;

        int operator()() const { return data; }
    };
    FunctionTest(SC::TestReport& report) : TestCase(report, "FunctionTest")
    {
        using namespace SC;

        if (test_section("bind"))
        {
            bind();
        }
        if (test_section("deduct"))
        {
            deduct();
        }
        if (test_section("reference"))
        {
            reference();
        }
        if (test_section("functor"))
        {
            functor();
        }
    }
    void bind();
    void deduct();
    void reference();
    void functor();

    void functionDocumentationSnippet();
};

void SC::FunctionTest::bind()
{
    TestClass           tc;
    Function<void(int)> setValue;
    Function<int(void)> getValue;
    Function<int(int)>  freeFunc;
    setValue.bind<TestClass, &TestClass::setValue>(tc);
    getValue.bind<TestClass, &TestClass::getValue>(tc);
    freeFunc.bind<&TestClass::freeFunc>();
    SC_TEST_EXPECT(getValue() == 0);
    setValue(3);
    SC_TEST_EXPECT(getValue() == 3);
    Function<int(void)> getValueCopy = getValue;
    Function<int(void)> getValueMove = move(getValue);
#ifndef __clang_analyzer__
    SC_TEST_EXPECT(not getValue.isValid()); // it has been moved
#endif                                      // not __clang_analyzer__
    SC_TEST_EXPECT(getValueCopy() == 3);
    SC_TEST_EXPECT(getValueMove() == 3);
    SC_TEST_EXPECT(freeFunc(2) == 3);
}

void SC::FunctionTest::deduct()
{

    TestClass          tc;
    Function<int(int)> freeFunc = &TestClass::freeFunc2;
    SC_TEST_EXPECT(freeFunc(2) == 1);
    freeFunc = &TestClass::freeFunc;
    SC_TEST_EXPECT(freeFunc(2) == 3);
    Function<void(int)> setValue;
    setValue.bind<TestClass, &TestClass::setValue>(tc);
    Function<int()> getValue;
    getValue.bind<TestClass, &TestClass::getValue>(tc);

    TestClass tc2;
    SC_TEST_EXPECT(setValue.isBoundToClassInstance(&tc));
    SC_TEST_EXPECT(not setValue.isBoundToClassInstance(&tc2));

    Function<int(int)> lambdaFreeFunc  = &TestClass::freeFunc;
    Function<int(int)> lambdaFreeFunc2 = lambdaFreeFunc;        // Copy Construct
    Function<int(int)> lambdaFreeFunc3 = move(lambdaFreeFunc2); // Move Construct
    Function<int(int)> lambdaCopy;
    Function<int(int)> lambdaMove;
    {
        uint8_t  val1 = 1;
        uint16_t val2 = 1;
        uint32_t val3 = 1;

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

void SC::FunctionTest::reference()
{

    // Try assigning a lambda with a reference
    Function<void(int&)> incrementWithReference = [](int& val) { val += 1; };

    int val = 0;
    incrementWithReference(val);
    SC_TEST_EXPECT(val == 1);

    // Upcast lambda with const reference to non const reference
    Function<void(int&)> constReference = [&](const int& val) { SC_TEST_EXPECT(val == 1); };
    constReference(val);
}

//! [FunctionFunctorSnippet]

// A simple functor object with some state
struct Functor
{
    int data = 0;
    int operator()() const { return data; }
};

struct UnrelatedFunctor
{
    int data = 0;
    int operator()() const { return data; }
};

void SC::FunctionTest::functor()
{
    // Initialize the function with a stateful functor (setting data == 1)
    Function<int(void)> callback = Functor{1};
    // Check that data is actually 1
    SC_TEST_EXPECT(callback() == 1);
    // Callback cannot be cast to an unrelated Functor, even if matches 1:1 with Functor
    SC_TEST_EXPECT(callback.dynamicCastTo<UnrelatedFunctor>() == nullptr);
    // We know that a Functor was bound and we can retrieve and modify it
    callback.dynamicCastTo<Functor>()->data = 123;
    // Check that previous call succeeded in getting us the proper functor object
    SC_TEST_EXPECT(callback() == 123);
}
//! [FunctionFunctorSnippet]

//! [FunctionMainSnippet]
// A regular class with a member function
struct SomeClass
{
    float memberValue = 2.0;
    int   memberFunc(float a) { return static_cast<int>(a + memberValue); }
};

// A Functor with operator ()
struct SomeFunctor
{
    float memberValue = 2.0;
    int   operator()(float a) { return static_cast<int>(a + memberValue); }
};

// Free function
int someFunc(float a) { return static_cast<int>(a * 2); }

// Class too big to be grabbed by copy
struct BigClass
{
    SC::uint64_t values[4];
};

void SC::FunctionTest::functionDocumentationSnippet()
{
    SomeClass someClass;

    Function<int(float)> func;

    func = &someFunc;                                                // Bind free func
    func.bind<SomeClass, &SomeClass::memberFunc>(someClass);         // Bind member func
    func = [](float a) -> int { return static_cast<int>(a + 1.5); }; // Bind lambda func
    func = SomeFunctor{2.5};                                         // Bind a functor

    // If you feel brave enough you can retrieve the bound functor by knowing its type
    SC_ASSERT_RELEASE(func.dynamicCastTo<SomeFunctor>()->memberValue == 2.5f);

    // This will static_assert because sizeof(BigClass) is bigger than LAMBDA_SIZE
    // BigClass bigClass;
    // func = [bigClass](float a) -> int { return static_cast<int>(a);};
}
//! [FunctionMainSnippet]

namespace SC
{
void runFunctionTest(SC::TestReport& report) { FunctionTest test(report); }
} // namespace SC
