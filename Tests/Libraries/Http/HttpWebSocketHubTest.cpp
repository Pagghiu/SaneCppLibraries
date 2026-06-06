// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpWebSocket.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"

#include <string.h>

namespace SC
{
struct HttpWebSocketHubTest;
void runHttpWebSocketHubTest(TestReport& report);
} // namespace SC

namespace
{
struct DummyReadableStream : public SC::AsyncReadableStream
{
    Request queue[1];

    DummyReadableStream() { setReadQueue(queue); }

  private:
    virtual SC::Result asyncRead() override { return SC::Result(true); }
};

struct DummyWritableStream : public SC::AsyncWritableStream
{
    Request queue[1];

    DummyWritableStream() { setWriteQueue(queue); }

  private:
    virtual SC::Result asyncWrite(SC::AsyncBufferView::ID, SC::Function<void(SC::AsyncBufferView::ID)>) override
    {
        return SC::Result(true);
    }
};

struct BroadcastCollector
{
    size_t calls      = 0;
    size_t lastIndex  = 0;
    char   frame[128] = {0};
    size_t frameSize  = 0;

    void reset()
    {
        calls     = 0;
        lastIndex = 0;
        frameSize = 0;
    }

    SC::Result onFrame(size_t clientIndex, SC::Span<const char> encodedFrame)
    {
        SC_TRY_MSG(encodedFrame.sizeInBytes() <= sizeof(frame), "BroadcastCollector frame too large");
        calls++;
        lastIndex = clientIndex;
        frameSize = encodedFrame.sizeInBytes();
        ::memcpy(frame, encodedFrame.data(), encodedFrame.sizeInBytes());
        return SC::Result(true);
    }
};

struct PayloadCollector
{
    SC::HttpWebSocketFrameHeaderView header;
    char                             payload[64] = {0};
    size_t                           payloadSize = 0;

    SC::Result onHeader(const SC::HttpWebSocketFrameHeaderView& frameHeader)
    {
        header = frameHeader;
        return SC::Result(true);
    }

    SC::Result onPayload(SC::Span<char> data, bool)
    {
        SC_TRY_MSG(payloadSize + data.sizeInBytes() <= sizeof(payload), "PayloadCollector payload too large");
        ::memcpy(payload + payloadSize, data.data(), data.sizeInBytes());
        payloadSize += data.sizeInBytes();
        return SC::Result(true);
    }
};

static bool resultMessageEquals(SC::Result result, SC::StringSpan expected)
{
    return not result and SC::StringSpan::fromNullTerminated(result.message, SC::StringEncoding::Ascii) == expected;
}
} // namespace

struct SC::HttpWebSocketHubTest : public SC::TestCase
{
    HttpWebSocketHubTest(SC::TestReport& report) : TestCase(report, "HttpWebSocketHubTest")
    {
        if (test_section("fixed slot join leave and broadcast"))
        {
            fixedSlotJoinLeaveAndBroadcast();
        }
        if (test_section("capacity limit"))
        {
            capacityLimit();
        }
        if (test_section("diagnostic messages"))
        {
            diagnosticMessages();
        }
    }

    void fixedSlotJoinLeaveAndBroadcast();
    void capacityLimit();
    void diagnosticMessages();
};

void SC::HttpWebSocketHubTest::fixedSlotJoinLeaveAndBroadcast()
{
    HttpWebSocketHubClient slots[2];
    HttpWebSocketSmallHub  hub;
    SC_TEST_EXPECT(hub.init(slots));

    DummyReadableStream readable[2];
    DummyWritableStream writable[2];
    AsyncBuffersPool    buffersPool[2];

    HttpWebSocketTransportView transports[2];
    for (size_t idx = 0; idx < 2; ++idx)
    {
        transports[idx].readableStream = &readable[idx];
        transports[idx].writableStream = &writable[idx];
        transports[idx].buffersPool    = &buffersPool[idx];
    }

    size_t firstIndex  = 0;
    size_t secondIndex = 0;
    SC_TEST_EXPECT(hub.join(transports[0], firstIndex));
    SC_TEST_EXPECT(hub.join(transports[1], secondIndex));
    SC_TEST_EXPECT(firstIndex == 0);
    SC_TEST_EXPECT(secondIndex == 1);
    SC_TEST_EXPECT(hub.getNumClients() == 2);

    BroadcastCollector collector;
    hub.onBroadcastFrame.bind<BroadcastCollector, &BroadcastCollector::onFrame>(collector);

    char frameStorage[64] = {0};
    SC_TEST_EXPECT(hub.broadcastText("hello"_a8.toCharSpan(), frameStorage));
    SC_TEST_EXPECT(collector.calls == 2);
    SC_TEST_EXPECT(collector.lastIndex == secondIndex);

    PayloadCollector         payloadCollector;
    HttpWebSocketFrameReader reader;
    reader.reset(HttpWebSocketEndpointRole::Client);
    reader.onFrameHeader.bind<PayloadCollector, &PayloadCollector::onHeader>(payloadCollector);
    reader.onFramePayload.bind<PayloadCollector, &PayloadCollector::onPayload>(payloadCollector);

    size_t consumed = 0;
    SC_TEST_EXPECT(reader.parse({collector.frame, collector.frameSize}, consumed));
    SC_TEST_EXPECT(payloadCollector.header.opcode == HttpWebSocketOpcode::Text);
    SC_TEST_EXPECT(StringSpan({payloadCollector.payload, payloadCollector.payloadSize}, false, StringEncoding::Ascii) ==
                   "hello");

    SC_TEST_EXPECT(hub.leave(firstIndex));
    collector.reset();
    SC_TEST_EXPECT(hub.broadcastText("again"_a8.toCharSpan(), frameStorage));
    SC_TEST_EXPECT(collector.calls == 1);
    SC_TEST_EXPECT(collector.lastIndex == secondIndex);
    SC_TEST_EXPECT(hub.getNumClients() == 1);
}

void SC::HttpWebSocketHubTest::capacityLimit()
{
    HttpWebSocketHubClient slots[1];
    HttpWebSocketSmallHub  hub;
    SC_TEST_EXPECT(hub.init(slots));

    DummyReadableStream readable;
    DummyWritableStream writable;
    AsyncBuffersPool    buffersPool;

    HttpWebSocketTransportView transport;
    transport.readableStream = &readable;
    transport.writableStream = &writable;
    transport.buffersPool    = &buffersPool;

    size_t index = 0;
    SC_TEST_EXPECT(hub.join(transport, index));
    SC_TEST_EXPECT(not hub.join(transport, index));
}

void SC::HttpWebSocketHubTest::diagnosticMessages()
{
    HttpWebSocketHubClient slots[1];
    HttpWebSocketSmallHub  hub;
    SC_TEST_EXPECT(hub.init(slots));

    HttpWebSocketTransportView invalidTransport;
    size_t                     index = 0;
    SC_TEST_EXPECT(
        resultMessageEquals(hub.join(invalidTransport, index), "HttpWebSocketSmallHub transport is invalid"));

    DummyReadableStream readable;
    DummyWritableStream writable;
    AsyncBuffersPool    buffersPool;

    HttpWebSocketTransportView transport;
    transport.readableStream = &readable;
    transport.writableStream = &writable;
    transport.buffersPool    = &buffersPool;

    SC_TEST_EXPECT(hub.join(transport, index));
    SC_TEST_EXPECT(resultMessageEquals(hub.join(transport, index), "HttpWebSocketSmallHub is full"));
    SC_TEST_EXPECT(
        resultMessageEquals(hub.broadcastFrame({}), "HttpWebSocketSmallHub cannot broadcast an empty frame"));
    SC_TEST_EXPECT(resultMessageEquals(hub.leave(2), "HttpWebSocketSmallHub client index out of range"));
}

void SC::runHttpWebSocketHubTest(SC::TestReport& report) { HttpWebSocketHubTest test(report); }
