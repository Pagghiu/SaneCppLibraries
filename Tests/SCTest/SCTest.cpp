// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

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
void runUniqueHandleTest(TestReport& report);
void runOptionalTest(TestReport& report);
void runTaggedUnionTest(TestReport& report);

// File
void runFileDescriptorTest(TestReport& report);

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
void runSocketDescriptorTest(TestReport& report);

// Strings
void runConsoleTest(TestReport& report);
void runStringTest(TestReport& report);
void runSmallStringTest(TestReport& report);
void runStringConverterTest(TestReport& report);
void runStringBuilderTest(TestReport& report);
void runStringFormatTest(TestReport& report);
void runStringViewTest(TestReport& report);

// Time
void runTimeTest(TestReport& report);

// Threading
void runAtomicTest(TestReport& report);
void runThreadingTest(TestReport& report);

// Async
void runAsyncTest(SC::TestReport& report);

// Support
void runDebugVisualizersTest(TestReport& report);

} // namespace SC

#include "../../Libraries/Containers/SmallVector.h"
#include "../../Libraries/FileSystem/FileSystemDirectories.h"
#include "../../Libraries/FileSystem/Path.h"
#include "../../Libraries/Socket/SocketDescriptor.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Testing/Testing.h"

SC::Console* globalConsole;

int main(int argc, const char* argv[])
{
    SC::SmallVector<char, 1024 * sizeof(SC::native_char_t)> globalConsoleConversionBuffer;
    using namespace SC;
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
    runArenaMapTest(report);
    runArrayTest(report);
    runBaseTest(report);
    runFunctionTest(report);
    runIntrusiveDoubleLinkedListTest(report);
    runUniqueHandleTest(report);
    runOptionalTest(report);
    runSmallVectorTest(report);
    runTaggedUnionTest(report);
    runVectorTest(report);
    runVectorMapTest(report);
    runVectorSetTest(report);

    // File tests
    runFileDescriptorTest(report);

    // FileSystem tests
    runFileSystemTest(report);
    runFileSystemDirectoriesTest(report);
    runPathTest(report);

    // FileSystemIterator tests
    runFileSystemIteratorTest(report);

    // FileSystemWatcher tests
#if !SC_PLATFORM_LINUX
    runFileSystemWatcherTest(report);
#endif

    // Hashing tests
    runHashingTest(report);

    // Http tests
    runHttpParserTest(report);
#if !SC_PLATFORM_LINUX
    runHttpClientTest(report);
    runHttpServerTest(report);
#endif
    runHttpURLParserTest(report);

    // Plugin tests
#if !SC_PLATFORM_LINUX
    runPluginTest(report);
#endif

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
    runSocketDescriptorTest(report);

    // Strings tests
    runConsoleTest(report);
    runStringConverterTest(report);
    runStringBuilderTest(report);
    runStringFormatTest(report);
    runStringTest(report);
    runSmallStringTest(report);
    runStringViewTest(report);

    // Time tests
    runTimeTest(report);

    // Threading tests
    runAtomicTest(report);
    runThreadingTest(report);

    // Async tests
#if !SC_PLATFORM_LINUX
    runAsyncTest(report);
#endif

    // DebugVisualizers tests
    runDebugVisualizersTest(report);

    // Build tests
    runBuildTest(report);

    return report.getTestReturnCode();
}
