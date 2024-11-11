
// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Async/Async.h"
#include "../../Async/AsyncStreams.h"
#include "../../FileSystem/FileSystem.h"
#include "../../FileSystem/Path.h"
#include "../../Foundation/HeapBuffer.h"
#include "../../Strings/StringBuilder.h"
#include "../../Testing/Testing.h"

namespace SC
{
struct AsyncRequestStreamsTest;
} // namespace SC

struct SC::AsyncRequestStreamsTest : public SC::TestCase
{
    AsyncRequestStreamsTest(SC::TestReport& report) : TestCase(report, "AsyncRequestStreamsTest")
    {
        if (test_section("file to File"))
        {
            fileToFile();
        }
    }
    void fileToFile();
    
    void createTCPSocketPair(AsyncEventLoop& eventLoop, SocketDescriptor& client,
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

namespace SC
{
struct AsyncPipeline
{
    static constexpr int        MaxListeners = 5;
    Event<MaxListeners, Result> eventError;

    AsyncReadableStream* source = nullptr;

    struct Sink
    {
        AsyncWritableStream* sink = nullptr;
    };
    Span<Sink> destination;

    Result init()
    {
        SC_TRY((source->eventData.addListener<AsyncPipeline, &AsyncPipeline::onBufferRead>(*this)));

        for (Sink& sink : destination)
        {
            if (&sink.sink->getBuffersPool() != &source->getBuffersPool())
            {
                return Result::Error("Source and sinks must have the same buffer pool");
            }
        }

        return Result(true);
    }

    Result start() { return source->start(); }

  private:
    void onBufferRead(AsyncBufferView::ID bufferID)
    {
        for (Sink& sink : destination)
        {
            source->getBuffersPool().refBuffer(bufferID);
            Function<void(AsyncBufferView::ID)> callback;
            callback.bind<AsyncPipeline, &AsyncPipeline::onBufferWritten>(*this);
            Result res = sink.sink->write(bufferID, move(callback));
            if (not res)
            {
                eventError.emit(res);
            }
        }
    }

    void onBufferWritten(AsyncBufferView::ID bufferID)
    {
        source->getBuffersPool().unrefBuffer(bufferID);
        source->resume();
    }
};

template<typename RequestType>
struct AsyncRequestReadableStream : public AsyncReadableStream
{
    struct Internal
    {
        [[nodiscard]] static bool isEnded(AsyncFileRead::Result& result) { return result.completionData.endOfFile; }
        [[nodiscard]] static bool isEnded(AsyncSocketReceive::Result& result) { return result.completionData.disconnected; }
        
        [[nodiscard]] static SocketDescriptor::Handle& getDescriptor(AsyncSocketReceive& request){ return request.handle; }
        [[nodiscard]] static FileDescriptor::Handle& getDescriptor(AsyncFileRead& request){ return request.fileDescriptor; }
    };
    
    AsyncRequestReadableStream()
    {
        AsyncReadableStream::asyncRead.bind<AsyncRequestReadableStream, &AsyncRequestReadableStream::read>(*this);
    }

    template<typename DescriptorType>
    Result init(AsyncBuffersPool& buffersPool, Span<Request> requests, AsyncEventLoop& eventLoop,
                DescriptorType& descriptor)
    {
        SC_TRY(descriptor.get(Internal::getDescriptor(request), Result::Error("no descriptor")));
        request.cacheInternalEventLoop(eventLoop);
        return AsyncReadableStream::init(buffersPool, requests);
    }


    RequestType request;

  private:
    Result read()
    {
        AsyncBufferView::ID bufferID;
        Span<char>          data;
        if (getBufferOrPause(0, bufferID, data))
        {
            request.buffer   = data;
            request.callback = [this, bufferID](typename RequestType::Result& result) { onRead(result, bufferID); };
            const Result res = request.start(*request.getEventLoop());
            if (not res)
            {
                getBuffersPool().unrefBuffer(bufferID);
            }
            return res;
        }
        return Result::Error("no buffers");
    }

    void onRead(typename RequestType::Result& result, AsyncBufferView::ID bufferID)
    {
        if (Internal::isEnded(result))
        {
            getBuffersPool().unrefBuffer(bufferID);
            AsyncRequestReadableStream::pushEnd();
        }
        else
        {
            Span<char>   data;
            const Result res = result.get(data);
            if (res)
            {
                AsyncRequestReadableStream::push(bufferID, data.sizeInBytes());
                getBuffersPool().unrefBuffer(bufferID);
                if (AsyncRequestReadableStream::getBufferOrPause(0, bufferID, data))
                {
                    request.buffer   = data;
                    request.callback = [this, bufferID](typename RequestType::Result& result) { onRead(result, bufferID); };
                    result.reactivateRequest(true);
                }
            }
            else
            {
                getBuffersPool().unrefBuffer(bufferID);
                emitError(res);
            }
        }
    }
};


template<typename RequestType>
struct AsyncRequestWritableStream : public AsyncWritableStream
{
    struct Internal
    {
        [[nodiscard]] static SocketDescriptor::Handle& getDescriptor(AsyncSocketSend& request){ return request.handle; }
        [[nodiscard]] static FileDescriptor::Handle& getDescriptor(AsyncFileWrite& request){ return request.fileDescriptor; }
    };
    AsyncRequestWritableStream()
    {
        AsyncWritableStream::asyncWrite.bind<AsyncRequestWritableStream, &AsyncRequestWritableStream::write>(*this);
    }

    template<typename DescriptorType>
    Result init(AsyncBuffersPool& buffersPool, Span<Request> requests, AsyncEventLoop& eventLoop,
                DescriptorType& descriptor)
    {
        SC_TRY(descriptor.get(Internal::getDescriptor(request), Result::Error("no descriptor")));
        request.cacheInternalEventLoop(eventLoop);
        return AsyncWritableStream::init(buffersPool, requests);
    }

    RequestType request;

  private:
    Function<void(AsyncBufferView::ID)> callback;

    Result write(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb)
    {
        Span<const char> data;
        if (getBuffersPool().getData(bufferID, data))
        {
            callback         = cb;
            request.callback = [this, bufferID](typename RequestType::Result& result) { afterWrite(result, bufferID); };
            request.buffer   = data;
            const Result res = request.start(*request.getEventLoop());
            if (res)
            {
                getBuffersPool().refBuffer(bufferID);
            }
            return res;
        }
        return Result::Error("no data");
    }

    void afterWrite(typename RequestType::Result& result, AsyncBufferView::ID bufferID)
    {
        getBuffersPool().unrefBuffer(bufferID);
        auto         callbackCopy = move(callback);
        callback                  = {};
        AsyncWritableStream::finishedWriting(bufferID, move(callbackCopy), result.isValid());
    }
};

using AsyncFileReadableStream = AsyncRequestReadableStream<AsyncFileRead>;
using AsyncFileWritableStream = AsyncRequestWritableStream<AsyncFileWrite>;
using AsyncSocketReadableStream = AsyncRequestReadableStream<AsyncSocketReceive>;
using AsyncSocketWritableStream = AsyncRequestWritableStream<AsyncSocketSend>;

} // namespace SC

void SC::AsyncRequestStreamsTest::fileToFile()
{
    String readablePath;
    (void)Path::join(readablePath, {report.applicationRootDirectory, "readable.txt"});

    // Generate test data
    Vector<uint64_t> referenceData;
    (void)referenceData.resize(1024 / sizeof(uint64_t));

    for (uint64_t idx = 0; idx < 1024 / sizeof(uint64_t); ++idx)
    {
        referenceData[idx] = idx;
    }
    {
        FileSystem fs;
        SC_TEST_EXPECT(
            fs.write(readablePath.view(), referenceData.toSpanConst().reinterpret_as_array_of<const char>()));
    }

    // Setup Async Event Loop
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    constexpr size_t numberOfBuffers1 = 2;
    constexpr size_t buffer1BytesSize = 16;
    AsyncBufferView  buffers1[numberOfBuffers1];
    HeapBuffer       buffer1;
    SC_TEST_EXPECT(buffer1.allocate(buffer1BytesSize * numberOfBuffers1));
    for (size_t idx = 0; idx < numberOfBuffers1; ++idx)
    {
        SC_TEST_EXPECT(buffer1.data.sliceStartLength(idx * buffer1BytesSize, buffer1BytesSize, buffers1[idx].data));
    }
    AsyncBuffersPool pool1;
    pool1.buffers = {buffers1, numberOfBuffers1};
    
    AsyncFileReadableStream   fileReadableStream;
    AsyncReadableStream::Request fileReadableRequests[numberOfBuffers1 + 1]; // Only N-1 slots will be used
    AsyncFileWritableStream   fileWritableStream;
    AsyncWritableStream::Request fileWritableRequests[numberOfBuffers1 + 1]; // Only N-1 slots will be used

    FileDescriptor::OpenOptions openOptions;
    openOptions.blocking = false; // Windows needs non-blocking flags set

    FileDescriptor readDescriptor;
    SC_TEST_EXPECT(readDescriptor.open(readablePath.view(), FileDescriptor::OpenMode::ReadOnly, openOptions));
    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(readDescriptor));

    FileDescriptor writeDescriptor;
    String         writeablePath;
    (void)Path::join(writeablePath, {report.applicationRootDirectory, "writeable.txt"});
    SC_TEST_EXPECT(
        writeDescriptor.open(writeablePath.view(), FileDescriptor::OpenMode::WriteCreateTruncate, openOptions));
    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(writeDescriptor));

    
    
    constexpr size_t numberOfBuffers2 = 2;
    constexpr size_t buffer2BytesSize = 16;
    AsyncBufferView  buffers2[numberOfBuffers2];
    HeapBuffer       buffer2;
    SC_TEST_EXPECT(buffer2.allocate(buffer2BytesSize * numberOfBuffers2));
    for (size_t idx = 0; idx < numberOfBuffers2; ++idx)
    {
        SC_TEST_EXPECT(buffer2.data.sliceStartLength(idx * buffer2BytesSize, buffer2BytesSize, buffers2[idx].data));
    }
    AsyncBuffersPool pool2;
    pool2.buffers = {buffers2, numberOfBuffers2};
    
    AsyncSocketReadableStream   socketReadableStream;
    AsyncReadableStream::Request socketReadableRequests[numberOfBuffers2 + 1]; // Only N-1 slots will be used
    AsyncSocketWritableStream   socketWritableStream;
    AsyncWritableStream::Request socketWritableRequests[numberOfBuffers2 + 1]; // Only N-1 slots will be used

    SocketDescriptor client[2];
    createTCPSocketPair(eventLoop, client[0], client[1]);

    SC_TEST_EXPECT(fileReadableStream.init(pool1, fileReadableRequests, eventLoop, readDescriptor));
    SC_TEST_EXPECT(socketWritableStream.init(pool1, socketWritableRequests, eventLoop, client[0]));
    (void)fileReadableStream.eventEnd.addListener([&socketWritableStream](){
        socketWritableStream.end();
    });
    
    (void)socketWritableStream.eventFinish.addListener([&client](){
        (void)client[0].close();
    });
    
    
    SC_TEST_EXPECT(socketReadableStream.init(pool2, socketReadableRequests, eventLoop, client[1]));
    (void)socketReadableStream.eventEnd.addListener([&fileWritableStream](){
        fileWritableStream.end();
    });
    SC_TEST_EXPECT(fileWritableStream.init(pool2, fileWritableRequests, eventLoop, writeDescriptor));

    // Create Pipeline
    AsyncPipeline pipeline[2];

    AsyncPipeline::Sink destinations1[1];
    pipeline[0].source      = &fileReadableStream;
    pipeline[0].destination = {destinations1, 1};
    destinations1[0].sink = &socketWritableStream;

    AsyncPipeline::Sink destinations2[1];
    pipeline[1].source      = &socketReadableStream;
    pipeline[1].destination = {destinations2, 1};
    destinations2[0].sink = &fileWritableStream;

    SC_TEST_EXPECT(pipeline[0].init());
    SC_TEST_EXPECT(pipeline[0].start());
    SC_TEST_EXPECT(pipeline[1].init());
    SC_TEST_EXPECT(pipeline[1].start());

    SC_TEST_EXPECT(eventLoop.run());

    SC_TEST_EXPECT(writeDescriptor.close());
    SC_TEST_EXPECT(readDescriptor.close());
    SC_TEST_EXPECT(client[0].close());
    SC_TEST_EXPECT(client[0].close());

    // Final Check
    FileSystem   fs;
    Vector<char> writableData;
    SC_TEST_EXPECT(fs.read(writeablePath.view(), writableData));

    Span<const uint64_t> writtenData = writableData.toSpanConst().reinterpret_as_array_of<const uint64_t>();

    SC_TEST_EXPECT(writtenData.sizeInBytes() == referenceData.toSpanConst().sizeInBytes());

    bool valuesOk = true;
    for (size_t idx = 0; idx < writtenData.sizeInElements(); ++idx)
    {
        valuesOk = valuesOk && (writtenData[idx] == referenceData.toSpanConst()[idx]);
    }
    SC_TEST_EXPECT(valuesOk);
}

namespace SC
{
void runAsyncRequestStreamTest(SC::TestReport& report) { AsyncRequestStreamsTest test(report); }
} // namespace SC
