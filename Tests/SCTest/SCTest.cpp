// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.

// Foundation
#include "../../Libraries/Foundation/ArrayTest.h"
#include "../../Libraries/Foundation/FunctionTest.h"
#include "../../Libraries/Foundation/IntrusiveDoubleLinkedListTest.h"
#include "../../Libraries/Foundation/MapTest.h"
#include "../../Libraries/Foundation/MemoryTest.h"
#include "../../Libraries/Foundation/OpaqueTest.h"
#include "../../Libraries/Foundation/OptionalTest.h"
#include "../../Libraries/Foundation/ResultTest.h"
#include "../../Libraries/Foundation/SmallVectorTest.h"
#include "../../Libraries/Foundation/StringFormatTest.h"
#include "../../Libraries/Foundation/StringTest.h"
#include "../../Libraries/Foundation/StringViewTest.h"
#include "../../Libraries/Foundation/TaggedUnionTest.h"
#include "../../Libraries/Foundation/VectorTest.h"

// FileSystem
#include "../../Libraries/FileSystem/FileSystemTest.h"
#include "../../Libraries/FileSystem/FileSystemWalkerTest.h"
#include "../../Libraries/FileSystem/FileSystemWatcherTest.h"
#include "../../Libraries/FileSystem/PathTest.h"

// Reflection
#include "../../Libraries/Reflection/ReflectionTest.h"

// Serialization
#include "../../Libraries/Serialization/SerializationTemplateTest.h"
#include "../../Libraries/Serialization/SerializationTypeErasedTest.h"

// System
#include "../../Libraries/System/ConsoleTest.h"
#include "../../Libraries/System/ProcessTest.h"
#include "../../Libraries/System/SystemTest.h"
#include "../../Libraries/System/TimeTest.h"

// Threading
#include "../../Libraries/Threading/AtomicTest.h"
#include "../../Libraries/Threading/ThreadingTest.h"

// Async
#include "../../Libraries/Async/EventLoopTest.h"

SC::SmallVector<char, 1024 * sizeof(SC::utf_char_t)> globalConsoleConversionBuffer;

int main(int argc, const char* argv[])
{
    using namespace SC;
    SystemDirectories directories;
    if (not directories.init())
        return -2;
    Console    console(globalConsoleConversionBuffer);
    TestReport report(console, argc, argv);
    report.applicationRootDirectory = directories.applicationRootDirectory.view();
    report.debugBreakOnFailedTest   = true;
    // clang-format off

    // Foundation tests
    { ArrayTest                     test(report); }
    { FunctionTest                  test(report); }
    { IntrusiveDoubleLinkedListTest test(report); }
    { MapTest                       test(report); }
    { MemoryTest                    test(report); }
    { OpaqueTest                    test(report); }
    { OptionalTest                  test(report); }
    { ResultTest                    test(report); }
    { SmallVectorTest               test(report); }
    { StringFormatTest              test(report); }
    { StringTest                    test(report); }
    { StringViewTest                test(report); }
    { TaggedUnionTest               test(report); }
    { VectorTest                    test(report); }

    // FileSystem tests
    { FileSystemTest                test(report); }
    { FileSystemWalkerTest          test(report); }
    { FileSystemWatcherTest         test(report); }
    { PathTest                      test(report); }

    // Reflection tests
    { ReflectionTest                test(report); }

    // Serialization tests
    { SerializationTemplateTest     test(report); }
    { SerializationTypeErasedTest   test(report); }

    // System tests
    { ConsoleTest                   test(report); }
    { ProcessTest                   test(report); }
    { SystemTest                    test(report); }
    { TimeTest                      test(report); }

    // Threading tests
    { AtomicTest                    test(report); }
    { ThreadingTest                 test(report); }

    // Async tests
    { EventLoopTest                 test(report); }

    // clang-format on

    return report.getTestReturnCode();
}
