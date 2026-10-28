// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

namespace SC
{
struct TestReport;

// Foundation
void runBaseTest(TestReport& report);
void runTestingTest(TestReport& report);
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
void runSerialPortTest(TestReport& report);

// Fibers
void runAsyncFibersTest(TestReport& report);
void runFibersTest(TestReport& report);

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

// Cryptography
void runCryptographyTest(TestReport& report);
void runCryptographyAsyncStreamsTest(TestReport& report);
void runCryptographyTransformStreamsTest(TestReport& report);

// Http
void runHttpParserTest(TestReport& report);
void runHttpAsyncClientTest(TestReport& report);
void runHttpAsyncServerTest(TestReport& report);
void runHttpAsyncFileServerTest(TestReport& report);
void runHttpURLParserTest(TestReport& report);
void runHttpHeadersTest(TestReport& report);
void runHttpRouterTest(TestReport& report);
void runHttpKeepAliveTest(TestReport& report);
void runHttpMultipartParserTest(TestReport& report);
void runHttpWebSocketFrameTest(TestReport& report);
void runHttpWebSocketHandshakeTest(TestReport& report);
void runHttpWebSocketHubTest(TestReport& report);
void runHttpWebSocketLifecycleTest(TestReport& report);
void runHttpStressTest(TestReport& report);

// HttpClient
void runHttpClientTest(TestReport& report);

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
void runCommandLineTest(TestReport& report);
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
void runAsyncContractTest(SC::TestReport& report);
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
#include "Libraries/Process/Process.h"
#include "Libraries/Socket/Socket.h"
#include "Libraries/Strings/Console.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"

#if SC_PLATFORM_WINDOWS
#include <direct.h>
#else
#include <stdlib.h>
#include <unistd.h>
#endif

SC::Console* globalConsole;

#if SC_COMPILER_MSVC
#define SC_TEST_RUN_NO_INLINE __declspec(noinline)
#else
#define SC_TEST_RUN_NO_INLINE __attribute__((noinline))
#endif

namespace
{
// Routing happens before a test constructor can touch shared resources. Keep
// TestReport's public selection semantics and child-process arguments unchanged.
struct TestExecution
{
    SC::TestReport& report;
    SC::uint32_t    shardIndex = 0;
    SC::uint32_t    shardCount = 1;
    bool            listOnly   = false;

    SC::StringView childRoots[2] = {};
    SC::StringView childLogs     = {};

    bool configure(int argc, const char* argv[])
    {
        bool           haveIndex = false;
        bool           haveCount = false;
        bool           haveRoot  = false;
        SC::StringView scratchRoot;
        for (int i = 1; i < argc; ++i)
        {
            const SC::StringView arg = SC::StringView::fromNullTerminated(argv[i], SC::StringEncoding::Ascii);
            if (arg == "--list-test-functions")
                listOnly = true;
            else if (arg == "--shard-index" or arg == "--shard-count")
            {
                const bool isIndex = arg == "--shard-index";
                if (++i == argc or (isIndex ? haveIndex : haveCount))
                    return false;
                const SC::StringView number = SC::StringView::fromNullTerminated(argv[i], SC::StringEncoding::Ascii);
                if (number.isEmpty())
                    return false;
                for (SC::size_t digit = 0; digit < number.sizeInBytes(); ++digit)
                {
                    if (number.bytesWithoutTerminator()[digit] < '0' or number.bytesWithoutTerminator()[digit] > '9')
                        return false;
                }
                SC::int32_t value = 0;
                if (not number.parseInt32(value) or value < 0 or (not isIndex and value == 0))
                    return false;
                if (isIndex)
                {
                    shardIndex = static_cast<SC::uint32_t>(value);
                    haveIndex  = true;
                }
                else
                {
                    shardCount = static_cast<SC::uint32_t>(value);
                    haveCount  = true;
                }
            }
            else if (arg == "--run-shards")
            {
                if (i + 3 >= argc or not childLogs.isEmpty())
                    return false;
                childRoots[0] = SC::StringView::fromNullTerminated(argv[++i], SC::StringEncoding::Utf8);
                childRoots[1] = SC::StringView::fromNullTerminated(argv[++i], SC::StringEncoding::Utf8);
                childLogs     = SC::StringView::fromNullTerminated(argv[++i], SC::StringEncoding::Utf8);
                SC::FileSystem fs;
                if (not fs.init(".") or not SC::Path::isAbsolute(childLogs, SC::Path::AsNative) or
                    not fs.existsAndIsDirectory(childLogs))
                    return false;
                for (const auto& root : childRoots)
                {
                    if (not SC::Path::isAbsolute(root, SC::Path::AsNative) or not fs.existsAndIsDirectory(root))
                        return false;
                }
            }
            else if (arg == "--test-root")
            {
                if (++i == argc or haveRoot)
                    return false;
                scratchRoot = SC::StringView::fromNullTerminated(argv[i], SC::StringEncoding::Utf8);
                haveRoot    = true;
                if (scratchRoot.isEmpty())
                    return false;
            }
        }
        if (haveIndex != haveCount or shardIndex >= shardCount)
            return false;
        if (not childLogs.isEmpty() and
            (haveIndex or haveRoot or listOnly or report.mapPort(0) > 2000 or childRoots[0] == childRoots[1]))
            return false;
        if (listOnly)
            return true;
        if (shardCount > 1 and scratchRoot.isEmpty())
            return false;
        if (not scratchRoot.isEmpty())
        {
            SC::FileSystem fs;
            if (not SC::Path::isAbsolute(scratchRoot, SC::Path::AsNative) or not fs.init(scratchRoot) or
                not fs.existsAndIsDirectory(scratchRoot))
                return false;
#if SC_PLATFORM_WINDOWS
            if (not report.applicationRootDirectory.assign(scratchRoot) or
                ::_wchdir(report.applicationRootDirectory.writableSpan().data()) != 0)
                return false;
#else
            // FileSystem scopes relative operations per instance; it does not
            // change the process cwd. Canonicalize aliases for FSEvents too.
            char physicalRoot[SC::StringPath::MaxPath + 1];
            if (::realpath(scratchRoot.bytesWithoutTerminator(), physicalRoot) == nullptr or
                not report.applicationRootDirectory.assign(
                    SC::StringView::fromNullTerminated(physicalRoot, SC::StringEncoding::Utf8)) or
                ::chdir(physicalRoot) != 0)
                return false;
#endif
        }
        return true;
    }

    int runChildren(int argc, const char* argv[])
    {
        using namespace SC;
        if (argc > 80)
            return 1;
        Process          processes[2];
        SmallString<16>  offsets[2];
        StringSpan       arguments[2][96];
        const StringView indices[] = {"0", "1"};
        size_t           launched  = 0;
        bool             succeeded = true;
        for (size_t index = 0; index < 2; ++index)
        {
            StringPath outputPath;
            StringPath errorPath;
            if (not StringBuilder::format(offsets[index], "{}",
                                          static_cast<size_t>(report.mapPort(0)) + 4000 * index) or
                not StringBuilder::format(outputPath, "{}/shard-{}.log", childLogs, index) or
                not StringBuilder::format(errorPath, "{}/shard-{}.err", childLogs, index))
            {
                succeeded = false;
                break;
            }
            FileOpen write(FileOpen::Write);
            write.inheritable = true;
            FileDescriptor output;
            FileDescriptor errors;
            if (not output.open(outputPath.view(), write) or not errors.open(errorPath.view(), write))
            {
                succeeded = false;
                break;
            }
            size_t count              = 0;
            arguments[index][count++] = report.executableFile.view();
            for (int i = 1; i < argc; ++i)
            {
                const auto arg = StringView::fromNullTerminated(argv[i], StringEncoding::Utf8);
                if (arg == "--run-shards")
                {
                    i += 3;
                    continue;
                }
                arguments[index][count++] = arg;
            }
            arguments[index][count++] = "--shard-index";
            arguments[index][count++] = indices[index];
            arguments[index][count++] = "--shard-count";
            arguments[index][count++] = "2";
            arguments[index][count++] = "--test-root";
            arguments[index][count++] = childRoots[index];
            arguments[index][count++] = "--port-offset";
            arguments[index][count++] = offsets[index].view();
            if (not processes[index].launch({arguments[index], count}, output, Process::StdIn(), errors))
            {
                succeeded = false;
                break;
            }
            launched++;
        }
        // Drain every launched process even when a sibling fails to start or fails a test.
        for (size_t index = 0; index < launched; ++index)
        {
            const bool waited = static_cast<bool>(processes[index].waitForExitSync());
            const int  code   = waited ? processes[index].getExitStatus() : 1;
            report.console.print("SCTest::Shard {} exit {}\n", index, static_cast<size_t>(code));
            succeeded = succeeded and waited and code == 0;
        }
        report.console.flush();
        return succeeded and launched == 2 ? 0 : 1;
    }

    SC_TEST_RUN_NO_INLINE void run(const char* name, SC::size_t length, void (*function)(SC::TestReport&))
    {
        // Keep label construction and routing in one compiled boundary. Repeated
        // inlining makes Fil-C instrument the same temporary/loop at every call.
        const SC::StringView registration({name, length}, true, SC::StringEncoding::Ascii);
        SC::uint32_t         hash = 2166136261u;
        for (SC::size_t i = 0; i < registration.sizeInBytes(); ++i)
        {
            hash ^= static_cast<SC::uint8_t>(registration.bytesWithoutTerminator()[i]);
            hash *= 16777619u;
        }
        if (hash % shardCount != shardIndex)
            return;
        if (listOnly or shardCount > 1)
        {
            report.console.print("SCTest::Registration {}\n", registration);
            report.console.flush();
        }
        if (not listOnly)
            function(report);
    }
};
} // namespace

#undef SC_TEST_RUN_NO_INLINE

#define SC_RUN_TEST(function) execution.run(#function, sizeof(#function) - 1, function)

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
    if (report.hasStartupFailure())
        return report.getTestReturnCode();
    report.debugBreakOnFailedTest = true;

    TestExecution execution{report};
    if (not execution.configure(argc, argv))
    {
        console.printLine("Invalid test execution options: use --shard-index N --shard-count M with 0 <= N < M "
                          "and an existing absolute --test-root directory");
        return 1;
    }
    if (not execution.childLogs.isEmpty())
    {
        execution.listOnly = true;
        report.quietMode   = true; // The children own test summaries and memory checks.
    }

#if !defined(SC_TEST_TOOLS_ONLY)
    // Foundation tests
    SC_RUN_TEST(runBaseTest);
    SC_RUN_TEST(runTestingTest);
    SC_RUN_TEST(runStringSpanTest);
    SC_RUN_TEST(runUniqueHandleTest);
    SC_RUN_TEST(runFunctionTest);

    // Memory tests
    SC_RUN_TEST(runGlobalsTest);
    SC_RUN_TEST(runBufferTest);
    SC_RUN_TEST(runVirtualMemoryTest);
    SC_RUN_TEST(runStringTest);

    // Containers tests
    SC_RUN_TEST(runArenaMapTest);
    SC_RUN_TEST(runArrayTest);
    SC_RUN_TEST(runSmallVectorTest);
    SC_RUN_TEST(runVectorTest);
    SC_RUN_TEST(runVectorMapTest);
    SC_RUN_TEST(runVectorSetTest);
    SC_RUN_TEST(runGlobalsContainerTest);

    // Foundation extra tests
    SC_RUN_TEST(runTaggedUnionTest);
    SC_RUN_TEST(runTaggedMapTest);

    // File tests
    SC_RUN_TEST(runFileTest);
    SC_RUN_TEST(runSerialPortTest);

    // Fibers tests
    SC_RUN_TEST(runFibersTest);
    SC_RUN_TEST(runAsyncFibersTest);

    // FileSystem tests
    SC_RUN_TEST(runFileSystemTest);
    SC_RUN_TEST(runPathTest);

    // FileSystemIterator tests
    SC_RUN_TEST(runFileSystemIteratorTest);

    // FileSystemWatcher tests
    SC_RUN_TEST(runFileSystemWatcherTest);
    SC_RUN_TEST(runFileSystemWatcherAsyncTest);

    // Hashing tests
    SC_RUN_TEST(runHashingTest);

    // Cryptography tests
    SC_RUN_TEST(runCryptographyTest);
    SC_RUN_TEST(runCryptographyAsyncStreamsTest);
    SC_RUN_TEST(runCryptographyTransformStreamsTest);

    // Http tests
    SC_RUN_TEST(runHttpParserTest);
    SC_RUN_TEST(runHttpAsyncClientTest);
    SC_RUN_TEST(runHttpAsyncServerTest);
    SC_RUN_TEST(runHttpAsyncFileServerTest);
    SC_RUN_TEST(runHttpURLParserTest);
    SC_RUN_TEST(runHttpHeadersTest);
    SC_RUN_TEST(runHttpRouterTest);
    SC_RUN_TEST(runHttpKeepAliveTest);
    SC_RUN_TEST(runHttpMultipartParserTest);
    SC_RUN_TEST(runHttpWebSocketFrameTest);
    SC_RUN_TEST(runHttpWebSocketHandshakeTest);
    SC_RUN_TEST(runHttpWebSocketHubTest);
    SC_RUN_TEST(runHttpWebSocketLifecycleTest);
    SC_RUN_TEST(runHttpStressTest);

    // HttpClient tests
    SC_RUN_TEST(runHttpClientTest);

    // Plugin tests
#if SC_XCTEST
#else
    SC_RUN_TEST(runPluginTest);
#endif

    // Process tests
    SC_RUN_TEST(runProcessTest);

    // Reflection tests
    SC_RUN_TEST(runReflectionTest);

    // Serialization tests
    SC_RUN_TEST(runSerializationBinaryTest);
    SC_RUN_TEST(runSerializationBinaryTypeErasedTest);
    SC_RUN_TEST(runSerializationJsonTokenizerTest);
    SC_RUN_TEST(runSerializationJsonTest);

    // Socket tests
    SC_RUN_TEST(runSocketTest);

    // Strings tests
    SC_RUN_TEST(runConsoleTest);
    SC_RUN_TEST(runStringConverterTest);
    SC_RUN_TEST(runStringBuilderTest);
    SC_RUN_TEST(runCommandLineTest);
    SC_RUN_TEST(runStringFormatTest);
    SC_RUN_TEST(runStringTest);
    SC_RUN_TEST(runStringViewTest);

    // Time tests
    SC_RUN_TEST(runTimeTest);

    // Threading tests
    SC_RUN_TEST(runAtomicTest);
    SC_RUN_TEST(runThreadingTest);
    SC_RUN_TEST(runThreadPoolTest);
    SC_RUN_TEST(runOptionalTest);

    // Async tests
    SC_RUN_TEST(runAsyncContractTest);
    SC_RUN_TEST(runAsyncTest);
    SC_RUN_TEST(runAsyncStreamTest);
    SC_RUN_TEST(runAsyncRequestStreamTest);
    SC_RUN_TEST(runZLibStreamTest);
    SC_RUN_TEST(runIntrusiveDoubleLinkedListTest);

    // DebugVisualizers tests
    SC_RUN_TEST(runDebugVisualizersTest);
#endif
#if !defined(SC_TEST_EXCLUDE_TOOLS)
    SC_RUN_TEST(runSupportToolsTest);
#endif

    if (not execution.childLogs.isEmpty())
    {
        const int code = execution.runChildren(argc, argv);
        SocketNetworking::shutdownNetworking();
        return code;
    }
    SocketNetworking::shutdownNetworking();
    report.runGlobalMemoryReport(Globals::get(Globals::Global).allocator.statistics);
    return report.getTestReturnCode();
}
