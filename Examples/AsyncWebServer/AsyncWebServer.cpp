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
#include "../../Libraries/Memory/String.h"
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

    HttpAsyncConnectionBase::Configuration asyncConfiguration;

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

    VirtualArray<HttpAsyncConnectionBase> clients = {MAX_CONNECTIONS};

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
            SC_TRY(threadPool.create(numThreads));
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
        HttpAsyncConnectionBase::Memory memory;
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

Result saneMain(Span<StringSpan> args)
{
    AsyncWebServerExample sample;
    SocketNetworking::initNetworking();
    Console::tryAttachingToParentConsole();
    Console    console;
    StringPath currentDirPath;

    globalConsole = &console;
    // Parse command line arguments
    for (size_t i = 0; i < args.sizeInElements(); ++i)
    {
        if (args[i] == "--directory" and i + 1 < args.sizeInElements())
        {
            sample.directory = args[i + 1];
            ++i;
        }
        else if (args[i] == "--sendfile")
        {
            sample.useSendFile = true;
        }
        else if (args[i] == "--no-sendfile")
        {
            sample.useSendFile = false;
        }
        else if (args[i] == "--epoll")
        {
            sample.useEpoll = true;
        }
        else if (args[i] == "--uring")
        {
            sample.useEpoll = false;
        }
        else if (args[i] == "--clients" and i + 1 < args.sizeInElements())
        {
            if (not StringView(args[i + 1]).parseInt32(sample.maxClients))
            {
                globalConsole->print("Invalid max clients value: {}", args[i + 1]);
            }
            ++i;
        }
        else if (args[i] == "--threads" and i + 1 < args.sizeInElements())
        {
            if (not StringView(args[i + 1]).parseInt32(sample.numThreads))
            {
                globalConsole->print("Invalid number of threads value: {}", args[i + 1]);
            }
            ++i;
        }
    }
    if (sample.directory.isEmpty())
    {
        sample.directory = FileSystem::Operations::getCurrentWorkingDirectory(currentDirPath);
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

int main(int argc, char** argv)
{
    using namespace SC;
    constexpr auto NUM_ARGS_MAX = 10;
    StringSpan     args[NUM_ARGS_MAX];
    for (int idx = 1; idx < min(argc, NUM_ARGS_MAX); ++idx)
        args[idx - 1] = StringSpan::fromNullTerminated(argv[idx], StringEncoding::Utf8);
    return SC::saneMain(args) ? 0 : -1;
}
