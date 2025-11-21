
// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/AsyncStreams/AsyncRequestStreams.h"
#include "Libraries/Async/Async.h"
#include "Libraries/AsyncStreams/AsyncStreams.h"
#include "Libraries/AsyncStreams/ZLibTransformStreams.h"
#include "Libraries/Containers/Vector.h"
#include "Libraries/File/File.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/Buffer.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Socket/Socket.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct AsyncRequestStreamsTest;
} // namespace SC

struct SC::AsyncRequestStreamsTest : public SC::TestCase
{
    AsyncEventLoop::Options options;
    AsyncRequestStreamsTest(SC::TestReport& report) : TestCase(report, "AsyncRequestStreamsTest")
    {
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
            if (test_section("file to file"))
            {
                fileToFile();
            }

            if (test_section("file to socket to file"))
            {
                // Create Event Loop
                AsyncEventLoop eventLoop;
                SC_TEST_EXPECT(eventLoop.create(options));

                // Create connected sockets pair
                SocketDescriptor writable, readable;
                createAsyncConnectedSockets(eventLoop, writable, readable);

                // Use *SocketStream as readable and writable are two SocketDescriptor
                // Sockets are duplex so the choice of who is writable and who is readable is just arbitrary
                fileCompressRemote<ReadableSocketStream, WritableSocketStream, AsyncZLibTransformStream>(
                    eventLoop, writable, readable, false);
            }

            if (test_section("file to pipe to file (async)"))
            {
                constexpr bool blocking = false;
                // Create Event Loop
                AsyncEventLoop eventLoop;
                SC_TEST_EXPECT(eventLoop.create(options));

                // Create an anonymous non-blocking pipe
                FileDescriptor writable, readable;
                createAsyncConnectedPipes(eventLoop, writable, readable, blocking);

                // Use *FileStream as readPipe and writePipe are two FileDescriptor
                // In Pipes there is a defined writeside and readside so the order of arguments here is important
                fileCompressRemote<ReadableFileStream, WritableFileStream, AsyncZLibTransformStream>(
                    eventLoop, writable, readable, blocking);
            }

            if (test_section("file to pipe to file (sync)"))
            {
                constexpr bool blocking = true;
                // Create Event Loop
                AsyncEventLoop eventLoop;
                SC_TEST_EXPECT(eventLoop.create(options));

                // Create an anonymous blocking pipe
                FileDescriptor writable, readable;
                createAsyncConnectedPipes(eventLoop, writable, readable, blocking);

                // Use *FileStream as readPipe and writePipe are two FileDescriptor
                // In Pipes there is a defined writeside and readside so the order of arguments here is important
                fileCompressRemote<ReadableFileStream, WritableFileStream, SyncZLibTransformStream>(eventLoop, writable,
                                                                                                    readable, blocking);
            }

            if (numTestsToRun == 2)
            {
                // If on Linux next run will test io_uring backend (if it's installed)
                options.apiType = AsyncEventLoop::Options::ApiType::ForceUseIOURing;
            }
        }
    }

    void createAsyncConnectedSockets(AsyncEventLoop& eventLoop, SocketDescriptor& writeSide,
                                     SocketDescriptor& readSide);
    void createAsyncConnectedPipes(AsyncEventLoop& eventLoop, FileDescriptor& writeSide, FileDescriptor& readSide,
                                   bool blocking);

    void fileToFile();

    template <typename READABLE_TYPE, typename WRITABLE_TYPE, typename ZLIB_STREAM_TYPE, typename DESCRIPTOR_TYPE>
    void fileCompressRemote(AsyncEventLoop& eventLoop, DESCRIPTOR_TYPE& writeSide, DESCRIPTOR_TYPE& readSide,
                            bool useStreamThreadPool);

    void setThreadPoolFor(AsyncZLibTransformStream& stream, AsyncEventLoop& eventLoop, ThreadPool& threadPool,
                          const char* name)
    {
        SC_TEST_EXPECT(stream.asyncWork.setThreadPool(threadPool));
        stream.setEventLoop(eventLoop);
        stream.asyncWork.setDebugName(name);
    }

    void setThreadPoolFor(SyncZLibTransformStream&, AsyncEventLoop&, ThreadPool&, const char*) {}
};

void SC::AsyncRequestStreamsTest::createAsyncConnectedSockets(AsyncEventLoop& eventLoop, SocketDescriptor& writeSide,
                                                              SocketDescriptor& readSide)
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

    SC_TEST_EXPECT(writeSide.create(nativeAddress.getAddressFamily()));
    SC_TEST_EXPECT(SocketClient(writeSide).connect(connectAddress, tcpPort));
    SC_TEST_EXPECT(SocketServer(serverSocket).accept(nativeAddress.getAddressFamily(), readSide));
    SC_TEST_EXPECT(writeSide.setBlocking(false));
    SC_TEST_EXPECT(readSide.setBlocking(false));

    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedSocket(writeSide));
    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedSocket(readSide));
}
void SC::AsyncRequestStreamsTest::createAsyncConnectedPipes(AsyncEventLoop& eventLoop, FileDescriptor& writeSide,
                                                            FileDescriptor& readSide, bool blocking)
{
    PipeDescriptor pipe;
    PipeOptions    pipeOptions;
    pipeOptions.blocking = blocking;
    SC_TEST_EXPECT(pipe.createPipe(pipeOptions));
    if (not pipeOptions.blocking)
    {
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(pipe.writePipe));
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(pipe.readPipe));
    }
    writeSide = move(pipe.writePipe);
    readSide  = move(pipe.readPipe);
}

void SC::AsyncRequestStreamsTest::fileToFile()
{
    // This test:
    // 1. Creates a "readable.txt" file with some data
    // 2. Opens "readable.txt" as a readable stream
    // 3. Opens "writable.txt" as a writable stream
    // 4. Pipes the readable stream into the writable stream
    // 5. Checks that the content of the writable stream is correct

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.removeFileIfExists("readable.txt"));
    SC_TEST_EXPECT(fs.removeFileIfExists("writable.txt"));
    String readablePath;
    (void)Path::join(readablePath, {report.applicationRootDirectory.view(), "readable.txt"});

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
        Span<char> writableData;
        SC_TEST_EXPECT(buffer.toSpan().sliceStartLength(idx * bufferBytesSize, bufferBytesSize, writableData));
        buffers[idx] = writableData;
    }
    AsyncBuffersPool pool;
    pool.buffers = {buffers, numberOfBuffers};

    ReadableFileStream           readable;
    AsyncReadableStream::Request readableRequests[numberOfBuffers + 1]; // Only N-1 slots will be used
    WritableFileStream           writable;
    AsyncWritableStream::Request writableRequests[numberOfBuffers + 1]; // Only N-1 slots will be used

    FileOpen openModeRead;
    openModeRead.mode     = FileOpen::Read;
    openModeRead.blocking = false; // Windows needs non-blocking flags set

    FileDescriptor readDescriptor;
    SC_TEST_EXPECT(readDescriptor.open(readablePath.view(), openModeRead));
    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(readDescriptor));

    FileDescriptor writeDescriptor;
    String         writeablePath;
    (void)Path::join(writeablePath, {report.applicationRootDirectory.view(), "writeable.txt"});
    FileOpen openModeWrite;
    openModeWrite.mode     = FileOpen::Write;
    openModeWrite.blocking = false; // Windows needs non-blocking flags set
    SC_TEST_EXPECT(writeDescriptor.open(writeablePath.view(), openModeWrite));
    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(writeDescriptor));

    SC_TEST_EXPECT(readable.init(pool, readableRequests, eventLoop, readDescriptor));
    SC_TEST_EXPECT(writable.init(pool, writableRequests, eventLoop, writeDescriptor));

    // Create Pipeline

    AsyncPipeline pipeline = {&readable, {}, {&writable}};
    SC_TEST_EXPECT(pipeline.pipe());
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

template <typename READABLE_TYPE, typename WRITABLE_TYPE, typename ZLIB_STREAM_TYPE, typename DESCRIPTOR_TYPE>
void SC::AsyncRequestStreamsTest::fileCompressRemote(AsyncEventLoop& eventLoop, DESCRIPTOR_TYPE& writeSide,
                                                     DESCRIPTOR_TYPE& readSide, bool useStreamThreadPool)
{
    // This test:
    // 1. Accepts a connected pair of sockets or the two sides of a pipe (flowing data from writeSide to readSide)
    // 2. Creates a "source.txt" file on disk filling it with some test data pattern
    // 3. Creates a readable file stream for "source.txt" and a writable file stream for "destination.txt"
    // 4. Pipes the readable file into the writeSide, through a compression transform stream
    // 5. Pipes the readSide (receiving from writeSide) to a decompression transform stream piped to a writable file
    // 6. Once the entire file is read, the first pipeline is forcefully ended by closing the two sides
    // 7. This action triggers also ending the second pipeline (as we listen to the disconnected event)
    // 8. Once both pipelines are finished, the event loop has no more active handles ::run() will return
    // 9. Finally the test checks that the written file matches the original one.

    // First pipeline is: FileStream --> Compression --> WRITABLE_TYPE
    // Second pipeline is: READABLE_TYPE --> Decompression --> WriteFileStream
    // Anything written to WRITABLE_TYPE will be available reading from READABLE_TYPE

    // Generate test data and write it to source.txt
    Vector<uint64_t> source;
    constexpr auto   numElements = 1 * 1024 / sizeof(uint64_t);
    SC_TEST_EXPECT(source.resizeWithoutInitializing(numElements));

    for (size_t idx = 0; idx < numElements; ++idx)
    {
        source[idx] = idx;
    }
    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.removeFileIfExists("source.txt"));
    SC_TEST_EXPECT(fs.removeFileIfExists("destination.txt"));
    SC_TEST_EXPECT(fs.write("source.txt", source.toSpanConst().reinterpret_as_span_of<const char>()));

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
        Span<char> writableData;
        SC_TEST_EXPECT(buffer1.toSpan().sliceStartLength(idx * buffers1Size, buffers1Size, writableData));
        buffers1[idx] = writableData;
    }

    ThreadPool fileThreadPool;
    SC_TEST_EXPECT(fileThreadPool.create(2));

    ThreadPool compressionThreadPool;
    SC_TEST_EXPECT(compressionThreadPool.create(2));

    // Create Readable File Stream
    ReadableFileStream readFileStream;
    FileDescriptor     readFd;
    String             fileName;
    SC_TEST_EXPECT(Path::join(fileName, {report.applicationRootDirectory.view(), "source.txt"}));
    FileOpen openModeRead;
    openModeRead.mode     = FileOpen::Read;
    openModeRead.blocking = true;
    SC_TEST_EXPECT(readFd.open(fileName.view(), openModeRead));
    AsyncTaskSequence readFileTask;
    readFileStream.request.setDebugName("File Source");
    SC_TEST_EXPECT(readFileStream.request.executeOn(readFileTask, fileThreadPool));
    AsyncReadableStream::Request readFileRequests[numberOfBuffers1 + 1];
    SC_TEST_EXPECT(readFileStream.init(buffersPool1, readFileRequests, eventLoop, readFd));

    // Create Writable File Stream
    WritableFileStream writeFileStream;
    FileDescriptor     writeFd;
    SC_TEST_EXPECT(Path::join(fileName, {report.applicationRootDirectory.view(), "destination.txt"}));
    FileOpen openModeWrite;
    openModeWrite.mode     = FileOpen::Write;
    openModeWrite.blocking = true;
    SC_TEST_EXPECT(writeFd.open(fileName.view(), openModeWrite));
    AsyncTaskSequence writeFileTask;
    writeFileStream.request.setDebugName("File Sink");
    SC_TEST_EXPECT(writeFileStream.request.executeOn(writeFileTask, fileThreadPool));

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
        Span<char> writableData;
        SC_TEST_EXPECT(buffer2.toSpan().sliceStartLength(idx * buffers2Size, buffers2Size, writableData));
        buffers2[idx] = writableData;
    }
    ThreadPool streamPool;
    if (useStreamThreadPool)
    {
        SC_TEST_EXPECT(streamPool.create(2));
    }

    // Create Writable Socket Stream
    WRITABLE_TYPE                writeSideStream;
    AsyncWritableStream::Request writeSideRequests[numberOfBuffers1 + 1];
    SC_TEST_EXPECT(writeSideStream.init(buffersPool1, writeSideRequests, eventLoop, writeSide));
    // Autoclose socket after write stream receives an ::end()
    writeSideStream.request.setDebugName("Writable Side");
    AsyncTaskSequence writeStreamTask;
    if (useStreamThreadPool)
    {
        SC_TEST_EXPECT(writeSideStream.request.executeOn(writeStreamTask, streamPool));
    }
    writeSideStream.setAutoCloseDescriptor(true);
    writeSide.detach(); // Taken care by setAutoCloseDescriptor(true)

    // Create Readable Socket Stream
    READABLE_TYPE                readSideStream;
    AsyncReadableStream::Request readSideRequests[numberOfBuffers2 + 1];
    SC_TEST_EXPECT(readSideStream.init(buffersPool2, readSideRequests, eventLoop, readSide));
    // Autoclose socket when socket stream receives an end event signaling socket disconnected
    readSideStream.request.setDebugName("Readable Side");
    AsyncTaskSequence readStreamTask;
    if (useStreamThreadPool)
    {
        SC_TEST_EXPECT(readSideStream.request.executeOn(readStreamTask, streamPool));
    }
    readSideStream.setAutoCloseDescriptor(true);
    readSide.detach(); // Taken care by setAutoCloseDescriptor(true)
    (void)readSideStream.eventError.addListener([this](Result res) { SC_TEST_EXPECT(res); });

    AsyncWritableStream::Request writeFileRequests[numberOfBuffers2 + 1];
    SC_TEST_EXPECT(writeFileStream.init(buffersPool2, writeFileRequests, eventLoop, writeFd));

    // Create first transform stream (compression)
    ZLIB_STREAM_TYPE             compressStream;
    AsyncWritableStream::Request compressWriteRequests[numberOfBuffers1 + 1];
    AsyncReadableStream::Request compressReadRequests[numberOfBuffers1 + 1];
    SC_TEST_EXPECT(compressStream.init(buffersPool1, compressReadRequests, compressWriteRequests));
    SC_TEST_EXPECT(compressStream.stream.init(ZLibStream::CompressZLib));
    setThreadPoolFor(compressStream, eventLoop, compressionThreadPool, "CompressStream");

    // Create first Async Pipeline (file to socket)
    AsyncPipeline pipeline0 = {&readFileStream, {&compressStream}, {&writeSideStream}};
    (void)pipeline0.eventError.addListener([this](Result res) { SC_TEST_EXPECT(res); });
    SC_TEST_EXPECT(pipeline0.pipe());

    // Create second transform stream (decompression)
    ZLIB_STREAM_TYPE             decompressStream;
    AsyncWritableStream::Request decompressWriteRequests[numberOfBuffers2 + 1];
    AsyncReadableStream::Request decompressReadRequests[numberOfBuffers2 + 1];
    SC_TEST_EXPECT(decompressStream.init(buffersPool2, decompressReadRequests, decompressWriteRequests));
    SC_TEST_EXPECT(decompressStream.stream.init(ZLibStream::DecompressZLib));
    setThreadPoolFor(decompressStream, eventLoop, compressionThreadPool, "DecompressStream");

    // Create second Async Pipeline (socket to file)
    AsyncPipeline pipeline1 = {&readSideStream, {&decompressStream}, {&writeFileStream}};
    (void)pipeline1.eventError.addListener([this](Result res) { SC_TEST_EXPECT(res); });
    SC_TEST_EXPECT(pipeline1.pipe());

    // Start both pipelines
    SC_TEST_EXPECT(pipeline0.start());
    SC_TEST_EXPECT(pipeline1.start());

    // Run Event Loop
    SC_TEST_EXPECT(eventLoop.run());

    // Cleanup
    SC_TEST_EXPECT(readFd.close());
    SC_TEST_EXPECT(writeFd.close());
    SC_TEST_EXPECT(not writeSide.isValid());
    SC_TEST_EXPECT(not readSide.isValid());

    // Check written file content against source file
    Buffer destination;
    SC_TEST_EXPECT(destination.reserve(source.size() * sizeof(uint64_t)));
    SC_TEST_EXPECT(fs.read("destination.txt", destination));
    SC_TEST_EXPECT(destination.size() == source.size() * sizeof(uint64_t));

    SC_TEST_EXPECT(::memcmp(destination.data(), source.data(), destination.size()) == 0);

    SC_TEST_EXPECT(fs.removeFiles({"source.txt", "destination.txt"}));
}

namespace SC
{
void runAsyncRequestStreamTest(SC::TestReport& report) { AsyncRequestStreamsTest test(report); }
} // namespace SC
