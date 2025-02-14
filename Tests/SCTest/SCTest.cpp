// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

namespace SC
{
struct TestReport;
// Build
void runBuildTest(TestReport& report);

// Foundation
void runBaseTest(TestReport& report);
void runBufferTest(TestReport& report);
void runArenaMapTest(TestReport& report);
void runFunctionTest(TestReport& report);
void runUniqueHandleTest(TestReport& report);

// Foundation Extra
void runTaggedUnionTest(TestReport& report);
void runTaggedMapTest(TestReport& report);

// Containers
void runArrayTest(TestReport& report);
void runIntrusiveDoubleLinkedListTest(TestReport& report);
void runSmallVectorTest(TestReport& report);
void runVectorMapTest(TestReport& report);
void runVectorSetTest(TestReport& report);
void runVectorTest(TestReport& report);

// File
void runFileTest(TestReport& report);

// FileSystem
void runFileSystemDirectoriesTest(TestReport& report);
void runFileSystemTest(TestReport& report);
void runPathTest(TestReport& report);

// FileSystemIterator
void runFileSystemIteratorTest(TestReport& report);

// FileSystemWatcher
void runFileSystemWatcherTest(TestReport& report);

// Hashing
void runHashingTest(TestReport& report);

// Http
void runHttpClientTest(TestReport& report);
void runHttpParserTest(TestReport& report);
void runHttpServerTest(TestReport& report);
void runHttpWebServerTest(TestReport& report);
void runHttpURLParserTest(TestReport& report);

// Plugin
void runPluginTest(TestReport& report);

// Process
void runProcessTest(TestReport& report);

// Reflection
void runReflectionTest(TestReport& report);

// Serialization
void runSerializationBinaryTest(TestReport& report);
void runSerializationBinaryTypeErasedTest(TestReport& report);
void runSerializationJsonTest(TestReport& report);
void runSerializationJsonTokenizerTest(TestReport& report);

// Socket
void runSocketTest(TestReport& report);

// Strings
void runConsoleTest(TestReport& report);
void runStringTest(TestReport& report);
void runStringConverterTest(TestReport& report);
void runStringBuilderTest(TestReport& report);
void runStringFormatTest(TestReport& report);
void runStringViewTest(TestReport& report);

// Time
void runTimeTest(TestReport& report);

// Threading
void runAtomicTest(TestReport& report);
void runThreadingTest(TestReport& report);
void runThreadPoolTest(TestReport& report);
void runOptionalTest(TestReport& report);

// Async
void runAsyncTest(SC::TestReport& report);
void runAsyncStreamTest(SC::TestReport& report);
void runAsyncRequestStreamTest(SC::TestReport& report);
void runZLibStreamTest(TestReport& report);

// Support
void runDebugVisualizersTest(TestReport& report);
void runSupportToolsTest(TestReport& report);

// Bindings
void runCBindingsTest(TestReport& report);
} // namespace SC

#include "../../Libraries/Containers/SmallVector.h"
#include "../../Libraries/FileSystem/FileSystemDirectories.h"
#include "../../Libraries/FileSystem/Path.h"
#include "../../Libraries/Socket/Socket.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Testing/Testing.h"

SC::Console* globalConsole;

int main(int argc, const char* argv[])
{
    using namespace SC;
    Memory::registerGlobals();
    SmallBuffer<1024 * sizeof(SC::native_char_t)> globalConsoleConversionBuffer;
    Console::tryAttachingToParentConsole();
    FileSystemDirectories directories;
    if (not directories.init())
        return -2;
    if (not SocketNetworking::initNetworking())
        return -3;
    Console    console(globalConsoleConversionBuffer);
    TestReport report(console, argc, argv);
    report.applicationRootDirectory = directories.getApplicationPath();
    report.executableFile           = directories.getExecutablePath();

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
    runBaseTest(report);
    runBufferTest(report);
    runFunctionTest(report);
    runUniqueHandleTest(report);

    // Containers tests
    runArenaMapTest(report);
    runArrayTest(report);
    runIntrusiveDoubleLinkedListTest(report);
    runSmallVectorTest(report);
    runVectorTest(report);
    runVectorMapTest(report);
    runVectorSetTest(report);

    // Foundation extra tests
    runTaggedUnionTest(report);
    runTaggedMapTest(report);

    // File tests
    runFileTest(report);

    // FileSystem tests
    runFileSystemTest(report);
    runFileSystemDirectoriesTest(report);
    runPathTest(report);

    // FileSystemIterator tests
    runFileSystemIteratorTest(report);

    // FileSystemWatcher tests
    runFileSystemWatcherTest(report);

    // Hashing tests
    runHashingTest(report);

    // Http tests
    runHttpParserTest(report);
    runHttpClientTest(report);
    runHttpServerTest(report);
    runHttpWebServerTest(report);
    runHttpURLParserTest(report);

    // Plugin tests
    runPluginTest(report);

    // Process tests
    runProcessTest(report);

    // Reflection tests
    runReflectionTest(report);

    // Serialization tests
    runSerializationBinaryTest(report);
    runSerializationBinaryTypeErasedTest(report);
    runSerializationJsonTokenizerTest(report);
    runSerializationJsonTest(report);

    // Socket tests
    runSocketTest(report);

    // Strings tests
    runConsoleTest(report);
    runStringConverterTest(report);
    runStringBuilderTest(report);
    runStringFormatTest(report);
    runStringTest(report);
    runStringViewTest(report);

    // Time tests
    runTimeTest(report);

    // Threading tests
    runAtomicTest(report);
    runThreadingTest(report);
    runThreadPoolTest(report);
    runOptionalTest(report);

    // Async tests
    runAsyncTest(report);
    runAsyncStreamTest(report);
    runAsyncRequestStreamTest(report);
    runZLibStreamTest(report);

    // DebugVisualizers tests
    runDebugVisualizersTest(report);
    // runSupportToolsTest(report);

    // Build tests
    runBuildTest(report);

    // C bindings tests
    runCBindingsTest(report);

    return report.getTestReturnCode();
}
