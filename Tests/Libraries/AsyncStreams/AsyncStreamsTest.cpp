
// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/AsyncStreams/AsyncStreams.h"
#include "Libraries/Async/Async.h"
#include "Libraries/Containers/Vector.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/Buffer.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Time/Time.h"

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
        if (test_section("writableStream"))
        {
            writableStream();
        }
        if (test_section("createChildView"))
        {
            createChildView();
        }
        if (test_section("unshift"))
        {
            unshift();
        }
    }

    void event();
    void circularQueue();
    void readableSyncStream();
    void readableAsyncStream();
    void writableStream();
    void createChildView();
    void unshift();
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
    SC_TEST_EXPECT(not circularBuffer.pushFront(res));
    SC_TEST_EXPECT(circularBuffer.popFront(res));
    SC_TEST_EXPECT(res == 2);
    res = 4;
    SC_TEST_EXPECT(circularBuffer.pushFront(res));
    res = 0;
    SC_TEST_EXPECT(circularBuffer.popFront(res));
    SC_TEST_EXPECT(res == 4);
    SC_TEST_EXPECT(circularBuffer.popFront(res));
    SC_TEST_EXPECT(res == 3);
    SC_TEST_EXPECT(not circularBuffer.popFront(res));
    SC_TEST_EXPECT(circularBuffer.isEmpty());
}

void SC::AsyncStreamsTest::event()
{
    Event<2, int> event;
    SC_TEST_EXPECT((event.addListener<AsyncStreamsTest, &AsyncStreamsTest::funcCallback>(*this)));
    event.emit(1);
    SC_TEST_EXPECT(memberCalls == 1); // +1
    SC_TEST_EXPECT((event.removeListener<AsyncStreamsTest, &AsyncStreamsTest::funcCallback>(*this)));
    event.emit(1);
    SC_TEST_EXPECT(memberCalls == 1); // +0
    int value = 1;
    SC_TEST_EXPECT((event.addListener<AsyncStreamsTest, &AsyncStreamsTest::funcCallback>(*this)));
    event.emit(value);
    SC_TEST_EXPECT(memberCalls == 2); // +1
    SC_TEST_EXPECT((event.removeAllListenersBoundTo(*this)));
    event.emit(value);
    SC_TEST_EXPECT(memberCalls == 2); // +0
    SC_TEST_EXPECT((event.addListener<AsyncStreamsTest, &AsyncStreamsTest::funcCallback>(*this)));
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
    SC_TEST_EXPECT((event.removeListener<AsyncStreamsTest, &AsyncStreamsTest::funcCallback>(*this)));
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
    Buffer           buffer;
    SC_TEST_EXPECT(buffer.resizeWithoutInitializing(bufferBytesSize * numberOfBuffers));
    for (size_t idx = 0; idx < numberOfBuffers; ++idx)
    {
        Span<char> writableData;
        SC_TEST_EXPECT(buffer.toSpan().sliceStartLength(idx * bufferBytesSize, bufferBytesSize, writableData));
        buffers[idx] = writableData;
        buffers[idx].setReusable(true);
    }
    AsyncBuffersPool pool;
    pool.setBuffers(buffers);

    struct TestReadableStream : public AsyncReadableStream
    {
        size_t         idx = 0;
        size_t         max = 100;
        Vector<size_t> indices;

        virtual Result asyncRead() override
        {

            if (idx < max)
            {
                AsyncBufferView::ID bufferID;
                Span<char>          data;
                if (getBufferOrPause(sizeof(idx), bufferID, data))
                {
                    memcpy(data.data(), &idx, sizeof(idx));
                    SC_TRY(push(bufferID, sizeof(idx)));
                    getBuffersPool().unrefBuffer(bufferID);
                    idx += 1;
                    reactivate(true);
                }
            }
            else
            {
                pushEnd();
            }
            return Result(true);
        }
    };
    TestReadableStream           readable;
    AsyncReadableStream::Request requests[numberOfBuffers + 1]; // Only N-1 slots will be used
    readable.setReadQueue(requests);
    SC_TEST_EXPECT(readable.init(pool));

    (void)readable.eventError.addListener([this](Result res) { SC_TEST_EXPECT(res); });

    // Listen to data events and put all data back into indices array
    (void)readable.eventData.addListener(
        [this, &readable](AsyncBufferView::ID bufferID)
        {
            Span<char> data;
            SC_TEST_EXPECT(readable.getBuffersPool().getWritableData(bufferID, data));

            if (not data.empty())
            {
                size_t idx = 0;
                memcpy(&idx, data.data(), data.sizeInBytes());
                (void)readable.indices.push_back(idx);
            }
        });
    SC_TEST_EXPECT(readable.start());
    SC_TEST_EXPECT(readable.isEnded());
    SC_TEST_EXPECT(readable.indices.size() == 100);
    bool valuesAreOk = true;
    for (size_t idx = 0; idx < readable.max; ++idx)
    {
        valuesAreOk &= readable.indices[idx] == idx;
    }
    SC_TEST_EXPECT(valuesAreOk);
}

void SC::AsyncStreamsTest::readableAsyncStream()
{
    // Create a pool of byte buffers slicing a single Buffer in multiple AsyncBufferView(s)
    constexpr size_t numberOfBuffers = 2;
    constexpr size_t bufferBytesSize = sizeof(size_t);
    AsyncBufferView  buffers[numberOfBuffers];
    Buffer           buffer;
    SC_TEST_EXPECT(buffer.resizeWithoutInitializing(bufferBytesSize * numberOfBuffers));
    for (size_t idx = 0; idx < numberOfBuffers; ++idx)
    {
        Span<char> writableData;
        SC_TEST_EXPECT(buffer.toSpan().sliceStartLength(idx * bufferBytesSize, bufferBytesSize, writableData));
        buffers[idx] = writableData;
        buffers[idx].setReusable(true);
    }
    AsyncBuffersPool pool;
    pool.setBuffers(buffers);

    AsyncEventLoop   loop;
    AsyncLoopTimeout timeout;

    struct TestReadableStream : public AsyncReadableStream
    {
        AsyncEventLoop*   eventLoop;
        AsyncLoopTimeout* timeout;
        size_t            idx = 0;
        size_t            max = 100;
        Vector<size_t>    indices;

        virtual Result asyncRead() override
        {
            if (idx < max)
            {
                Result res = timeout->start(*eventLoop, Time::Milliseconds(1));
                if (not res)
                {
                    emitError(res);
                }
            }
            else
            {
                pushEnd();
            }
            return Result(true);
        }
    };

    TestReadableStream           readable;
    AsyncReadableStream::Request requests[numberOfBuffers + 1]; // Only N-1 slots will be used
    readable.setReadQueue(requests);
    readable.eventLoop = &loop;
    readable.timeout   = &timeout;
    SC_TEST_EXPECT(readable.init(pool));

    SC_TEST_EXPECT(loop.create());
    timeout.callback = [this, &readable](AsyncLoopTimeout::Result&)
    {
        AsyncBufferView::ID bufferID;
        Span<char>          data;
        if (readable.getBufferOrPause(sizeof(readable.idx), bufferID, data))
        {
            memcpy(data.data(), &readable.idx, sizeof(readable.idx));
            SC_TEST_EXPECT(readable.push(bufferID, sizeof(readable.idx)));
            readable.getBuffersPool().unrefBuffer(bufferID);
            readable.idx += 1;
            readable.reactivate(true);
        }
    };

    // Listen to data events and put all data back into indices array
    (void)readable.eventData.addListener(
        [this, &readable](AsyncBufferView::ID bufferID)
        {
            Span<char> data;
            SC_TEST_EXPECT(readable.getBuffersPool().getWritableData(bufferID, data));

            if (not data.empty())
            {
                size_t idx = 0;
                memcpy(&idx, data.data(), data.sizeInBytes());
                (void)readable.indices.push_back(idx);
            }
        });

    SC_TEST_EXPECT(readable.start());
    SC_TEST_EXPECT(not readable.isEnded());
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(readable.isEnded());

    // Check that indices array contains what we expect
    SC_TEST_EXPECT(readable.indices.size() == 100);
    bool valuesAreOk = true;
    for (size_t idx = 0; idx < readable.max; ++idx)
    {
        valuesAreOk &= readable.indices[idx] == idx;
    }
    SC_TEST_EXPECT(valuesAreOk);
}

void SC::AsyncStreamsTest::writableStream()
{
    constexpr size_t numberOfBuffers = 2;
    AsyncBufferView  bufferViews[numberOfBuffers]; // Empty BufferViews (to be filled with ReadOnly ones)
    AsyncBuffersPool pool;
    pool.setBuffers(bufferViews);

    struct TestWritableStream : public AsyncWritableStream
    {
        size_t numAsyncWrites = 0;
        String concatenated;

        AsyncBufferView::ID bufferID;

        virtual Result asyncWrite(AsyncBufferView::ID incomingBufferID, Function<void(AsyncBufferView::ID)> cb) override
        {
            (void)cb;
            numAsyncWrites++;
            Span<const char> data;
            SC_TRY(getBuffersPool().getReadableData(incomingBufferID, data));
            StringView sv(data, false, StringEncoding::Ascii);
            SC_TRY(StringBuilder::createForAppendingTo(concatenated).append(sv));
            bufferID = incomingBufferID;
            return Result(true);
        }
    };
    TestWritableStream           writable;
    AsyncWritableStream::Request writeRequestsQueue[numberOfBuffers + 1]; // Only N-1 slots will be used
    writable.setWriteQueue(writeRequestsQueue);
    SC_TEST_EXPECT(writable.init(pool));
    (void)writable.eventError.addListener([this](Result res) { SC_TEST_EXPECT(res); });

    int numDrain = 0;
    (void)writable.eventDrain.addListener([&numDrain] { numDrain++; });

    // When passing String(...) the writable takes ownership of the String destroying it after the write
    SC_TEST_EXPECT(writable.write(String("1"))); // Executes asyncWrites and queue slot is freed immediately
    SC_TEST_EXPECT(writable.numAsyncWrites == 1);
    SC_TEST_EXPECT(writable.write("2"));         // queued, uses first write slot
    SC_TEST_EXPECT(writable.write(String("3"))); // queued, uses second write slot
    SC_TEST_EXPECT(not writable.write("4"));     // no more write queue slots
    SC_TEST_EXPECT(writable.numAsyncWrites == 1);
    writable.finishedWriting(writable.bufferID, {}, Result(true)); // writes 2
    SC_TEST_EXPECT(writable.concatenated == "12");
    SC_TEST_EXPECT(numDrain == 0);
    SC_TEST_EXPECT(writable.numAsyncWrites == 2);
    SC_TEST_EXPECT(writable.write("4"));
    SC_TEST_EXPECT(writable.numAsyncWrites == 2);
    SC_TEST_EXPECT(not writable.write(String("5")));
    writable.finishedWriting(writable.bufferID, {}, Result(true)); // writes 3
    SC_TEST_EXPECT(writable.concatenated == "123");
    SC_TEST_EXPECT(numDrain == 0);
    writable.finishedWriting(writable.bufferID, {}, Result(true)); // writes 4
    SC_TEST_EXPECT(writable.concatenated == "1234");
    SC_TEST_EXPECT(numDrain == 0);
    writable.finishedWriting(writable.bufferID, {}, Result(true)); // writes nothing
    SC_TEST_EXPECT(writable.concatenated == "1234");
    SC_TEST_EXPECT(numDrain == 1);
    SC_TEST_EXPECT(writable.numAsyncWrites == 4);
    SC_TEST_EXPECT(writable.write("5"));
    SC_TEST_EXPECT(writable.numAsyncWrites == 5);
    SC_TEST_EXPECT(writable.write(String("6")));
    SC_TEST_EXPECT(writable.numAsyncWrites == 5);
    SC_TEST_EXPECT(writable.write("7"));
    SC_TEST_EXPECT(not writable.write(String("8")));
    writable.finishedWriting(writable.bufferID, {}, Result(true));
    SC_TEST_EXPECT(writable.concatenated == "123456");
    SC_TEST_EXPECT(writable.numAsyncWrites == 6);
    SC_TEST_EXPECT(numDrain == 1);
    writable.finishedWriting(writable.bufferID, {}, Result(true));
    SC_TEST_EXPECT(writable.concatenated == "1234567");
    SC_TEST_EXPECT(numDrain == 1);
    SC_TEST_EXPECT(writable.numAsyncWrites == 7);
    writable.finishedWriting(writable.bufferID, {}, Result(true));
    SC_TEST_EXPECT(writable.concatenated == "1234567");
    SC_TEST_EXPECT(numDrain == 2);
    SC_TEST_EXPECT(writable.numAsyncWrites == 7);
    writable.end();
    SC_TEST_EXPECT(writable.concatenated == "1234567");
}

void SC::AsyncStreamsTest::createChildView()
{
    constexpr size_t numberOfBuffers = 4;
    AsyncBufferView  buffers[numberOfBuffers];
    Buffer           buffer;
    SC_TEST_EXPECT(buffer.resizeWithoutInitializing(100));
    // Create parent buffer
    buffers[0] = Span<char>(buffer.toSpan().data(), 100);
    buffers[0].setReusable(true);
    AsyncBuffersPool pool;
    pool.setBuffers(buffers);

    // Get a buffer and fill it with data
    AsyncBufferView::ID parentID;
    Span<char>          parentData;
    SC_TEST_EXPECT(pool.requestNewBuffer(100, parentID, parentData));
    const char* testData = "Hello World! This is a test buffer for child views.";
    memcpy(parentData.data(), testData, strlen(testData));

    // Create child view
    AsyncBufferView::ID childID;
    SC_TEST_EXPECT(pool.createChildView(parentID, 6, 11, childID)); // "World"

    // Check child data
    Span<const char> childData;
    SC_TEST_EXPECT(pool.getReadableData(childID, childData));
    SC_TEST_EXPECT(childData.sizeInBytes() == 11);
    SC_TEST_EXPECT(memcmp(childData.data(), "World! This ", 11) == 0);

    // Verify writable data on child (since parent is writable)
    Span<char> childWritableData;
    SC_TEST_EXPECT(pool.getWritableData(childID, childWritableData));
    SC_TEST_EXPECT(childWritableData.sizeInBytes() == 11);
    childWritableData[0] = 'W'; // "World"

    // Create grandchild view (child of child)
    AsyncBufferView::ID grandchildID;
    SC_TEST_EXPECT(
        pool.createChildView(childID, 7, 4, grandchildID)); // "This" (relative to child: 7+6=13 relative to parent)

    Span<const char> grandchildData;
    SC_TEST_EXPECT(pool.getReadableData(grandchildID, grandchildData));
    SC_TEST_EXPECT(grandchildData.sizeInBytes() == 4);
    SC_TEST_EXPECT(memcmp(grandchildData.data(), "This", 4) == 0);

    // Test error cases
    AsyncBufferView::ID invalidID;
    SC_TEST_EXPECT(not pool.createChildView(AsyncBufferView::ID(999), 0, 10, invalidID)); // Invalid parent
    SC_TEST_EXPECT(not pool.createChildView(parentID, 90, 20, invalidID));                // Out of bounds

    // Verify resizing child view
    pool.setNewBufferSize(childID, 5); // Resize to 5 ("World")
    SC_TEST_EXPECT(pool.getReadableData(childID, childData));
    SC_TEST_EXPECT(childData.sizeInBytes() == 5);
    SC_TEST_EXPECT(memcmp(childData.data(), "World", 5) == 0);

    pool.setNewBufferSize(childID, 10); // Try to expand back (should be ignored)
    SC_TEST_EXPECT(pool.getReadableData(childID, childData));
    SC_TEST_EXPECT(childData.sizeInBytes() == 5); // Still 5

    // Test refcount: when we unref child, parent should still be ref'd
    // Initially parent has 3 refs (1 from request + 1 from child + 1 from grandchild)
    pool.unrefBuffer(childID); // Child deleted, parent refs = 2
    SC_TEST_EXPECT(pool.getBuffer(childID) == nullptr);

    Span<const char> stillValid;
    SC_TEST_EXPECT(pool.getReadableData(parentID, stillValid)); // Parent still accessible

    pool.unrefBuffer(grandchildID); // Grandchild deleted, parent refs = 1
    SC_TEST_EXPECT(pool.getBuffer(grandchildID) == nullptr);

    pool.unrefBuffer(parentID); // Now unref parent, parent refs = 0
    // parentID is NOT nullptr because it was marked as reusable!
    SC_TEST_EXPECT(pool.getBuffer(parentID) != nullptr);
}

void SC::AsyncStreamsTest::unshift()
{
    AsyncBufferView::ID bufferID;
    Span<char>          data;
    AsyncBufferView     buffers[1];
    Buffer              buffer;
    SC_TEST_EXPECT(buffer.resizeWithoutInitializing(123));
    buffers[0] = buffer.toSpan();
    buffers[0].setReusable(true);

    AsyncBuffersPool pool;
    pool.setBuffers(buffers);

    struct TestReadableStream : public AsyncReadableStream
    {
        int  step    = 0;
        bool success = true;

        virtual Result asyncRead() override
        {
            step++;
            // We do nothing here, just waiting for push
            return Result(true);
        }
    };
    TestReadableStream           readable;
    AsyncReadableStream::Request requests[3];
    readable.setReadQueue(requests); // Capacity 2
    SC_TEST_EXPECT(readable.init(pool));

    SC_TEST_EXPECT(readable.getBuffersPool().requestNewBuffer(123, bufferID, data));

    // 1. Manually unshift a buffer
    char content[] = "123";
    memcpy(data.data(), content, 3);
    readable.getBuffersPool().setNewBufferSize(bufferID, 3);
    SC_TRUST_RESULT(readable.unshift(bufferID));
    // Release our reference so that the stream is the only owner and can recycle it after emission
    pool.unrefBuffer(bufferID);

    // 2. Start reading, it should immediately receive the unshifted buffer
    SC_TEST_EXPECT(readable.eventData.addListener(
        [&readable](AsyncBufferView::ID id)
        {
            Span<const char> readData;
            SC_TRUST_RESULT(readable.getBuffersPool().getReadableData(id, readData));
            StringView str = StringView(readData, false, StringEncoding::Ascii);
            if (str != "123")
                readable.success = false;
            // Should be received before asyncRead is even called or right at start
            if (readable.step != 0)
                readable.success = false;
        }));

    SC_TRUST_RESULT(readable.start());
    SC_TEST_EXPECT(readable.success);

    // Cleanup to allow re-use of buffer for next check
    // In this test we only have 1 buffer so we rely on unref happening in emitOnData (which calls unrefBuffer)
    // But readableStream.emitOnData calls `buffers->unrefBuffer(request.bufferID)`

    // We need to check if we can push again.
    // Since we consumed the buffer in the listener (implied by just receiving it, we don't unref it ourselves if we
    // don't hold it, but wait, readable stream unrefs it when emitting). So the buffer should indeed be free now if
    // refs went to 0.

    AsyncBufferView::ID bufferID2;
    Span<char>          data2;
    // Verify we can still push normally after unshift
    SC_TRUST_RESULT(readable.getBuffersPool().requestNewBuffer(123, bufferID2, data2));
    SC_TEST_EXPECT(readable.push(bufferID2, 10));
}

namespace SC
{
void runAsyncStreamTest(SC::TestReport& report) { AsyncStreamsTest test(report); }
} // namespace SC
