// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpWebSocket.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"

#include <string.h>

namespace SC
{
struct HttpWebSocketLifecycleTest;
void runHttpWebSocketLifecycleTest(TestReport& report);
} // namespace SC

namespace
{
struct MessageCollector
{
    SC::HttpWebSocketOpcode opcode      = SC::HttpWebSocketOpcode::Text;
    char                    payload[64] = {0};
    size_t                  size        = 0;
    size_t                  count       = 0;

    SC::Result onMessage(SC::HttpWebSocketOpcode messageOpcode, SC::Span<const char> message)
    {
        SC_TRY_MSG(message.sizeInBytes() <= sizeof(payload), "MessageCollector payload too large");
        opcode = messageOpcode;
        size   = message.sizeInBytes();
        count++;
        if (message.sizeInBytes() > 0)
        {
            ::memcpy(payload, message.data(), message.sizeInBytes());
        }
        return SC::Result(true);
    }
};

struct FrameCollector
{
    SC::HttpWebSocketFrameHeaderView header;
    char                             payload[128] = {0};
    size_t                           payloadSize  = 0;
    size_t                           headers      = 0;

    SC::Result onHeader(const SC::HttpWebSocketFrameHeaderView& frameHeader)
    {
        header = frameHeader;
        headers++;
        return SC::Result(true);
    }

    SC::Result onPayload(SC::Span<char> data, bool)
    {
        SC_TRY_MSG(payloadSize + data.sizeInBytes() <= sizeof(payload), "FrameCollector payload too large");
        if (data.sizeInBytes() > 0)
        {
            ::memcpy(payload + payloadSize, data.data(), data.sizeInBytes());
            payloadSize += data.sizeInBytes();
        }
        return SC::Result(true);
    }
};

static bool bytesEqual(SC::Span<const char> lhs, SC::StringSpan rhs)
{
    if (lhs.sizeInBytes() != rhs.sizeInBytes())
    {
        return false;
    }
    return ::memcmp(lhs.data(), rhs.bytesWithoutTerminator(), lhs.sizeInBytes()) == 0;
}
} // namespace

struct SC::HttpWebSocketLifecycleTest : public SC::TestCase
{
    HttpWebSocketLifecycleTest(SC::TestReport& report) : TestCase(report, "HttpWebSocketLifecycleTest")
    {
        if (test_section("message assembler"))
        {
            messageAssembler();
        }
        if (test_section("message assembler limit"))
        {
            messageAssemblerLimit();
        }
        if (test_section("automatic pong"))
        {
            automaticPong();
        }
        if (test_section("close echo"))
        {
            closeEcho();
        }
        if (test_section("pending control backpressure"))
        {
            pendingControlBackpressure();
        }
        if (test_section("send data helper"))
        {
            sendDataHelper();
        }
    }

    void messageAssembler();
    void messageAssemblerLimit();
    void automaticPong();
    void closeEcho();
    void pendingControlBackpressure();
    void sendDataHelper();
};

void SC::HttpWebSocketLifecycleTest::messageAssembler()
{
    HttpWebSocketEndpoint server;
    server.reset(HttpWebSocketEndpointRole::Server);

    char             frameStorage[64] = {0};
    Span<const char> firstFrame;
    Span<const char> secondFrame;
    SC_TEST_EXPECT(
        server.sendData(HttpWebSocketOpcode::Text, "hel"_a8.toCharSpan(), false, nullptr, frameStorage, firstFrame));
    SC_TEST_EXPECT(server.sendData(
        HttpWebSocketOpcode::Continuation, "lo"_a8.toCharSpan(), true, nullptr,
        {frameStorage + firstFrame.sizeInBytes(), sizeof(frameStorage) - firstFrame.sizeInBytes()}, secondFrame));

    char combined[64] = {0};
    ::memcpy(combined, firstFrame.data(), firstFrame.sizeInBytes());
    ::memcpy(combined + firstFrame.sizeInBytes(), secondFrame.data(), secondFrame.sizeInBytes());

    char                          messageStorage[16] = {0};
    HttpWebSocketMessageAssembler assembler;
    MessageCollector              collector;
    assembler.reset(messageStorage);
    assembler.onMessage.bind<MessageCollector, &MessageCollector::onMessage>(collector);

    HttpWebSocketFrameReader reader;
    reader.reset(HttpWebSocketEndpointRole::Client);
    reader.onFrameHeader.bind<HttpWebSocketMessageAssembler, &HttpWebSocketMessageAssembler::onFrameHeader>(assembler);
    reader.onFramePayload.bind<HttpWebSocketMessageAssembler, &HttpWebSocketMessageAssembler::onFramePayload>(
        assembler);

    size_t consumed = 0;
    SC_TEST_EXPECT(reader.parse({combined, firstFrame.sizeInBytes() + secondFrame.sizeInBytes()}, consumed));
    SC_TEST_EXPECT(consumed == firstFrame.sizeInBytes() + secondFrame.sizeInBytes());
    SC_TEST_EXPECT(collector.count == 1);
    SC_TEST_EXPECT(collector.opcode == HttpWebSocketOpcode::Text);
    SC_TEST_EXPECT(bytesEqual({collector.payload, collector.size}, "hello"));
}

void SC::HttpWebSocketLifecycleTest::messageAssemblerLimit()
{
    HttpWebSocketEndpoint server;
    server.reset(HttpWebSocketEndpointRole::Server);

    char             frameStorage[64] = {0};
    Span<const char> encodedFrame;
    SC_TEST_EXPECT(
        server.sendData(HttpWebSocketOpcode::Text, "hello"_a8.toCharSpan(), true, nullptr, frameStorage, encodedFrame));

    char                          messageStorage[4] = {0};
    HttpWebSocketMessageAssembler assembler;
    assembler.reset(messageStorage);

    HttpWebSocketFrameReader reader;
    reader.reset(HttpWebSocketEndpointRole::Client);
    reader.onFrameHeader.bind<HttpWebSocketMessageAssembler, &HttpWebSocketMessageAssembler::onFrameHeader>(assembler);
    reader.onFramePayload.bind<HttpWebSocketMessageAssembler, &HttpWebSocketMessageAssembler::onFramePayload>(
        assembler);

    size_t consumed = 0;
    SC_TEST_EXPECT(not reader.parse({frameStorage, encodedFrame.sizeInBytes()}, consumed));
}

void SC::HttpWebSocketLifecycleTest::automaticPong()
{
    const uint8_t maskKey[4] = {1, 2, 3, 4};

    HttpWebSocketEndpoint client;
    client.reset(HttpWebSocketEndpointRole::Client);
    char             pingStorage[64] = {0};
    Span<const char> pingFrame;
    SC_TEST_EXPECT(client.sendPing("hi"_a8.toCharSpan(), maskKey, pingStorage, pingFrame));

    HttpWebSocketEndpoint server;
    server.reset(HttpWebSocketEndpointRole::Server);

    size_t consumed = 0;
    SC_TEST_EXPECT(server.receive({pingStorage, pingFrame.sizeInBytes()}, consumed));
    SC_TEST_EXPECT(consumed == pingFrame.sizeInBytes());
    SC_TEST_EXPECT(server.hasPendingControlFrame());

    Span<const char> pongFrame;
    SC_TEST_EXPECT(server.getPendingControlFrame(pongFrame));

    FrameCollector           collector;
    HttpWebSocketFrameReader reader;
    reader.reset(HttpWebSocketEndpointRole::Client);
    reader.onFrameHeader.bind<FrameCollector, &FrameCollector::onHeader>(collector);
    reader.onFramePayload.bind<FrameCollector, &FrameCollector::onPayload>(collector);

    size_t pongConsumed = 0;
    SC_TEST_EXPECT(reader.parse({const_cast<char*>(pongFrame.data()), pongFrame.sizeInBytes()}, pongConsumed));
    SC_TEST_EXPECT(collector.headers == 1);
    SC_TEST_EXPECT(collector.header.opcode == HttpWebSocketOpcode::Pong);
    SC_TEST_EXPECT(bytesEqual({collector.payload, collector.payloadSize}, "hi"));
}

void SC::HttpWebSocketLifecycleTest::closeEcho()
{
    const uint8_t maskKey[4] = {9, 8, 7, 6};

    HttpWebSocketEndpoint client;
    client.reset(HttpWebSocketEndpointRole::Client);
    char             closeStorage[64] = {0};
    Span<const char> closeFrame;
    SC_TEST_EXPECT(client.sendClose(1000, "bye"_a8.toCharSpan(), maskKey, closeStorage, closeFrame));

    HttpWebSocketEndpoint server;
    server.reset(HttpWebSocketEndpointRole::Server);

    size_t consumed = 0;
    SC_TEST_EXPECT(server.receive({closeStorage, closeFrame.sizeInBytes()}, consumed));
    SC_TEST_EXPECT(server.hasCloseBeenReceived());
    SC_TEST_EXPECT(server.hasCloseBeenSent());

    Span<const char> echoFrame;
    SC_TEST_EXPECT(server.getPendingControlFrame(echoFrame));

    FrameCollector           collector;
    HttpWebSocketFrameReader reader;
    reader.reset(HttpWebSocketEndpointRole::Client);
    reader.onFrameHeader.bind<FrameCollector, &FrameCollector::onHeader>(collector);
    reader.onFramePayload.bind<FrameCollector, &FrameCollector::onPayload>(collector);

    size_t echoConsumed = 0;
    SC_TEST_EXPECT(reader.parse({const_cast<char*>(echoFrame.data()), echoFrame.sizeInBytes()}, echoConsumed));
    SC_TEST_EXPECT(collector.header.opcode == HttpWebSocketOpcode::Close);
    SC_TEST_EXPECT(collector.payloadSize == 5);
    SC_TEST_EXPECT(static_cast<uint8_t>(collector.payload[0]) == 0x03);
    SC_TEST_EXPECT(static_cast<uint8_t>(collector.payload[1]) == 0xE8);
    SC_TEST_EXPECT(bytesEqual({collector.payload + 2, collector.payloadSize - 2}, "bye"));
}

void SC::HttpWebSocketLifecycleTest::pendingControlBackpressure()
{
    const uint8_t maskKey[4] = {1, 1, 1, 1};

    HttpWebSocketEndpoint client;
    client.reset(HttpWebSocketEndpointRole::Client);

    char             firstStorage[32]  = {0};
    char             secondStorage[32] = {0};
    Span<const char> firstFrame;
    Span<const char> secondFrame;
    SC_TEST_EXPECT(client.sendPing("1"_a8.toCharSpan(), maskKey, firstStorage, firstFrame));
    SC_TEST_EXPECT(client.sendPing("2"_a8.toCharSpan(), maskKey, secondStorage, secondFrame));

    char combined[64] = {0};
    ::memcpy(combined, firstFrame.data(), firstFrame.sizeInBytes());
    ::memcpy(combined + firstFrame.sizeInBytes(), secondFrame.data(), secondFrame.sizeInBytes());

    HttpWebSocketEndpoint server;
    server.reset(HttpWebSocketEndpointRole::Server);

    size_t consumed = 0;
    SC_TEST_EXPECT(not server.receive({combined, firstFrame.sizeInBytes() + secondFrame.sizeInBytes()}, consumed));
    SC_TEST_EXPECT(server.hasPendingControlFrame());
}

void SC::HttpWebSocketLifecycleTest::sendDataHelper()
{
    HttpWebSocketEndpoint server;
    server.reset(HttpWebSocketEndpointRole::Server);

    char             frameStorage[64] = {0};
    Span<const char> encodedFrame;
    SC_TEST_EXPECT(
        server.sendData(HttpWebSocketOpcode::Text, "hello"_a8.toCharSpan(), true, nullptr, frameStorage, encodedFrame));

    FrameCollector           collector;
    HttpWebSocketFrameReader reader;
    reader.reset(HttpWebSocketEndpointRole::Client);
    reader.onFrameHeader.bind<FrameCollector, &FrameCollector::onHeader>(collector);
    reader.onFramePayload.bind<FrameCollector, &FrameCollector::onPayload>(collector);

    size_t consumed = 0;
    SC_TEST_EXPECT(reader.parse({frameStorage, encodedFrame.sizeInBytes()}, consumed));
    SC_TEST_EXPECT(collector.header.opcode == HttpWebSocketOpcode::Text);
    SC_TEST_EXPECT(bytesEqual({collector.payload, collector.payloadSize}, "hello"));
}

void SC::runHttpWebSocketLifecycleTest(SC::TestReport& report) { HttpWebSocketLifecycleTest test(report); }
