// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

namespace SC
{
struct TestReport;
// Build
void runBuildTest(TestReport& report);

// Foundation
void runBaseTest(TestReport& report);
void runStringSpanTest(TestReport& report);
void runFunctionTest(TestReport& report);
void runUniqueHandleTest(TestReport& report);

// Memory
void runBufferTest(TestReport& report);
void runVirtualMemoryTest(TestReport& report);
void runGlobalsTest(TestReport& report);
void runStringTest(TestReport& report);

// Foundation Extra
void runTaggedUnionTest(TestReport& report);
void runTaggedMapTest(TestReport& report);

// Containers
void runArenaMapTest(TestReport& report);
void runArrayTest(TestReport& report);
void runSmallVectorTest(TestReport& report);
void runVectorMapTest(TestReport& report);
void runVectorSetTest(TestReport& report);
void runVectorTest(TestReport& report);
void runGlobalsContainerTest(TestReport& report);

// File
void runFileTest(TestReport& report);

// FileSystem
void runFileSystemTest(TestReport& report);
void runPathTest(TestReport& report);

// FileSystemIterator
void runFileSystemIteratorTest(TestReport& report);

// FileSystemWatcher
void runFileSystemWatcherTest(TestReport& report);

// FileSystemWatcherAsync
void runFileSystemWatcherAsyncTest(TestReport& report);

// Hashing
void runHashingTest(TestReport& report);

// Http
void runHttpClientTest(TestReport& report);
void runHttpParserTest(TestReport& report);
void runHttpAsyncServerTest(TestReport& report);
void runHttpAsyncFileServerTest(TestReport& report);
void runHttpURLParserTest(TestReport& report);
void runHttpKeepAliveTest(TestReport& report);
void runHttpMultipartParserTest(TestReport& report);

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
void runIntrusiveDoubleLinkedListTest(TestReport& report);

// Support
void runDebugVisualizersTest(TestReport& report);
void runSupportToolsTest(TestReport& report);

} // namespace SC

#ifdef SC_SPACES_SPECIFIC_DEFINE
// This tests that a file-specific define for "SC Spaces.cpp" is not defined globally by mistake
#error "SC_SPACES_SPECIFIC_DEFINE should NOT be defined on this file"
#endif

#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/Globals.h"
#include "Libraries/Memory/Memory.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Socket/Socket.h"
#include "Libraries/Strings/Console.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Testing/Testing.h"

SC::Console* globalConsole;

int main(int argc, const char* argv[])
{
    using namespace SC;
    Globals::init(Globals::Global, {1024 * 1024}); // 1MB for ownership tracking
    Console::tryAttachingToParentConsole();
    SocketNetworking::initNetworking();
    Console console;
    globalConsole = &console;

    TestReport::Output<Console> trConsole = {console};

    TestReport report(trConsole, argc, argv);
    FileSystem::Operations::getExecutablePath(report.executableFile);
    FileSystem::Operations::getApplicationRootDirectory(report.applicationRootDirectory);

    SC::SmallString<255> correctedPath;
    {
        StringView components[64];
        (void)Path::normalizeUNCAndTrimQuotes(correctedPath, SC_COMPILER_LIBRARY_PATH, Path::AsNative, components);
        // If you hit this assertion you must figure out a way to derive location of Libraries
        SC_ASSERT_RELEASE(Path::isAbsolute(correctedPath.view(), SC::Path::AsNative));
    }
    SC_ASSERT_RELEASE(report.libraryRootDirectory.assign(correctedPath.view()));
    report.debugBreakOnFailedTest = true;

    // Foundation tests
    runBaseTest(report);
    runStringSpanTest(report);
    runUniqueHandleTest(report);
    runFunctionTest(report);

    // Memory tests
    runGlobalsTest(report);
    runBufferTest(report);
    runVirtualMemoryTest(report);
    runStringTest(report);

    // Containers tests
    runArenaMapTest(report);
    runArrayTest(report);
    runSmallVectorTest(report);
    runVectorTest(report);
    runVectorMapTest(report);
    runVectorSetTest(report);
    runGlobalsContainerTest(report);

    // Foundation extra tests
    runTaggedUnionTest(report);
    runTaggedMapTest(report);

    // File tests
    runFileTest(report);

    // FileSystem tests
    runFileSystemTest(report);
    runPathTest(report);

    // FileSystemIterator tests
    runFileSystemIteratorTest(report);

    // FileSystemWatcher tests
    runFileSystemWatcherTest(report);
    runFileSystemWatcherAsyncTest(report);

    // Hashing tests
    runHashingTest(report);

    // Http tests
    runHttpParserTest(report);
    runHttpClientTest(report);
    runHttpAsyncServerTest(report);
    runHttpAsyncFileServerTest(report);
    runHttpURLParserTest(report);
    runHttpKeepAliveTest(report);
    runHttpMultipartParserTest(report);

    // Plugin tests
#if SC_XCTEST
#else
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
    runIntrusiveDoubleLinkedListTest(report);

    // DebugVisualizers tests
    runDebugVisualizersTest(report);
    // runSupportToolsTest(report);

    // Build tests (opt-in locally with --all-tests, still runnable with --test BuildTest)
    if (report.runAllTests or
        report.isTestExplicitlySelected(StringSpan::fromNullTerminated("BuildTest", StringEncoding::Ascii)))
    {
        runBuildTest(report);
    }
    SocketNetworking::shutdownNetworking();
    report.runGlobalMemoryReport(Globals::get(Globals::Global).allocator.statistics);
    return report.getTestReturnCode();
}
