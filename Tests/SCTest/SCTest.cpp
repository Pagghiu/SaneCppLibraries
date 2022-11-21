// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../../Libraries/Foundation/ArrayTest.h"
#include "../../Libraries/Foundation/ConsoleTest.h"
#include "../../Libraries/Foundation/FunctionTest.h"
#include "../../Libraries/Foundation/MapTest.h"
#include "../../Libraries/Foundation/MemoryTest.h"
#include "../../Libraries/Foundation/OSTest.h"
#include "../../Libraries/Foundation/ReflectionTest.h"
#include "../../Libraries/Foundation/ResultTest.h"
#include "../../Libraries/Foundation/SerializationTemplateTest.h"
#include "../../Libraries/Foundation/SerializationTypeErasedTest.h"
#include "../../Libraries/Foundation/SmallVectorTest.h"
#include "../../Libraries/Foundation/StringBuilderTest.h"
#include "../../Libraries/Foundation/StringFunctionsTest.h"
#include "../../Libraries/Foundation/StringTest.h"
#include "../../Libraries/Foundation/StringViewTest.h"
#include "../../Libraries/Foundation/VectorTest.h"

int main(int argc, const char* argv[])
{
    using namespace SC;
    TestReport report(argc, argv);
    report.debugBreakOnFailedTest = true;
    // clang-format off
    { OSTest                        test(report); }
    { ConsoleTest                   test(report); }
    { MemoryTest                    test(report); }
    { ArrayTest                     test(report); }
    { VectorTest                    test(report); }
    { ResultTest                    test(report); }
    { StringViewTest                test(report); }
    { StringFunctionsTest           test(report); }
    { StringTest                    test(report); }
    { StringBuilderTest             test(report); }
    { MapTest                       test(report); }
    { SmallVectorTest               test(report); }
    { FunctionTest                  test(report); }
    { ReflectionTest                test(report); }
    { SerializationTypeErasedTest   test(report); }
    { SerializationTemplateTest     test(report); }
    // clang-format on

    return report.getTestReturnCode();
}
