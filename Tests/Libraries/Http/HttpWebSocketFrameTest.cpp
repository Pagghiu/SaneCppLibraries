// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpStringAppend.h"
#include "Libraries/Http/HttpWebSocket.h"
#include "Libraries/Memory/Buffer.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct HttpWebSocketFrameTest;
void runHttpWebSocketFrameTest(TestReport& report);
} // namespace SC

namespace
{
static SC::Span<char> byteSpan(SC::uint8_t* bytes, SC::size_t size)
{
    return SC::Span<char>::reinterpret_bytes(bytes, size);
}

static SC::Span<const char> byteSpan(const SC::uint8_t* bytes, SC::size_t size)
{
    return SC::Span<const char>::reinterpret_bytes(bytes, size);
}

struct ReaderCollector
{
    SC::HttpWebSocketFrameHeaderView headers[16];

    SC::Buffer payload;
    bool       finishedFlags[32] = {false};
    size_t     numHeaders        = 0;
    size_t     numPayloadEvents  = 0;

    SC::Result append(SC::Span<char> data)
    {
        SC::GrowableBuffer<SC::Buffer> gb(payload);
        SC::HttpStringAppend&          sb = static_cast<SC::HttpStringAppend&>(static_cast<SC::IGrowableBuffer&>(gb));
        return SC::Result(sb.append(data, 0));
    }

    SC::Result onHeader(const SC::HttpWebSocketFrameHeaderView& header)
    {
        SC_TRY_MSG(numHeaders < sizeof(headers) / sizeof(headers[0]), "ReaderCollector too many headers");
        headers[numHeaders++] = header;
        return SC::Result(true);
    }

    SC::Result onPayload(SC::Span<char> data, bool finished)
    {
        SC_TRY_MSG(numPayloadEvents < sizeof(finishedFlags) / sizeof(finishedFlags[0]),
                   "ReaderCollector too many payload callbacks");
        SC_TRY(append(data));
        finishedFlags[numPayloadEvents++] = finished;
        return SC::Result(true);
    }

    [[nodiscard]] SC::StringSpan view() const { return {payload.toSpanConst(), false, SC::StringEncoding::Ascii}; }
};

static SC::Result encodeFrame(SC::HttpWebSocketFrameWriter& writer, const SC::HttpWebSocketFrameHeaderView& header,
                              SC::Span<char> payload, SC::Span<char> outputStorage, size_t& outputBytes)
{
    SC::Span<const char> encodedHeader;
    SC_TRY(writer.beginFrame(header, outputStorage, encodedHeader));
    SC_TRY_MSG(outputStorage.sizeInBytes() >= encodedHeader.sizeInBytes() + payload.sizeInBytes(),
               "encodeFrame output storage too small");

    if (payload.sizeInBytes() > 0)
    {
        SC::Span<char> destinationPayload = {outputStorage.data() + encodedHeader.sizeInBytes(), payload.sizeInBytes()};
        for (size_t idx = 0; idx < payload.sizeInBytes(); ++idx)
        {
            destinationPayload[idx] = payload[idx];
        }
        SC_TRY(writer.writePayload(destinationPayload));
    }

    SC_TRY(writer.finishFrame());
    outputBytes = encodedHeader.sizeInBytes() + payload.sizeInBytes();
    return SC::Result(true);
}

static SC::Result parseFrameInChunks(SC::HttpWebSocketFrameReader& reader, SC::Span<char> encoded,
                                     const size_t* chunkSizes, size_t numChunkSizes)
{
    size_t offset = 0;
    for (size_t idx = 0; idx < numChunkSizes; ++idx)
    {
        const size_t remaining = encoded.sizeInBytes() - offset;
        const size_t chunk     = chunkSizes[idx] < remaining ? chunkSizes[idx] : remaining;
        if (chunk == 0)
        {
            continue;
        }
        size_t consumed = 0;
        SC_TRY(reader.parse({encoded.data() + offset, chunk}, consumed));
        SC_TRY_MSG(consumed == chunk, "parseFrameInChunks did not consume the full chunk");
        offset += chunk;
    }
    if (offset < encoded.sizeInBytes())
    {
        size_t consumed = 0;
        SC_TRY(reader.parse({encoded.data() + offset, encoded.sizeInBytes() - offset}, consumed));
        SC_TRY_MSG(consumed == encoded.sizeInBytes() - offset, "parseFrameInChunks did not consume the trailing bytes");
        offset += consumed;
    }
    SC_TRY_MSG(offset == encoded.sizeInBytes(), "parseFrameInChunks did not process all bytes");
    return SC::Result(true);
}

static bool spansEqual(SC::Span<const char> lhs, SC::Span<const char> rhs)
{
    if (lhs.sizeInBytes() != rhs.sizeInBytes())
    {
        return false;
    }
    for (size_t idx = 0; idx < lhs.sizeInBytes(); ++idx)
    {
        if (lhs[idx] != rhs[idx])
        {
            return false;
        }
    }
    return true;
}

static bool resultMessageEquals(SC::Result result, SC::StringSpan expected)
{
    if (result or result.message == nullptr)
    {
        return false;
    }

    return SC::StringSpan::fromNullTerminated(result.message, SC::StringEncoding::Ascii) == expected;
}
} // namespace

struct SC::HttpWebSocketFrameTest : public SC::TestCase
{
    HttpWebSocketFrameTest(SC::TestReport& report) : TestCase(report, "HttpWebSocketFrameTest")
    {
        if (test_section("unmasked server text and binary frames"))
        {
            unmaskedServerFrames();
        }
        if (test_section("masked client text and binary frames"))
        {
            maskedClientFrames();
        }
        if (test_section("extended payload lengths"))
        {
            extendedPayloadLengths();
        }
        if (test_section("fragmentation sequences"))
        {
            fragmentationSequences();
        }
        if (test_section("control frames"))
        {
            controlFrames();
        }
        if (test_section("masking role validation"))
        {
            maskingRoleValidation();
        }
        if (test_section("invalid frame rejection"))
        {
            invalidFrameRejection();
        }
        if (test_section("writer roundtrip"))
        {
            writerRoundtrip();
        }
    }

    void unmaskedServerFrames();
    void maskedClientFrames();
    void extendedPayloadLengths();
    void fragmentationSequences();
    void controlFrames();
    void maskingRoleValidation();
    void invalidFrameRejection();
    void writerRoundtrip();
};

void SC::HttpWebSocketFrameTest::unmaskedServerFrames()
{
    HttpWebSocketFrameWriter writer;
    writer.reset(HttpWebSocketEndpointRole::Server);

    HttpWebSocketFrameHeaderView textHeader;
    textHeader.fin           = true;
    textHeader.opcode        = HttpWebSocketOpcode::Text;
    textHeader.masked        = false;
    textHeader.payloadLength = 5;

    HttpWebSocketFrameHeaderView binaryHeader;
    binaryHeader.fin           = true;
    binaryHeader.opcode        = HttpWebSocketOpcode::Binary;
    binaryHeader.masked        = false;
    binaryHeader.payloadLength = 3;

    char   output[64] = {0};
    size_t totalBytes = 0;
    size_t frameBytes = 0;

    char textPayload[]   = {'h', 'e', 'l', 'l', 'o'};
    char binaryPayload[] = {char(0x01), char(0x00), char(0x7F)};

    SC_TEST_EXPECT(
        encodeFrame(writer, textHeader, {textPayload, sizeof(textPayload)}, {output, sizeof(output)}, frameBytes));
    totalBytes += frameBytes;
    SC_TEST_EXPECT(encodeFrame(writer, binaryHeader, {binaryPayload, sizeof(binaryPayload)},
                               {output + totalBytes, sizeof(output) - totalBytes}, frameBytes));
    totalBytes += frameBytes;

    ReaderCollector          collector;
    HttpWebSocketFrameReader reader;
    reader.reset(HttpWebSocketEndpointRole::Client);
    reader.onFrameHeader.bind<ReaderCollector, &ReaderCollector::onHeader>(collector);
    reader.onFramePayload.bind<ReaderCollector, &ReaderCollector::onPayload>(collector);

    size_t consumed = 0;
    SC_TEST_EXPECT(reader.parse({output, totalBytes}, consumed));
    SC_TEST_EXPECT(consumed == totalBytes);
    SC_TEST_EXPECT(collector.numHeaders == 2);
    SC_TEST_EXPECT(collector.headers[0].opcode == HttpWebSocketOpcode::Text);
    SC_TEST_EXPECT(collector.headers[1].opcode == HttpWebSocketOpcode::Binary);
    SC_TEST_EXPECT(collector.headers[0].payloadLength == 5);
    SC_TEST_EXPECT(collector.headers[1].payloadLength == 3);

    const char expected[] = {'h', 'e', 'l', 'l', 'o', char(0x01), char(0x00), char(0x7F)};
    SC_TEST_EXPECT(spansEqual(collector.payload.toSpanConst(), {expected, sizeof(expected)}));
}

void SC::HttpWebSocketFrameTest::maskedClientFrames()
{
    HttpWebSocketFrameWriter writer;
    writer.reset(HttpWebSocketEndpointRole::Client);

    HttpWebSocketFrameHeaderView textHeader;
    textHeader.fin           = true;
    textHeader.opcode        = HttpWebSocketOpcode::Text;
    textHeader.masked        = true;
    textHeader.payloadLength = 5;
    textHeader.maskKey[0]    = 0x37;
    textHeader.maskKey[1]    = 0xFA;
    textHeader.maskKey[2]    = 0x21;
    textHeader.maskKey[3]    = 0x3D;

    HttpWebSocketFrameHeaderView binaryHeader;
    binaryHeader.fin           = true;
    binaryHeader.opcode        = HttpWebSocketOpcode::Binary;
    binaryHeader.masked        = true;
    binaryHeader.payloadLength = 4;
    binaryHeader.maskKey[0]    = 0x10;
    binaryHeader.maskKey[1]    = 0x20;
    binaryHeader.maskKey[2]    = 0x30;
    binaryHeader.maskKey[3]    = 0x40;

    char   output[64] = {0};
    size_t totalBytes = 0;
    size_t frameBytes = 0;

    char        textPayload[]   = {'h', 'e', 'l', 'l', 'o'};
    SC::uint8_t binaryPayload[] = {0xDE, 0xAD, 0xBE, 0xEF};

    SC_TEST_EXPECT(
        encodeFrame(writer, textHeader, {textPayload, sizeof(textPayload)}, {output, sizeof(output)}, frameBytes));
    totalBytes += frameBytes;
    SC_TEST_EXPECT(encodeFrame(writer, binaryHeader, byteSpan(binaryPayload, sizeof(binaryPayload)),
                               {output + totalBytes, sizeof(output) - totalBytes}, frameBytes));
    totalBytes += frameBytes;

    ReaderCollector          collector;
    HttpWebSocketFrameReader reader;
    reader.reset(HttpWebSocketEndpointRole::Server);
    reader.onFrameHeader.bind<ReaderCollector, &ReaderCollector::onHeader>(collector);
    reader.onFramePayload.bind<ReaderCollector, &ReaderCollector::onPayload>(collector);

    const size_t chunks[] = {1, 2, 3, 4, 5, 6, 7};
    SC_TEST_EXPECT(parseFrameInChunks(reader, {output, totalBytes}, chunks, sizeof(chunks) / sizeof(chunks[0])));
    SC_TEST_EXPECT(collector.numHeaders == 2);
    SC_TEST_EXPECT(collector.headers[0].masked);
    SC_TEST_EXPECT(collector.headers[1].masked);

    const SC::uint8_t expected[] = {'h', 'e', 'l', 'l', 'o', 0xDE, 0xAD, 0xBE, 0xEF};
    SC_TEST_EXPECT(spansEqual(collector.payload.toSpanConst(), byteSpan(expected, sizeof(expected))));
}

void SC::HttpWebSocketFrameTest::extendedPayloadLengths()
{
    HttpWebSocketFrameWriter writer;
    writer.reset(HttpWebSocketEndpointRole::Server);

    static char payload16[130];
    for (size_t idx = 0; idx < sizeof(payload16); ++idx)
    {
        payload16[idx] = static_cast<char>('a' + (idx % 26));
    }

    HttpWebSocketFrameHeaderView header16;
    header16.fin           = true;
    header16.opcode        = HttpWebSocketOpcode::Binary;
    header16.masked        = false;
    header16.payloadLength = sizeof(payload16);

    static char encoded16[160];
    size_t      encoded16Size = 0;
    SC_TEST_EXPECT(
        encodeFrame(writer, header16, {payload16, sizeof(payload16)}, {encoded16, sizeof(encoded16)}, encoded16Size));
    SC_TEST_EXPECT(static_cast<uint8_t>(encoded16[1]) == 126);

    ReaderCollector          collector16;
    HttpWebSocketFrameReader reader16;
    reader16.reset(HttpWebSocketEndpointRole::Client);
    reader16.onFrameHeader.bind<ReaderCollector, &ReaderCollector::onHeader>(collector16);
    reader16.onFramePayload.bind<ReaderCollector, &ReaderCollector::onPayload>(collector16);

    size_t consumed = 0;
    SC_TEST_EXPECT(reader16.parse({encoded16, encoded16Size}, consumed));
    SC_TEST_EXPECT(consumed == encoded16Size);
    SC_TEST_EXPECT(collector16.numHeaders == 1);
    SC_TEST_EXPECT(collector16.headers[0].payloadLength == sizeof(payload16));
    SC_TEST_EXPECT(spansEqual(collector16.payload.toSpanConst(), {payload16, sizeof(payload16)}));

    writer.reset(HttpWebSocketEndpointRole::Server);

    static char payload64[66000];
    for (size_t idx = 0; idx < sizeof(payload64); ++idx)
    {
        payload64[idx] = static_cast<char>(idx & 0x7F);
    }

    HttpWebSocketFrameHeaderView header64;
    header64.fin           = true;
    header64.opcode        = HttpWebSocketOpcode::Binary;
    header64.masked        = false;
    header64.payloadLength = sizeof(payload64);

    static char encoded64[66032];
    size_t      encoded64Size = 0;
    SC_TEST_EXPECT(
        encodeFrame(writer, header64, {payload64, sizeof(payload64)}, {encoded64, sizeof(encoded64)}, encoded64Size));
    SC_TEST_EXPECT(static_cast<uint8_t>(encoded64[1]) == 127);

    ReaderCollector          collector64;
    HttpWebSocketFrameReader reader64;
    reader64.reset(HttpWebSocketEndpointRole::Client);
    reader64.onFrameHeader.bind<ReaderCollector, &ReaderCollector::onHeader>(collector64);
    reader64.onFramePayload.bind<ReaderCollector, &ReaderCollector::onPayload>(collector64);

    const size_t chunks[] = {2, 8, 4096, 1024, 2048, 8192, 16384, 32768};
    SC_TEST_EXPECT(
        parseFrameInChunks(reader64, {encoded64, encoded64Size}, chunks, sizeof(chunks) / sizeof(chunks[0])));
    SC_TEST_EXPECT(collector64.numHeaders == 1);
    SC_TEST_EXPECT(collector64.headers[0].payloadLength == sizeof(payload64));
    SC_TEST_EXPECT(spansEqual(collector64.payload.toSpanConst(), {payload64, sizeof(payload64)}));
}

void SC::HttpWebSocketFrameTest::fragmentationSequences()
{
    HttpWebSocketFrameWriter writer;
    writer.reset(HttpWebSocketEndpointRole::Server);

    char output[64] = {0};

    HttpWebSocketFrameHeaderView first;
    first.fin           = false;
    first.opcode        = HttpWebSocketOpcode::Text;
    first.masked        = false;
    first.payloadLength = 3;

    HttpWebSocketFrameHeaderView second;
    second.fin           = false;
    second.opcode        = HttpWebSocketOpcode::Continuation;
    second.masked        = false;
    second.payloadLength = 2;

    HttpWebSocketFrameHeaderView third;
    third.fin           = true;
    third.opcode        = HttpWebSocketOpcode::Continuation;
    third.masked        = false;
    third.payloadLength = 5;

    size_t frameBytes = 0;
    size_t totalBytes = 0;

    char payload1[] = {'h', 'e', 'l'};
    char payload2[] = {'l', 'o'};
    char payload3[] = {' ', 'w', 'o', 'r', 'l'};

    SC_TEST_EXPECT(encodeFrame(writer, first, {payload1, sizeof(payload1)}, {output, sizeof(output)}, frameBytes));
    totalBytes += frameBytes;
    SC_TEST_EXPECT(encodeFrame(writer, second, {payload2, sizeof(payload2)},
                               {output + totalBytes, sizeof(output) - totalBytes}, frameBytes));
    totalBytes += frameBytes;
    SC_TEST_EXPECT(encodeFrame(writer, third, {payload3, sizeof(payload3)},
                               {output + totalBytes, sizeof(output) - totalBytes}, frameBytes));
    totalBytes += frameBytes;

    ReaderCollector          collector;
    HttpWebSocketFrameReader reader;
    reader.reset(HttpWebSocketEndpointRole::Client);
    reader.onFrameHeader.bind<ReaderCollector, &ReaderCollector::onHeader>(collector);
    reader.onFramePayload.bind<ReaderCollector, &ReaderCollector::onPayload>(collector);

    size_t consumed = 0;
    SC_TEST_EXPECT(reader.parse({output, totalBytes}, consumed));
    SC_TEST_EXPECT(consumed == totalBytes);
    SC_TEST_EXPECT(collector.numHeaders == 3);
    SC_TEST_EXPECT(collector.headers[0].opcode == HttpWebSocketOpcode::Text);
    SC_TEST_EXPECT(collector.headers[1].opcode == HttpWebSocketOpcode::Continuation);
    SC_TEST_EXPECT(collector.headers[2].opcode == HttpWebSocketOpcode::Continuation);
    SC_TEST_EXPECT(collector.finishedFlags[0]);
    SC_TEST_EXPECT(collector.finishedFlags[1]);
    SC_TEST_EXPECT(collector.finishedFlags[2]);

    const char expected[] = {'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l'};
    SC_TEST_EXPECT(spansEqual(collector.payload.toSpanConst(), {expected, sizeof(expected)}));
}

void SC::HttpWebSocketFrameTest::controlFrames()
{
    HttpWebSocketFrameWriter writer;
    writer.reset(HttpWebSocketEndpointRole::Server);

    char output[64] = {0};

    HttpWebSocketFrameHeaderView ping;
    ping.fin           = true;
    ping.opcode        = HttpWebSocketOpcode::Ping;
    ping.masked        = false;
    ping.payloadLength = 4;

    HttpWebSocketFrameHeaderView close;
    close.fin           = true;
    close.opcode        = HttpWebSocketOpcode::Close;
    close.masked        = false;
    close.payloadLength = 2;

    size_t frameBytes = 0;
    size_t totalBytes = 0;

    char        pingPayload[]  = {'p', 'i', 'n', 'g'};
    SC::uint8_t closePayload[] = {0x03, 0xE8};

    SC_TEST_EXPECT(encodeFrame(writer, ping, {pingPayload, sizeof(pingPayload)}, {output, sizeof(output)}, frameBytes));
    totalBytes += frameBytes;
    SC_TEST_EXPECT(encodeFrame(writer, close, byteSpan(closePayload, sizeof(closePayload)),
                               {output + totalBytes, sizeof(output) - totalBytes}, frameBytes));
    totalBytes += frameBytes;

    ReaderCollector          collector;
    HttpWebSocketFrameReader reader;
    reader.reset(HttpWebSocketEndpointRole::Client);
    reader.onFrameHeader.bind<ReaderCollector, &ReaderCollector::onHeader>(collector);
    reader.onFramePayload.bind<ReaderCollector, &ReaderCollector::onPayload>(collector);

    size_t consumed = 0;
    SC_TEST_EXPECT(reader.parse({output, totalBytes}, consumed));
    SC_TEST_EXPECT(consumed == totalBytes);
    SC_TEST_EXPECT(collector.numHeaders == 2);
    SC_TEST_EXPECT(collector.headers[0].isControlFrame());
    SC_TEST_EXPECT(collector.headers[1].isControlFrame());
    SC_TEST_EXPECT(collector.headers[0].opcode == HttpWebSocketOpcode::Ping);
    SC_TEST_EXPECT(collector.headers[1].opcode == HttpWebSocketOpcode::Close);

    const SC::uint8_t expected[] = {'p', 'i', 'n', 'g', 0x03, 0xE8};
    SC_TEST_EXPECT(spansEqual(collector.payload.toSpanConst(), byteSpan(expected, sizeof(expected))));
}

void SC::HttpWebSocketFrameTest::maskingRoleValidation()
{
    HttpWebSocketFrameWriter writer;
    writer.reset(HttpWebSocketEndpointRole::Client);

    HttpWebSocketFrameHeaderView maskedHeader;
    maskedHeader.fin           = true;
    maskedHeader.opcode        = HttpWebSocketOpcode::Text;
    maskedHeader.masked        = true;
    maskedHeader.payloadLength = 2;
    maskedHeader.maskKey[0]    = 1;
    maskedHeader.maskKey[1]    = 2;
    maskedHeader.maskKey[2]    = 3;
    maskedHeader.maskKey[3]    = 4;

    char   maskedOutput[32] = {0};
    size_t maskedSize       = 0;
    char   payload[]        = {'o', 'k'};
    SC_TEST_EXPECT(encodeFrame(writer, maskedHeader, {payload, sizeof(payload)}, {maskedOutput, sizeof(maskedOutput)},
                               maskedSize));

    HttpWebSocketFrameReader clientReader;
    clientReader.reset(HttpWebSocketEndpointRole::Client);
    size_t consumed = 0;
    Result result   = clientReader.parse({maskedOutput, maskedSize}, consumed);
    SC_TEST_EXPECT(resultMessageEquals(result, "HttpWebSocketFrameReader invalid frame masking for endpoint role"));

    HttpWebSocketFrameWriter serverWriter;
    serverWriter.reset(HttpWebSocketEndpointRole::Server);

    HttpWebSocketFrameHeaderView unmaskedHeader;
    unmaskedHeader.fin           = true;
    unmaskedHeader.opcode        = HttpWebSocketOpcode::Text;
    unmaskedHeader.masked        = false;
    unmaskedHeader.payloadLength = 2;

    char   unmaskedOutput[32] = {0};
    size_t unmaskedSize       = 0;
    char   unmaskedPayload[]  = {'n', 'o'};
    SC_TEST_EXPECT(encodeFrame(serverWriter, unmaskedHeader, {unmaskedPayload, sizeof(unmaskedPayload)},
                               {unmaskedOutput, sizeof(unmaskedOutput)}, unmaskedSize));

    HttpWebSocketFrameReader serverReader;
    serverReader.reset(HttpWebSocketEndpointRole::Server);
    consumed = 0;
    result   = serverReader.parse({unmaskedOutput, unmaskedSize}, consumed);
    SC_TEST_EXPECT(resultMessageEquals(result, "HttpWebSocketFrameReader invalid frame masking for endpoint role"));
}

void SC::HttpWebSocketFrameTest::invalidFrameRejection()
{
    HttpWebSocketFrameReader reader;
    size_t                   consumed = 0;

    SC::uint8_t rsvFrame[] = {0xC1, 0x00};
    reader.reset(HttpWebSocketEndpointRole::Client);
    Result result = reader.parse(byteSpan(rsvFrame, sizeof(rsvFrame)), consumed);
    SC_TEST_EXPECT(resultMessageEquals(result, "HttpWebSocketFrameReader RSV bits are not supported"));

    SC::uint8_t invalidOpcodeFrame[] = {0x83, 0x00};
    reader.reset(HttpWebSocketEndpointRole::Client);
    result = reader.parse(byteSpan(invalidOpcodeFrame, sizeof(invalidOpcodeFrame)), consumed);
    SC_TEST_EXPECT(resultMessageEquals(result, "HttpWebSocketFrameReader unsupported opcode"));

    char fragmentedControl[] = {char(0x09), char(0x00)};
    reader.reset(HttpWebSocketEndpointRole::Client);
    result = reader.parse({fragmentedControl, sizeof(fragmentedControl)}, consumed);
    SC_TEST_EXPECT(resultMessageEquals(result, "HttpWebSocketFrameReader control frames must not be fragmented"));

    SC::uint8_t oversizedControl[] = {0x89, 0x7E, 0x00, 0x7E};
    reader.reset(HttpWebSocketEndpointRole::Client);
    SC_TEST_EXPECT(not reader.parse(byteSpan(oversizedControl, sizeof(oversizedControl)), consumed));

    SC::uint8_t invalid64Length[] = {0x82, 0x7F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    reader.reset(HttpWebSocketEndpointRole::Client);
    SC_TEST_EXPECT(not reader.parse(byteSpan(invalid64Length, sizeof(invalid64Length)), consumed));

    SC::uint8_t continuationWithoutStart[] = {0x80, 0x00};
    reader.reset(HttpWebSocketEndpointRole::Client);
    SC_TEST_EXPECT(not reader.parse(byteSpan(continuationWithoutStart, sizeof(continuationWithoutStart)), consumed));

    SC::uint8_t unexpectedNewDataFrame[] = {0x01, 0x00, 0x81, 0x00};
    reader.reset(HttpWebSocketEndpointRole::Client);
    SC_TEST_EXPECT(not reader.parse(byteSpan(unexpectedNewDataFrame, sizeof(unexpectedNewDataFrame)), consumed));

    HttpWebSocketFrameWriter writer;
    writer.reset(HttpWebSocketEndpointRole::Server);
    HttpWebSocketFrameHeaderView invalidControl;
    invalidControl.fin           = false;
    invalidControl.opcode        = HttpWebSocketOpcode::Ping;
    invalidControl.masked        = false;
    invalidControl.payloadLength = 0;
    Span<const char> encodedHeader;
    char             storage[16] = {0};
    result                       = writer.beginFrame(invalidControl, {storage, sizeof(storage)}, encodedHeader);
    SC_TEST_EXPECT(resultMessageEquals(result, "HttpWebSocketFrameWriter control frames must not be fragmented"));
}

void SC::HttpWebSocketFrameTest::writerRoundtrip()
{
    HttpWebSocketFrameWriter writer;
    writer.reset(HttpWebSocketEndpointRole::Client);

    HttpWebSocketFrameHeaderView header;
    header.fin           = true;
    header.opcode        = HttpWebSocketOpcode::Binary;
    header.masked        = true;
    header.payloadLength = 6;
    header.maskKey[0]    = 0xAA;
    header.maskKey[1]    = 0xBB;
    header.maskKey[2]    = 0xCC;
    header.maskKey[3]    = 0xDD;

    char payload[]   = {char(0x00), char(0x11), char(0x22), char(0x33), char(0x44), char(0x55)};
    char encoded[32] = {0};

    Span<const char> encodedHeader;
    SC_TEST_EXPECT(writer.beginFrame(header, {encoded, sizeof(encoded)}, encodedHeader));

    Span<char> payloadOut = {encoded + encodedHeader.sizeInBytes(), sizeof(payload)};
    for (size_t idx = 0; idx < sizeof(payload); ++idx)
    {
        payloadOut[idx] = payload[idx];
    }

    SC_TEST_EXPECT(writer.writePayload({payloadOut.data(), 2}));
    SC_TEST_EXPECT(writer.writePayload({payloadOut.data() + 2, 4}));
    SC_TEST_EXPECT(writer.finishFrame());

    ReaderCollector          collector;
    HttpWebSocketFrameReader reader;
    reader.reset(HttpWebSocketEndpointRole::Server);
    reader.onFrameHeader.bind<ReaderCollector, &ReaderCollector::onHeader>(collector);
    reader.onFramePayload.bind<ReaderCollector, &ReaderCollector::onPayload>(collector);

    size_t consumed = 0;
    SC_TEST_EXPECT(reader.parse({encoded, encodedHeader.sizeInBytes() + sizeof(payload)}, consumed));
    SC_TEST_EXPECT(consumed == encodedHeader.sizeInBytes() + sizeof(payload));
    SC_TEST_EXPECT(collector.numHeaders == 1);
    SC_TEST_EXPECT(collector.headers[0].opcode == HttpWebSocketOpcode::Binary);
    SC_TEST_EXPECT(collector.headers[0].masked);
    SC_TEST_EXPECT(spansEqual(collector.payload.toSpanConst(), {payload, sizeof(payload)}));
}

void SC::runHttpWebSocketFrameTest(SC::TestReport& report) { HttpWebSocketFrameTest test(report); }
