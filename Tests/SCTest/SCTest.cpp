// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.

namespace SC
{
struct TestReport;
// Build
void runBuildTest(TestReport& report);

// Foundation
void runBaseTest(TestReport& report);
void runArenaMapTest(TestReport& report);
void runArrayTest(TestReport& report);
void runIntrusiveDoubleLinkedListTest(TestReport& report);
void runSmallVectorTest(TestReport& report);
void runVectorMapTest(TestReport& report);
void runVectorSetTest(TestReport& report);
void runVectorTest(TestReport& report);
void runFunctionTest(TestReport& report);
void runOpaqueTest(TestReport& report);
void runOptionalTest(TestReport& report);
void runTaggedUnionTest(TestReport& report);
void runSmallStringTest(TestReport& report);
void runStringConverterTest(TestReport& report);
void runStringFormatTest(TestReport& report);
void runStringViewTest(TestReport& report);

// File
void runFileDescriptorTest(TestReport& report);

// FileSystem
void runFileSystemTest(TestReport& report);
void runFileSystemWalkerTest(TestReport& report);
void runFileSystemWatcherTest(TestReport& report);
void runPathTest(TestReport& report);

// Hashing
void runHashingTest(TestReport& report);

// Http
void runHttpClientTest(TestReport& report);
void runHttpParserTest(TestReport& report);
void runHttpServerTest(TestReport& report);
void runHttpURLParserTest(TestReport& report);

// JSON
void runJsonFormatterTest(TestReport& report);
void runJsonTokenizerTest(TestReport& report);

// Plugin
void runPluginTest(TestReport& report);

// Process
void runProcessTest(TestReport& report);

// Reflection
void runReflectionTest(TestReport& report);

// Serialization
void runSerializationBinaryTemplateTest(TestReport& report);
void runSerializationBinaryTypeErasedTest(TestReport& report);
void runSerializationStructuredJsonTest(TestReport& report);

// Socket
void runSocketDescriptorTest(TestReport& report);

// System
void runConsoleTest(TestReport& report);
void runSystemTest(TestReport& report);
void runTimeTest(TestReport& report);

// Threading
void runAtomicTest(TestReport& report);
void runThreadingTest(TestReport& report);

// Async
void runEventLoopTest(SC::TestReport& report);

// Support
void runDebugVisualizersTest(TestReport& report);

} // namespace SC

#include "../../Libraries/FileSystem/Path.h"
#include "../../Libraries/Foundation/Containers/SmallVector.h"
#include "../../Libraries/System/Console.h"
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
    Console    console(globalConsoleConversionBuffer);
    TestReport report(console, argc, argv);
    report.applicationRootDirectory = directories.applicationRootDirectory.view();
    report.executableFile           = directories.executableFile.view();
    SC::SmallString<255> correctedPath;
    {
        SmallVector<StringView, 50> components;
        (void)Path::normalizeUNCAndTrimQuotes(SC_COMPILER_LIBRARY_PATH, components, correctedPath, Path::AsNative);
        // If you hit this assertion you must figure out a way to derive location of Libraries
        SC_ASSERT_RELEASE(Path::isAbsolute(correctedPath.view(), SC::Path::AsNative));
    }
    report.libraryRootDirectory   = correctedPath.view();
    report.debugBreakOnFailedTest = true;

    globalConsole = &console;

    // Foundation tests
    runArenaMapTest(report);
    runArrayTest(report);
    runBaseTest(report);
    runFunctionTest(report);
    runIntrusiveDoubleLinkedListTest(report);
    runOpaqueTest(report);
    runOptionalTest(report);
    runSmallVectorTest(report);
    runStringConverterTest(report);
    runStringFormatTest(report);
    runSmallStringTest(report);
    runStringViewTest(report);
    runTaggedUnionTest(report);
    runVectorTest(report);
    runVectorMapTest(report);
    runVectorSetTest(report);

    // File tests
    runFileDescriptorTest(report);

    // FileSystem tests
    runFileSystemTest(report);
    runFileSystemWalkerTest(report);
    runFileSystemWatcherTest(report);
    runPathTest(report);

    // Hashing tests
    runHashingTest(report);

    // Http tests
    runHttpParserTest(report);
    runHttpClientTest(report);
    runHttpServerTest(report);
    runHttpURLParserTest(report);

    // JSON tests
    runJsonFormatterTest(report);
    runJsonTokenizerTest(report);

    // Plugin tests
    runPluginTest(report);

    // Process tests
    runProcessTest(report);

    // Reflection tests
    runReflectionTest(report);

    // Serialization tests
    runSerializationBinaryTemplateTest(report);
    runSerializationBinaryTypeErasedTest(report);
    runSerializationStructuredJsonTest(report);

    // Socket tests
    runSocketDescriptorTest(report);

    // System tests
    runConsoleTest(report);
    runSystemTest(report);
    runTimeTest(report);

    // Threading tests
    runAtomicTest(report);
    runThreadingTest(report);

    // Async tests
    runEventLoopTest(report);

    // DebugVisualizers tests
    runDebugVisualizersTest(report);

    // Build tests
    runBuildTest(report);

    return report.getTestReturnCode();
}
