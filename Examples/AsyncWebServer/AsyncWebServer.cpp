// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A simple Async Web Server example using the Http library.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Execute `BuildAndRun.{bat|sh}` to quickly try this example.
// (Alternatively) Run `./SC.sh build configure` from repo root to generate IDE projects in _Build/_Projects.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Containers/VirtualArray.h"
#include "../../Libraries/Http/HttpAsyncFileServer.h"
#include "../../Libraries/Http/HttpAsyncServer.h"
#include "../../Libraries/Memory/String.h"
#include "../../Libraries/Strings/CommandLine.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/StringView.h"

SC::Console* globalConsole;

namespace SC
{
struct AsyncWebServerExample
{
    String  directory;
    String  interface   = "127.0.0.1";
    int32_t port        = 8090;
    int32_t maxClients  = 400; // Max number of concurrent connections
    int32_t numThreads  = 4;   // Number of threads for async file stream operations
    bool    useSendFile = true;
    bool    useEpoll    = false;

    HttpConnectionsPool::Configuration asyncConfiguration;

    AsyncEventLoop*     eventLoop = nullptr;
    HttpAsyncServer     httpServer;
    HttpAsyncFileServer fileServer;

    ThreadPool threadPool;

    static constexpr size_t MAX_CONNECTIONS  = 1000000;     // Reserve space for max 1 million connections
    static constexpr size_t MAX_READ_QUEUE   = 10;          // Max number of read queue buffers for each connection
    static constexpr size_t MAX_WRITE_QUEUE  = 10;          // Max number of write queue buffers for each connection
    static constexpr size_t MAX_BUFFERS      = 10;          // Max number of write queue buffers for each connection
    static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024; // Max number of bytes to stream data for each connection
    static constexpr size_t MAX_HEADER_SIZE  = 32 * 1024;   // Max number of bytes to hold request and response headers

    VirtualArray<HttpConnection> clients = {MAX_CONNECTIONS};

    // For simplicity just hardcode a read queue of 3 for file streams
    VirtualArray<HttpAsyncFileServer::StreamQueue<3>> fileStreams = {MAX_CONNECTIONS};

    VirtualArray<AsyncReadableStream::Request> allReadQueues  = {MAX_CONNECTIONS * MAX_READ_QUEUE};
    VirtualArray<AsyncWritableStream::Request> allWriteQueues = {MAX_CONNECTIONS * MAX_WRITE_QUEUE};
    VirtualArray<AsyncBufferView>              allBuffers     = {MAX_CONNECTIONS * MAX_BUFFERS};
    VirtualArray<char>                         allHeaders     = {MAX_CONNECTIONS * MAX_HEADER_SIZE};
    VirtualArray<char>                         allStreams     = {MAX_CONNECTIONS * MAX_REQUEST_SIZE};

    Result start()
    {
        SC_TRY(assignConnectionMemory(static_cast<size_t>(maxClients)));
        // Optimization: only create a thread pool for FS operations if needed (i.e. when async backend != io_uring)
        if (eventLoop->needsThreadPoolForFileOperations())
        {
            SC_TRY(threadPool.create(static_cast<size_t>(numThreads)));
            if (not useSendFile)
            {
                globalConsole->print("IO/Threads: {}\n", numThreads);
            }
        }
        // Initialize and start http and file servers, delegating requests to the latter in order to serve files
        SC_TRY(httpServer.init(clients.toSpan()));
        SC_TRY(httpServer.start(*eventLoop, interface.view(), static_cast<uint16_t>(port)));
        SC_TRY(fileServer.init(threadPool, *eventLoop, directory.view()));
        fileServer.setUseAsyncFileSend(useSendFile);
        globalConsole->print("Serving files from folder: {}\n", directory);
        globalConsole->print("AsyncFileSend optimization: {}\n", useSendFile);
        globalConsole->print("Max clients: {}\n", maxClients);
#if SC_PLATFORM_LINUX
        globalConsole->print("Using {}\n", useEpoll ? "epoll" : "io_uring");
#endif
        httpServer.onRequest = [&](HttpConnection& connection)
        {
            HttpAsyncFileServer::Stream& stream = fileStreams.toSpan()[connection.getConnectionID().getIndex()];
            SC_ASSERT_RELEASE(fileServer.handleRequest(stream, connection));
        };
        return Result(true);
    }

    Result assignConnectionMemory(size_t numClients)
    {
        SC_TRY(clients.resize(numClients));
        SC_TRY(fileStreams.resize(numClients));
        SC_TRY(allReadQueues.resize(numClients * asyncConfiguration.readQueueSize));
        SC_TRY(allWriteQueues.resize(numClients * asyncConfiguration.writeQueueSize));
        SC_TRY(allBuffers.resize(numClients * asyncConfiguration.buffersQueueSize));
        SC_TRY(allHeaders.resizeWithoutInitializing(numClients * asyncConfiguration.headerBytesLength));
        SC_TRY(allStreams.resizeWithoutInitializing(numClients * asyncConfiguration.streamBytesLength));
        HttpConnectionsPool::Memory memory;
        memory.allBuffers    = allBuffers;
        memory.allReadQueue  = allReadQueues;
        memory.allWriteQueue = allWriteQueues;
        memory.allHeaders    = allHeaders;
        memory.allStreams    = allStreams;
        SC_TRY(memory.assignTo(asyncConfiguration, clients.toSpan()));
        return Result(true);
    }

    Result runtimeResize()
    {
        const size_t numClients =
            max(static_cast<size_t>(maxClients), httpServer.getConnections().getHighestActiveConnection());
        SC_TRY(assignConnectionMemory(numClients));
        SC_TRY(httpServer.resize(clients.toSpan()));
        return Result(true);
    }
};

Result saneMain(Span<const StringSpan> args)
{
    AsyncWebServerExample sample;
    SocketNetworking::initNetworking();
    Console::tryAttachingToParentConsole();
    Console    console;
    StringPath currentDirPath;
    StringView directory;
    uint16_t   port = static_cast<uint16_t>(sample.port);

    globalConsole = &console;
    CommandLineOption cmdOptions[6];
    cmdOptions[0].longName  = "directory";
    cmdOptions[0].help      = "Directory to serve (defaults to current working directory)";
    cmdOptions[0].valueName = "PATH";
    cmdOptions[0].shortName = 'd';
    cmdOptions[0].value     = CommandLineValue::stringView(directory);

    cmdOptions[1].longName         = "sendfile";
    cmdOptions[1].negativeLongName = "no-sendfile";
    cmdOptions[1].help             = "Enable or disable async file send optimization";
    cmdOptions[1].value            = CommandLineValue::boolean(sample.useSendFile);

    cmdOptions[2].longName         = "epoll";
    cmdOptions[2].negativeLongName = "uring";
    cmdOptions[2].help             = "Select Linux async backend (epoll or io_uring)";
    cmdOptions[2].value            = CommandLineValue::boolean(sample.useEpoll);

    cmdOptions[3].longName  = "clients";
    cmdOptions[3].help      = "Maximum number of concurrent clients";
    cmdOptions[3].valueName = "NUM";
    cmdOptions[3].shortName = 'c';
    cmdOptions[3].value     = CommandLineValue::int32(sample.maxClients);

    cmdOptions[4].longName  = "threads";
    cmdOptions[4].help      = "Number of worker threads for file operations";
    cmdOptions[4].valueName = "NUM";
    cmdOptions[4].shortName = 't';
    cmdOptions[4].value     = CommandLineValue::int32(sample.numThreads);

    cmdOptions[5].longName  = "port";
    cmdOptions[5].help      = "Port to listen on";
    cmdOptions[5].valueName = "PORT";
    cmdOptions[5].shortName = 'p';
    cmdOptions[5].value     = CommandLineValue::uint16(port);

    CommandLineSpec spec;
    spec.programName = "AsyncWebServer";
    spec.summary     = "A simple async web server example using Sane C++ Libraries.";
    spec.options     = cmdOptions;

    const CommandLineParseResult parseResult = spec.parse(args);
    if (parseResult.status == CommandLineParseResult::Status::HelpRequested)
    {
        StringFormatOutput output(StringEncoding::Utf8, console, true);
        SC_TRY(spec.writeHelp(output));
        console.flush();
        return Result(true);
    }
    if (parseResult.status == CommandLineParseResult::Status::Error)
    {
        StringFormatOutput output(StringEncoding::Utf8, console, false);
        SC_TRY(spec.writeError(parseResult, output));
        console.flushStdErr();
        return Result(false);
    }

    if (port == 0)
    {
        console.printError("Invalid port value: 0\n");
        return Result(false);
    }
    sample.port = static_cast<int32_t>(port);
    if (sample.directory.isEmpty())
    {
        if (directory.isEmpty())
        {
            sample.directory = FileSystem::Operations::getCurrentWorkingDirectory(currentDirPath);
        }
        else
        {
            sample.directory = directory;
        }
    }

    AsyncEventLoop::Options options;
    if (sample.useEpoll)
    {
        options.apiType = AsyncEventLoop::Options::ApiType::ForceUseEpoll;
    }
    AsyncEventLoop eventLoop;
    SC_TRY(eventLoop.create(options));
    sample.eventLoop = &eventLoop;
    console.print("Address: {}:{}\nFolder : {}\n", sample.interface, sample.port, sample.directory);
    SC_TRY(sample.start());
    return eventLoop.run();
}

} // namespace SC

template <typename CharType>
static int asyncWebServerMain(int argc, CharType** argv)
{
    using namespace SC;
    static constexpr size_t MAX_COMMAND_LINE_ARGUMENTS = 10; // 4 valued options + 2 boolean flags
    StringSpan              argsStorage[MAX_COMMAND_LINE_ARGUMENTS];
    CommandLineArguments    args;
    if (not args.setFromMainArguments(argc, argv, argsStorage))
    {
        return -1;
    }
    return SC::saneMain(args.values) ? 0 : -1;
}

#if SC_PLATFORM_WINDOWS
int wmain(int argc, wchar_t** argv) { return asyncWebServerMain(argc, argv); }
#else
int main(int argc, char** argv) { return asyncWebServerMain(argc, argv); }
#endif
