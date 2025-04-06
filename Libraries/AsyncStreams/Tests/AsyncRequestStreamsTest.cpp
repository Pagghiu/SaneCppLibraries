
// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../AsyncStreams/AsyncRequestStreams.h"
#include "../../Async/Async.h"
#include "../../AsyncStreams/AsyncStreams.h"
#include "../../AsyncStreams/ZLibTransformStreams.h"
#include "../../Containers/Vector.h"
#include "../../File/File.h"
#include "../../FileSystem/FileSystem.h"
#include "../../FileSystem/Path.h"
#include "../../Foundation/Buffer.h"
#include "../../Socket/Socket.h"
#include "../../Testing/Testing.h"

namespace SC
{
struct AsyncRequestStreamsTest;
} // namespace SC

struct SC::AsyncRequestStreamsTest : public SC::TestCase
{
    AsyncEventLoop::Options options;
    AsyncRequestStreamsTest(SC::TestReport& report) : TestCase(report, "AsyncRequestStreamsTest")
    {
        if (test_section("file to file"))
        {
            fileToFile();
        }

        int numTestsToRun = 1;
        if (AsyncEventLoop::tryLoadingLiburing())
        {
            // Run all tests on epoll backend first, and then re-run them on io_uring
            options.apiType = AsyncEventLoop::Options::ApiType::ForceUseEpoll;
            numTestsToRun   = 2;
        }

        for (int i = 0; i < numTestsToRun; ++i)
        {
            // Avoid "expression is constant" warning
            auto host           = HostPlatform;
            auto instructionSet = HostInstructionSet;
            if (host == Platform::Windows and instructionSet == InstructionSet::ARM64)
            {
                // Can't load the system installed x86_64 zlib dll from ARM64 executable
                continue;
            }

            if (test_section("file to socket to file"))
            {
                fileToSocketToFile();
            }
            if (numTestsToRun == 2)
            {
                // If on Linux next run will test io_uring backend (if it's installed)
                options.apiType = AsyncEventLoop::Options::ApiType::ForceUseIOURing;
            }
        }
    }

    void fileToFile();

    void fileToSocketToFile();

    void createAsyncConnectedSockets(AsyncEventLoop& eventLoop, SocketDescriptor& client,
                                     SocketDescriptor& serverSideClient)
    {
        SocketDescriptor serverSocket;
        uint16_t         tcpPort        = 5050;
        StringView       connectAddress = "::1";
        SocketIPAddress  nativeAddress;
        SC_TEST_EXPECT(nativeAddress.fromAddressPort(connectAddress, tcpPort));
        SC_TEST_EXPECT(serverSocket.create(nativeAddress.getAddressFamily()));

        {
            SocketServer server(serverSocket);
            SC_TEST_EXPECT(server.bind(nativeAddress));
            SC_TEST_EXPECT(server.listen(0));
        }

        SC_TEST_EXPECT(client.create(nativeAddress.getAddressFamily()));
        SC_TEST_EXPECT(SocketClient(client).connect(connectAddress, tcpPort));
        SC_TEST_EXPECT(SocketServer(serverSocket).accept(nativeAddress.getAddressFamily(), serverSideClient));
        SC_TEST_EXPECT(client.setBlocking(false));
        SC_TEST_EXPECT(serverSideClient.setBlocking(false));

        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedTCPSocket(client));
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedTCPSocket(serverSideClient));
    }
};

void SC::AsyncRequestStreamsTest::fileToFile()
{
    // This test:
    // 1. Creates a "readable.txt" file with some data
    // 2. Opens "readable.txt" as a readable stream
    // 3. Opens "writable.txt" as a writable stream
    // 4. Pipes the readable stream into the writable stream
    // 5. Checks that the content of the writable stream is correct

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));
    SC_TEST_EXPECT(fs.removeFileIfExists("readable.txt"));
    SC_TEST_EXPECT(fs.removeFileIfExists("writable.txt"));
    String readablePath;
    (void)Path::join(readablePath, {report.applicationRootDirectory, "readable.txt"});

    // Generate test data
    Vector<uint64_t> referenceData;
    (void)referenceData.resizeWithoutInitializing(1024 / sizeof(uint64_t));

    for (uint64_t idx = 0; idx < 1024 / sizeof(uint64_t); ++idx)
    {
        referenceData[size_t(idx)] = idx;
    }
    const auto spanOfChars = referenceData.toSpanConst().reinterpret_as_span_of<const char>();
    SC_TEST_EXPECT(fs.write(readablePath.view(), spanOfChars));

    // Setup Async Event Loop
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    constexpr size_t numberOfBuffers = 2;
    constexpr size_t bufferBytesSize = 16;
    AsyncBufferView  buffers[numberOfBuffers];
    Buffer           buffer;
    SC_TEST_EXPECT(buffer.resizeWithoutInitializing(bufferBytesSize * numberOfBuffers));
    for (size_t idx = 0; idx < numberOfBuffers; ++idx)
    {
        SC_TEST_EXPECT(buffer.toSpan().sliceStartLength(idx * bufferBytesSize, bufferBytesSize, buffers[idx].data));
    }
    AsyncBuffersPool pool;
    pool.buffers = {buffers, numberOfBuffers};

    ReadableFileStream           readable;
    AsyncReadableStream::Request readableRequests[numberOfBuffers + 1]; // Only N-1 slots will be used
    WritableFileStream           writable;
    AsyncWritableStream::Request writableRequests[numberOfBuffers + 1]; // Only N-1 slots will be used

    File::OpenOptions openOptions;
    openOptions.blocking = false; // Windows needs non-blocking flags set

    FileDescriptor readDescriptor;
    SC_TEST_EXPECT(File(readDescriptor).open(readablePath.view(), File::OpenMode::ReadOnly, openOptions));
    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(readDescriptor));

    FileDescriptor writeDescriptor;
    String         writeablePath;
    (void)Path::join(writeablePath, {report.applicationRootDirectory, "writeable.txt"});
    SC_TEST_EXPECT(File(writeDescriptor).open(writeablePath.view(), File::OpenMode::WriteCreateTruncate, openOptions));
    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(writeDescriptor));

    SC_TEST_EXPECT(readable.init(pool, readableRequests, eventLoop, readDescriptor));
    SC_TEST_EXPECT(writable.init(pool, writableRequests, eventLoop, writeDescriptor));

    // Create Pipeline

    AsyncWritableStream* sinks[1];
    sinks[0] = &writable;

    AsyncPipeline pipeline;
    SC_TEST_EXPECT(pipeline.pipe(readable, {sinks, 1}));
    SC_TEST_EXPECT(pipeline.start());

    SC_TEST_EXPECT(eventLoop.run());

    SC_TEST_EXPECT(writeDescriptor.close());
    SC_TEST_EXPECT(readDescriptor.close());

    // Final Check
    Buffer writableData;
    SC_TEST_EXPECT(fs.read(writeablePath.view(), writableData));

    Span<const uint64_t> writtenData = writableData.toSpanConst().reinterpret_as_span_of<const uint64_t>();

    SC_TEST_EXPECT(writtenData.sizeInBytes() == referenceData.toSpanConst().sizeInBytes());

    bool valuesOk = true;
    for (size_t idx = 0; idx < writtenData.sizeInElements(); ++idx)
    {
        valuesOk = valuesOk && (writtenData[idx] == referenceData.toSpanConst()[idx]);
    }
    SC_TEST_EXPECT(valuesOk);
    SC_TEST_EXPECT(fs.removeFiles({"readable.txt", "writeable.txt"}));
}

void SC::AsyncRequestStreamsTest::fileToSocketToFile()
{
    // This test is:
    // 1. Creates a "source.txt" file on disk filling it with some test data pattern
    // 2. Creates a readable file stream from  "source.txt"
    // 3. Creates a TCP socket pair (client server)
    // 4. Pipes the readable file into one of the two sockets, through a compression transform stream
    // 5. Pipe the receiving socket into a decompression transform stream, writing to a "destination.txt" file
    // 6. Once the entire file is read, the first pipeline is forcefully ended by disconnecting the socket
    // 7. This action triggers also ending the second pipeline (as we listen to the disconnected event)
    // 8. Once both pipelines are finished, the event loop has no more active handles ::run() will return
    // 9. Finally the test checks that the written file matches the original one.

    // First pipeline is: FileStream --> Compression --> WriteSocketStream
    // Second pipeline is: ReadSocketStream --> Decompression --> WriteFileStream

    // Generate data and write it to source.txt
    Vector<uint64_t> source;
    constexpr auto   numElements = 1 * 1024 / sizeof(uint64_t);
    SC_TEST_EXPECT(source.resizeWithoutInitializing(numElements));

    for (size_t idx = 0; idx < numElements; ++idx)
    {
        source[idx] = idx;
    }
    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));
    SC_TEST_EXPECT(fs.removeFileIfExists("source.txt"));
    SC_TEST_EXPECT(fs.removeFileIfExists("destination.txt"));
    SC_TEST_EXPECT(fs.write("source.txt", source.toSpanConst().reinterpret_as_span_of<const char>()));

    // Create Event Loop
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    // Allocate transient buffers
    AsyncBuffersPool buffersPool1;
    constexpr size_t numberOfBuffers1 = 3; // Need at least 3
    constexpr size_t buffers1Size     = 512;
    AsyncBufferView  buffers1[numberOfBuffers1];
    buffersPool1.buffers = {buffers1, numberOfBuffers1};
    Buffer buffer1;
    SC_TEST_EXPECT(buffer1.resizeWithoutInitializing(buffers1Size * numberOfBuffers1));
    for (size_t idx = 0; idx < numberOfBuffers1; ++idx)
    {
        SC_TEST_EXPECT(buffer1.toSpan().sliceStartLength(idx * buffers1Size, buffers1Size, buffers1[idx].data));
    }

    File::OpenOptions openOptions;
    openOptions.blocking = true;

    ThreadPool fileThreadPool;
    SC_TEST_EXPECT(fileThreadPool.create(2));

    ThreadPool compressionThreadPool;
    SC_TEST_EXPECT(compressionThreadPool.create(2));

    // Create Readable File Stream
    ReadableFileStream readFileStream;
    FileDescriptor     readFd;
    String             fileName;
    SC_TEST_EXPECT(Path::join(fileName, {report.applicationRootDirectory, "source.txt"}));
    SC_TEST_EXPECT(File(readFd).open(fileName.view(), File::OpenMode::ReadOnly, openOptions));
    AsyncFileRead::Task readFileTask;
    SC_TEST_EXPECT(readFileStream.request.setThreadPoolAndTask(fileThreadPool, readFileTask));
    AsyncReadableStream::Request readFileRequests[numberOfBuffers1 + 1];
    SC_TEST_EXPECT(readFileStream.init(buffersPool1, readFileRequests, eventLoop, readFd));

    // Create Writable File Stream
    WritableFileStream writeFileStream;
    FileDescriptor     writeFd;
    SC_TEST_EXPECT(Path::join(fileName, {report.applicationRootDirectory, "destination.txt"}));
    SC_TEST_EXPECT(File(writeFd).open(fileName.view(), File::OpenMode::WriteCreateTruncate, openOptions));
    AsyncFileWrite::Task writeFileTask;
    SC_TEST_EXPECT(writeFileStream.request.setThreadPoolAndTask(fileThreadPool, writeFileTask));

    // Allocate transient buffers
    AsyncBuffersPool buffersPool2;
    constexpr size_t numberOfBuffers2 = 3; // Need at least 3
    constexpr size_t buffers2Size     = 512;
    AsyncBufferView  buffers2[numberOfBuffers2 + 1];
    buffersPool2.buffers = {buffers2, numberOfBuffers2};
    Buffer buffer2;
    SC_TEST_EXPECT(buffer2.resizeWithoutInitializing(buffers2Size * numberOfBuffers2));
    for (size_t idx = 0; idx < numberOfBuffers2; ++idx)
    {
        SC_TEST_EXPECT(buffer2.toSpan().sliceStartLength(idx * buffers2Size, buffers2Size, buffers2[idx].data));
    }

    // Create sockets pairs
    SocketDescriptor client[2];
    createAsyncConnectedSockets(eventLoop, client[0], client[1]);

    // Create Writable Socket Stream
    WritableSocketStream         writeSocketStream;
    AsyncWritableStream::Request writeSocketRequests[numberOfBuffers1 + 1];
    SC_TEST_EXPECT(writeSocketStream.init(buffersPool1, writeSocketRequests, eventLoop, client[0]));
    // Autoclose socket after write stream receives an ::end()
    SC_TEST_EXPECT(writeSocketStream.registerAutoCloseDescriptor(true));
    client[0].detach(); // Taken care by registerAutoCloseDescriptor(true)

    // Create Readable Socket Stream
    ReadableSocketStream         readSocketStream;
    AsyncReadableStream::Request readSocketRequests[numberOfBuffers2 + 1];
    SC_TEST_EXPECT(readSocketStream.init(buffersPool2, readSocketRequests, eventLoop, client[1]));
    // Autoclose socket when socket stream receives an end event signaling socket disconnected
    SC_TEST_EXPECT(readSocketStream.registerAutoCloseDescriptor(true));
    client[1].detach(); // Taken care by registerAutoCloseDescriptor(true)
    (void)readSocketStream.eventError.addListener([this](Result res) { SC_TEST_EXPECT(res); });

    AsyncWritableStream::Request writeFileRequests[numberOfBuffers2 + 1];
    SC_TEST_EXPECT(writeFileStream.init(buffersPool2, writeFileRequests, eventLoop, writeFd));

    // Create first transform stream (compression)
    AsyncZLibTransformStream     compressStream;
    AsyncWritableStream::Request compressWriteRequests[numberOfBuffers1 + 1];
    AsyncReadableStream::Request compressReadRequests[numberOfBuffers1 + 1];
    SC_TEST_EXPECT(compressStream.init(buffersPool1, compressReadRequests, compressWriteRequests));
    SC_TEST_EXPECT(compressStream.stream.init(ZLibStream::CompressZLib));
    SC_TEST_EXPECT(compressStream.asyncWork.setThreadPool(compressionThreadPool));
    compressStream.asyncWork.cacheInternalEventLoop(eventLoop);
    compressStream.asyncWork.setDebugName("CompressStream");

    // Create first Async Pipeline (file to socket)
    AsyncDuplexStream*   transforms1[1] = {&compressStream};
    AsyncWritableStream* sinks1[1]      = {&writeSocketStream};
    AsyncPipeline        pipeline0;
    (void)pipeline0.eventError.addListener([this](Result res) { SC_TEST_EXPECT(res); });
    SC_TEST_EXPECT(pipeline0.pipe(readFileStream, transforms1, {sinks1}));

    // Create second transform stream (decompression)
    AsyncZLibTransformStream     decompressStream;
    AsyncWritableStream::Request decompressWriteRequests[numberOfBuffers2 + 1];
    AsyncReadableStream::Request decompressReadRequests[numberOfBuffers2 + 1];
    SC_TEST_EXPECT(decompressStream.init(buffersPool2, decompressReadRequests, decompressWriteRequests));
    SC_TEST_EXPECT(decompressStream.stream.init(ZLibStream::DecompressZLib));
    SC_TEST_EXPECT(decompressStream.asyncWork.setThreadPool(compressionThreadPool));
    decompressStream.asyncWork.cacheInternalEventLoop(eventLoop);
    decompressStream.asyncWork.setDebugName("DecompressStream");

    // Create second Async Pipeline (socket to file)
    AsyncDuplexStream*   transforms2[1] = {&decompressStream};
    AsyncWritableStream* sinks2[1]      = {&writeFileStream};
    AsyncPipeline        pipeline1;
    (void)pipeline1.eventError.addListener([this](Result res) { SC_TEST_EXPECT(res); });
    SC_TEST_EXPECT(pipeline1.pipe(readSocketStream, transforms2, {sinks2}));

    // Start both pipelines
    SC_TEST_EXPECT(pipeline0.start());
    SC_TEST_EXPECT(pipeline1.start());

    // Run Event Loop
    SC_TEST_EXPECT(eventLoop.run());

    // Cleanup
    SC_TEST_EXPECT(readFd.close());
    SC_TEST_EXPECT(writeFd.close());
    SC_TEST_EXPECT(not client[0].isValid());
    SC_TEST_EXPECT(not client[1].isValid());

    // Check written file content against source file
    Buffer destination;
    SC_TEST_EXPECT(destination.reserve(source.size() * sizeof(uint64_t)));
    SC_TEST_EXPECT(fs.read("destination.txt", destination));
    SC_TEST_EXPECT(destination.size() == source.size() * sizeof(uint64_t));

    SC_TEST_EXPECT(memcmp(destination.data(), source.data(), destination.size()) == 0);

    SC_TEST_EXPECT(fs.removeFiles({"source.txt", "destination.txt"}));
}

namespace SC
{
void runAsyncRequestStreamTest(SC::TestReport& report) { AsyncRequestStreamsTest test(report); }
} // namespace SC
