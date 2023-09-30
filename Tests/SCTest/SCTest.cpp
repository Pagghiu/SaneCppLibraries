// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.

#define SC_RUN_SPECIFIC_TEST 0

#if SC_RUN_SPECIFIC_TEST

namespace SC
{
struct TestReport;
void runSpecificTests(TestReport& report)
{
    (void)report;
    // clang-format off
    // clang-format on
}
} // namespace SC

#else

// Build
#include "../../Libraries/Build/BuildTest.h"

// Foundation
#include "../../Libraries/Foundation/Base/BaseTest.h"
#include "../../Libraries/Foundation/Containers/ArenaMapTest.h"
#include "../../Libraries/Foundation/Containers/ArrayTest.h"
#include "../../Libraries/Foundation/Containers/IntrusiveDoubleLinkedListTest.h"
#include "../../Libraries/Foundation/Containers/SmallVectorTest.h"
#include "../../Libraries/Foundation/Containers/VectorMapTest.h"
#include "../../Libraries/Foundation/Containers/VectorSetTest.h"
#include "../../Libraries/Foundation/Containers/VectorTest.h"
#include "../../Libraries/Foundation/Language/FunctionTest.h"
#include "../../Libraries/Foundation/Language/OpaqueTest.h"
#include "../../Libraries/Foundation/Language/OptionalTest.h"
#include "../../Libraries/Foundation/Language/TaggedUnionTest.h"
#include "../../Libraries/Foundation/Strings/StringConverterTest.h"
#include "../../Libraries/Foundation/Strings/StringFormatTest.h"
#include "../../Libraries/Foundation/Strings/StringTest.h"
#include "../../Libraries/Foundation/Strings/StringViewTest.h"

// File
#include "../../Libraries/File/FileDescriptorTest.h"

// FileSystem
#include "../../Libraries/FileSystem/FileSystemTest.h"
#include "../../Libraries/FileSystem/FileSystemWalkerTest.h"
#include "../../Libraries/FileSystem/FileSystemWatcherTest.h"
#include "../../Libraries/FileSystem/PathTest.h"

// Hashing
#include "../../Libraries/Hashing/HashingTest.h"

// Http
#include "../../Libraries/Http/HttpClientTest.h"
#include "../../Libraries/Http/HttpParserTest.h"
#include "../../Libraries/Http/HttpServerTest.h"
#include "../../Libraries/Http/HttpURLParserTest.h"

// JSON
#include "../../Libraries/Json/JsonFormatterTest.h"
#include "../../Libraries/Json/JsonTokenizerTest.h"

// Plugin
#include "../../Libraries/Plugin/PluginTest.h"

// Process
#include "../../Libraries/Process/ProcessTest.h"

// Reflection
#include "../../Libraries/Reflection/ReflectionTest.h"

// Serialization
#include "../../Libraries/Serialization/SerializationBinaryTemplateTest.h"
#include "../../Libraries/Serialization/SerializationBinaryTypeErasedTest.h"
#include "../../Libraries/Serialization/SerializationStructuredJsonTest.h"

// Socket
#include "../../Libraries/Socket/SocketDescriptorTest.h"

// System
#include "../../Libraries/System/ConsoleTest.h"
#include "../../Libraries/System/SystemTest.h"
#include "../../Libraries/System/TimeTest.h"

// Threading
#include "../../Libraries/Threading/AtomicTest.h"
#include "../../Libraries/Threading/ThreadingTest.h"

// Async
#include "../../Libraries/Async/EventLoopTest.h"

#include "../../Support/DebugVisualizers/DebugVisualizersTest.h"

#endif

#include "../../Libraries/FileSystem/Path.h"
#include "../../Libraries/Foundation/Containers/SmallVector.h"
#include "../../Libraries/System/System.h"
#include "../../Libraries/Testing/Test.h"

SC::Console* globalConsole;

int main(int argc, const char* argv[])
{
    SC::SmallVector<char, 1024 * sizeof(SC::native_char_t)> globalConsoleConversionBuffer;
    using namespace SC;
    SystemDirectories directories;
    if (not directories.init())
        return -2;
    SystemFunctions functions;
    if (not functions.initNetworking())
        return -3;
    Console              console(globalConsoleConversionBuffer);
    SC::SmallString<255> correctedPath;
    TestReport           report(console, argc, argv);
    report.applicationRootDirectory = directories.applicationRootDirectory.view();
    report.executableFile           = directories.executableFile.view();
    {
        SmallVector<StringView, 50> components;
        (void)Path::normalizeUNCAndTrimQuotes(SC_COMPILER_LIBRARY_PATH, components, correctedPath, Path::AsNative);
        // If you hit this assertion you must figure out a way to derive location of Libraries
        SC_ASSERT_RELEASE(Path::isAbsolute(correctedPath.view(), SC::Path::AsNative));
    }
    report.libraryRootDirectory   = correctedPath.view();
    report.debugBreakOnFailedTest = true;

    globalConsole = &console;
    // clang-format off
#if SC_RUN_SPECIFIC_TEST
    runSpecificTests(report);
#else

    // Foundation tests
    { ArenaMapTest                  test(report); }
    { ArrayTest                     test(report); }
    { BaseTest                      test(report); }
    { FunctionTest                  test(report); }
    { IntrusiveDoubleLinkedListTest test(report); }
    { OpaqueTest                    test(report); }
    { OptionalTest                  test(report); }
    { SmallVectorTest               test(report); }
    { StringConverterTest           test(report); }
    { StringFormatTest              test(report); }
    { StringTest                    test(report); }
    { StringViewTest                test(report); }
    { TaggedUnionTest               test(report); }
    { VectorTest                    test(report); }
    { VectorMapTest                 test(report); }
    { VectorSetTest                 test(report); }

    // File tests
    { FileDescriptorTest            test(report); }

    // FileSystem tests
    { FileSystemTest                test(report); }
    { FileSystemWalkerTest          test(report); }
    { FileSystemWatcherTest         test(report); }
    { PathTest                      test(report); }

    // Hashing tests
    { HashingTest                   test(report); }

    // Http tests
    { HttpParserTest                test(report); }
    { HttpClientTest                test(report); }
    { HttpServerTest                test(report); }
    { HttpURLParserTest             test(report); }

    // JSON tests
    { JsonFormatterTest             test(report); }
    { JsonTokenizerTest             test(report); }

    // Plugin tests
    { PluginTest                    test(report); }

    // Process tests
    { ProcessTest                   test(report); }

    // Reflection tests
    { ReflectionTest                test(report); }

    // Serialization tests
    { SerializationBinaryTemplateTest   test(report); }
    { SerializationBinaryTypeErasedTest test(report); }
    { SerializationStructuredJsonTest   test(report); }

    // Socket tests
    { SocketDescriptorTest          test(report); }

    // System tests
    { ConsoleTest                   test(report); }
    { SystemTest                    test(report); }
    { TimeTest                      test(report); }

    // Threading tests
    { AtomicTest                    test(report); }
    { ThreadingTest                 test(report); }

    // Async tests
    { EventLoopTest                 test(report); }

    // DebugVisualizers tests
    { DebugVisualizersTest          test(report); }

    // Build tests
    { BuildTest                     test(report); }

    // clang-format on
#endif

    return report.getTestReturnCode();
}
