// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.

// Foundation
#include "../../Libraries/Foundation/ArrayTest.h"
#include "../../Libraries/Foundation/ConsoleTest.h"
#include "../../Libraries/Foundation/FunctionTest.h"
#include "../../Libraries/Foundation/MapTest.h"
#include "../../Libraries/Foundation/MemoryTest.h"
#include "../../Libraries/Foundation/MovableHandleTest.h"
#include "../../Libraries/Foundation/OptionalTest.h"
#include "../../Libraries/Foundation/PathTest.h"
#include "../../Libraries/Foundation/ResultTest.h"
#include "../../Libraries/Foundation/SmallVectorTest.h"
#include "../../Libraries/Foundation/StringBuilderTest.h"
#include "../../Libraries/Foundation/StringTest.h"
#include "../../Libraries/Foundation/StringViewTest.h"
#include "../../Libraries/Foundation/SystemTest.h"
#include "../../Libraries/Foundation/VectorTest.h"

// InputOutput
#include "../../Libraries/InputOutput/FileSystemTest.h"
#include "../../Libraries/InputOutput/FileSystemWalkerTest.h"
#include "../../Libraries/InputOutput/ProcessTest.h"

// Reflection
#include "../../Libraries/Reflection/ReflectionTest.h"

// Serialization
#include "../../Libraries/Serialization/SerializationTemplateTest.h"
#include "../../Libraries/Serialization/SerializationTypeErasedTest.h"

// Threading
#include "../../Libraries/Threading/ThreadingTest.h"

SC::SmallVector<char, 1024 * sizeof(SC::utf_char_t)> globalConsoleBuffer;
SC::SmallVector<char, 1024 * sizeof(SC::utf_char_t)> formatConsoleBuffer;

int main(int argc, const char* argv[])
{
    using namespace SC;
    SystemDirectories directories;
    if (not directories.init())
        return -2;
    Console    console(globalConsoleBuffer, formatConsoleBuffer);
    TestReport report(console, directories, argc, argv);
    report.debugBreakOnFailedTest = true;
    // clang-format off

    // Foundation tests
    { ArrayTest                     test(report); }
    { ConsoleTest                   test(report); }
    { FunctionTest                  test(report); }
    { MapTest                       test(report); }
    { MemoryTest                    test(report); }
    { MovableHandleTest             test(report); }
    { OptionalTest                  test(report); }
    { PathTest                      test(report); }
    { ResultTest                    test(report); }
    { SmallVectorTest               test(report); }
    { StringBuilderTest             test(report); }
    { StringTest                    test(report); }
    { StringViewTest                test(report); }
    { SystemTest                    test(report); }
    { VectorTest                    test(report); }

    // InputOutput tests
    { FileSystemTest                test(report); }
    { FileSystemWalkerTest          test(report); }
    { ProcessTest                   test(report); }

    // Reflection tests
    { ReflectionTest                test(report); }

    // Serialization tests
    { SerializationTemplateTest     test(report); }
    { SerializationTypeErasedTest   test(report); }

    // Threading tests
    { ThreadingTest                 test(report); }

    // clang-format on

    return report.getTestReturnCode();
}
