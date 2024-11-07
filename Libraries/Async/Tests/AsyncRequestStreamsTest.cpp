
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

struct AsyncRequestReadableStream : public AsyncReadableStream
{
    AsyncRequestReadableStream()
    {
        AsyncReadableStream::asyncRead.bind<AsyncRequestReadableStream, &AsyncRequestReadableStream::read>(*this);
    }

    Result init(AsyncBuffersPool& buffersPool, Span<Request> requests, AsyncEventLoop& eventLoop,
                FileDescriptor& descriptor)
    {
        SC_TRY(descriptor.get(request.fileDescriptor, Result::Error("no descriptor")));
        request.cacheInternalEventLoop(eventLoop);
        return AsyncReadableStream::init(buffersPool, requests);
    }

    AsyncFileRead request;

  private:
    Result read()
    {
        AsyncBufferView::ID bufferID;
        Span<char>          data;
        if (getBufferOrPause(0, bufferID, data))
        {
            request.buffer   = data;
            request.callback = [this, bufferID](AsyncFileRead::Result& result) { onRead(result, bufferID); };
            const Result res = request.start(*request.getEventLoop());
            if (not res)
            {
                getBuffersPool().unrefBuffer(bufferID);
            }
            return res;
        }
        return Result::Error("no buffers");
    }

    void onRead(AsyncFileRead::Result& result, AsyncBufferView::ID bufferID)
    {
        Span<char>   data;
        const Result res = result.get(data);
        if (res)
        {
            if (result.completionData.endOfFile)
            {
                getBuffersPool().unrefBuffer(bufferID);
                AsyncRequestReadableStream::pushEnd();
            }
            else
            {
                AsyncRequestReadableStream::push(bufferID, data.sizeInBytes());
                getBuffersPool().unrefBuffer(bufferID);
                if (AsyncRequestReadableStream::getBufferOrPause(0, bufferID, data))
                {
                    request.buffer   = data;
                    request.callback = [this, bufferID](AsyncFileRead::Result& result) { onRead(result, bufferID); };
                    result.reactivateRequest(true);
                }
            }
        }
        else
        {
            getBuffersPool().unrefBuffer(bufferID);
            emitError(res);
        }
    }
};

struct AsyncRequestWritableStream : public AsyncWritableStream
{
    AsyncRequestWritableStream()
    {
        AsyncWritableStream::asyncWrite.bind<AsyncRequestWritableStream, &AsyncRequestWritableStream::write>(*this);
    }

    Result init(AsyncBuffersPool& buffersPool, Span<Request> requests, AsyncEventLoop& eventLoop,
                FileDescriptor& fileDescriptor)
    {
        SC_TRY(fileDescriptor.get(request.fileDescriptor, Result::Error("no descriptor")));
        request.cacheInternalEventLoop(eventLoop);
        return AsyncWritableStream::init(buffersPool, requests);
    }

    AsyncFileWrite request;

  private:
    Function<void(AsyncBufferView::ID)> callback;

    Result write(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb)
    {
        Span<const char> data;
        if (getBuffersPool().getData(bufferID, data))
        {
            callback         = cb;
            request.callback = [this, bufferID](AsyncFileWrite::Result& result) { afterWrite(result, bufferID); };
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

    void afterWrite(AsyncFileWrite::Result& result, AsyncBufferView::ID bufferID)
    {
        getBuffersPool().unrefBuffer(bufferID);
        size_t       sizeInBytes  = 0;
        const Result res          = result.get(sizeInBytes);
        auto         callbackCopy = move(callback);
        callback                  = {};
        AsyncWritableStream::finishedWriting(bufferID, move(callbackCopy), res);
    }
};

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

    constexpr size_t numberOfBuffers = 2;
    constexpr size_t bufferBytesSize = 16;
    AsyncBufferView  buffers[numberOfBuffers];
    HeapBuffer       buffer;
    SC_TEST_EXPECT(buffer.allocate(bufferBytesSize * numberOfBuffers));
    for (size_t idx = 0; idx < numberOfBuffers; ++idx)
    {
        SC_TEST_EXPECT(buffer.data.sliceStartLength(idx * bufferBytesSize, bufferBytesSize, buffers[idx].data));
    }
    AsyncBuffersPool pool;
    pool.buffers = {buffers, numberOfBuffers};

    AsyncRequestReadableStream   readable;
    AsyncReadableStream::Request readableRequests[numberOfBuffers + 1]; // Only N-1 slots will be used
    AsyncRequestWritableStream   writable;
    AsyncWritableStream::Request writableRequests[numberOfBuffers + 1]; // Only N-1 slots will be used

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

    SC_TEST_EXPECT(readable.init(pool, readableRequests, eventLoop, readDescriptor));
    SC_TEST_EXPECT(writable.init(pool, writableRequests, eventLoop, writeDescriptor));

    // Create Pipeline
    AsyncPipeline pipeline;

    AsyncPipeline::Sink destinations[1];
    pipeline.source      = &readable;
    pipeline.destination = {destinations, 1};
    destinations[0].sink = &writable;

    SC_TEST_EXPECT(pipeline.init());
    SC_TEST_EXPECT(pipeline.start());

    SC_TEST_EXPECT(eventLoop.run());

    SC_TEST_EXPECT(writeDescriptor.close());
    SC_TEST_EXPECT(readDescriptor.close());

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
