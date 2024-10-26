
// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Async/AsyncStreams.h"
#include "../../Async/Async.h"
#include "../../FileSystem/FileSystem.h"
#include "../../FileSystem/Path.h"
#include "../../Foundation/HeapBuffer.h"
#include "../../Strings/StringBuilder.h"
#include "../../Testing/Testing.h"

namespace SC
{
struct AsyncStreamsTest;
} // namespace SC

struct SC::AsyncStreamsTest : public SC::TestCase
{
    int  memberCalls = 0;
    void funcCallback(int value)
    {
        SC_TEST_EXPECT(value == 1);
        memberCalls++;
    }

    AsyncStreamsTest(SC::TestReport& report) : TestCase(report, "AsyncStreamsTest")
    {
        if (test_section("Event"))
        {
            event();
        }
        if (test_section("CircularQueue"))
        {
            circularQueue();
        }
        if (test_section("readableSyncStream"))
        {
            readableSyncStream();
        }
        if (test_section("readableAsyncStream"))
        {
            readableAsyncStream();
        }
    }

    void event();
    void circularQueue();
    void readableSyncStream();
    void readableAsyncStream();
};

void SC::AsyncStreamsTest::circularQueue()
{
    int                buffer[3];
    CircularQueue<int> circularBuffer = {buffer};

    SC_TEST_EXPECT(circularBuffer.isEmpty());
    SC_TEST_EXPECT(circularBuffer.pushBack(1));
    SC_TEST_EXPECT(circularBuffer.pushBack(2));
    // Can only use up to N-1 (3-1 == 2) slots
    SC_TEST_EXPECT(not circularBuffer.pushBack(3));
    int res;
    SC_TEST_EXPECT(circularBuffer.popFront(res));
    SC_TEST_EXPECT(res == 1);
    SC_TEST_EXPECT(circularBuffer.pushBack(3));
    SC_TEST_EXPECT(circularBuffer.popFront(res));
    SC_TEST_EXPECT(res == 2);
    SC_TEST_EXPECT(circularBuffer.popFront(res));
    SC_TEST_EXPECT(res == 3);
    SC_TEST_EXPECT(not circularBuffer.popFront(res));
    SC_TEST_EXPECT(circularBuffer.isEmpty());
}

void SC::AsyncStreamsTest::event()
{
    Event<2, int> event;
    int           memberIndex = -1;
    SC_TEST_EXPECT((event.addListener<AsyncStreamsTest, &AsyncStreamsTest::funcCallback>(*this, &memberIndex)));
    event.emit(1);
    SC_TEST_EXPECT(memberCalls == 1); // +1
    int value = 1;
    event.emit(value);
    SC_TEST_EXPECT(memberCalls == 2); // +1
    event.emit(move(value));
    SC_TEST_EXPECT(memberCalls == 3); // +1
    int  lambdaCalls = 0;
    auto lambda      = [this, &lambdaCalls](int param)
    {
        SC_TEST_EXPECT(param == 1);
        lambdaCalls++;
    };
    SC_TEST_EXPECT(event.addListener(lambda));
    event.emit(1);
    SC_TEST_EXPECT(memberCalls == 4); // +1
    SC_TEST_EXPECT(lambdaCalls == 1); // +1
    SC_TEST_EXPECT(event.removeListenerAt(memberIndex));
    event.emit(1);
    SC_TEST_EXPECT(memberCalls == 4); // +0
    SC_TEST_EXPECT(lambdaCalls == 2); // +1
    SC_TEST_EXPECT(event.removeListener(lambda));
    event.emit(1);
    SC_TEST_EXPECT(memberCalls == 4); // +0
    SC_TEST_EXPECT(lambdaCalls == 2); // +0
}

void SC::AsyncStreamsTest::readableSyncStream()
{
    // Create a pool of byte buffers slicing a single HeapBuffer in multiple AsyncBufferView(s)
    constexpr size_t numberOfBuffers = 2;
    constexpr size_t bufferBytesSize = sizeof(size_t);
    AsyncBufferView  buffers[numberOfBuffers];
    HeapBuffer       buffer;
    SC_TEST_EXPECT(buffer.allocate(bufferBytesSize * numberOfBuffers));
    for (size_t idx = 0; idx < numberOfBuffers; ++idx)
    {
        SC_TEST_EXPECT(buffer.data.sliceStartLength(idx * bufferBytesSize, bufferBytesSize, buffers[idx].data));
    }
    AsyncBuffersPool pool;
    pool.buffers = {buffers, numberOfBuffers};

    AsyncReadableStream          readable;
    AsyncReadableStream::Request requests[numberOfBuffers + 1]; // Only N-1 slots will be used
    SC_TEST_EXPECT(readable.init(pool, requests));
    struct Context
    {
        AsyncReadableStream& readable;
        size_t               idx;
        size_t               max;
        Vector<size_t>       indices;
    } context = {readable, 0, 100, {}};

    (void)readable.eventError.addListener([this](Result res) { SC_TEST_EXPECT(res); });
    readable.asyncRead = [&context]() -> Result
    {
        if (context.idx < context.max)
        {
            AsyncBufferView::ID bufferID;
            Span<char>          data;
            if (context.readable.getBufferOrPause(sizeof(context.idx), bufferID, data))
            {
                memcpy(data.data(), &context.idx, sizeof(context.idx));
                context.readable.push(bufferID, sizeof(context.idx));
                context.readable.getBuffersPool().unrefBuffer(bufferID);
                context.idx += 1;
                context.readable.reactivate(true);
            }
        }
        else
        {
            context.readable.pushEnd();
        }
        return Result(true);
    };
    // Listen to data events and put all data back into indices array
    (void)readable.eventData.addListener(
        [this, &context](AsyncBufferView::ID bufferID)
        {
            Span<char> data;
            SC_TEST_EXPECT(context.readable.getBuffersPool().getData(bufferID, data));

            if (not data.empty())
            {
                size_t idx = 0;
                memcpy(&idx, data.data(), data.sizeInBytes());
                (void)context.indices.push_back(idx);
            }
        });
    SC_TEST_EXPECT(readable.start());
    SC_TEST_EXPECT(readable.isEnded());
    SC_TEST_EXPECT(context.indices.size() == 100);
    bool valuesAreOk = true;
    for (size_t idx = 0; idx < context.max; ++idx)
    {
        valuesAreOk &= context.indices[idx] == idx;
    }
    SC_TEST_EXPECT(valuesAreOk);
}

void SC::AsyncStreamsTest::readableAsyncStream()
{
    // Create a pool of byte buffers slicing a single HeapBuffer in multiple AsyncBufferView(s)
    constexpr size_t numberOfBuffers = 2;
    constexpr size_t bufferBytesSize = sizeof(size_t);
    AsyncBufferView  buffers[numberOfBuffers];
    HeapBuffer       buffer;
    SC_TEST_EXPECT(buffer.allocate(bufferBytesSize * numberOfBuffers));
    for (size_t idx = 0; idx < numberOfBuffers; ++idx)
    {
        SC_TEST_EXPECT(buffer.data.sliceStartLength(idx * bufferBytesSize, bufferBytesSize, buffers[idx].data));
    }
    AsyncBuffersPool pool;
    pool.buffers = {buffers, numberOfBuffers};

    AsyncReadableStream          readable;
    AsyncReadableStream::Request requests[numberOfBuffers + 1]; // Only N-1 slots will be used
    SC_TEST_EXPECT(readable.init(pool, requests));
    struct Context
    {
        AsyncReadableStream& readable;
        size_t               idx;
        size_t               max;
        Vector<size_t>       indices;
    } context = {readable, 0, 100, {}};

    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());
    AsyncLoopTimeout timeout;
    timeout.cacheInternalEventLoop(loop);
    timeout.callback = [&context](AsyncLoopTimeout::Result&)
    {
        AsyncBufferView::ID bufferID;
        Span<char>          data;
        if (context.readable.getBufferOrPause(sizeof(context.idx), bufferID, data))
        {
            memcpy(data.data(), &context.idx, sizeof(context.idx));
            context.readable.push(bufferID, sizeof(context.idx));
            context.readable.getBuffersPool().unrefBuffer(bufferID);
            context.idx += 1;
            context.readable.reactivate(true);
        }
    };

    readable.asyncRead = [&context, &timeout]() -> Result
    {
        if (context.idx < context.max)
        {
            Result res = timeout.start(*timeout.getEventLoop(), Time::Milliseconds(1));
            if (not res)
            {
                context.readable.emitError(res);
            }
        }
        else
        {
            context.readable.pushEnd();
        }
        return Result(true);
    };

    // Listen to data events and put all data back into indices array
    (void)readable.eventData.addListener(
        [this, &context](AsyncBufferView::ID bufferID)
        {
            Span<char> data;
            SC_TEST_EXPECT(context.readable.getBuffersPool().getData(bufferID, data));

            if (not data.empty())
            {
                size_t idx = 0;
                memcpy(&idx, data.data(), data.sizeInBytes());
                (void)context.indices.push_back(idx);
            }
        });

    SC_TEST_EXPECT(timeout.start(loop, Time::Milliseconds(1)));
    SC_TEST_EXPECT(readable.start());
    SC_TEST_EXPECT(not readable.isEnded());
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(readable.isEnded());

    // Check that indices array contains what we expect
    SC_TEST_EXPECT(context.indices.size() == 100);
    bool valuesAreOk = true;
    for (size_t idx = 0; idx < context.max; ++idx)
    {
        valuesAreOk &= context.indices[idx] == idx;
    }
    SC_TEST_EXPECT(valuesAreOk);
}

namespace SC
{
void runAsyncStreamTest(SC::TestReport& report) { AsyncStreamsTest test(report); }
} // namespace SC
