// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/AsyncStreams/CryptographyTransformStreams.h"
#include "Libraries/Testing/Testing.h"

#include <memory.h>

namespace SC
{
struct CryptographyTransformStreamsTest;
}

struct SC::CryptographyTransformStreamsTest : public SC::TestCase
{
    struct TestCipher
    {
        int constructorArgument = 0;
        int updateCalls         = 0;
        int finishCalls         = 0;
        int resetCalls          = 0;

        TestCipher() = default;
        explicit TestCipher(int argument) : constructorArgument(argument) {}

        Result update(Span<const uint8_t> input, Span<uint8_t> output, size_t& bytesWritten)
        {
            updateCalls += 1;
            bytesWritten = 0;
            SC_TRY_MSG(output.sizeInBytes() >= input.sizeInBytes(), "TestCipher - insufficient output");
            for (size_t idx = 0; idx < input.sizeInBytes(); ++idx)
                output[idx] = static_cast<uint8_t>(input[idx] ^ 0x5a);
            bytesWritten = input.sizeInBytes();
            return Result(true);
        }

        Result finish(Span<uint8_t> output, size_t& bytesWritten)
        {
            finishCalls += 1;
            bytesWritten = 0;
            SC_TRY_MSG(not output.empty(), "TestCipher - insufficient final output");
            output[0]    = 0xee;
            bytesWritten = 1;
            return Result(true);
        }

        void reset() { resetCalls += 1; }
    };

    struct TestHmac
    {
        int     constructorArgument = 0;
        uint8_t bytes[32]           = {0};
        size_t  size                = 0;
        int     resetCalls          = 0;

        TestHmac() = default;
        explicit TestHmac(int argument) : constructorArgument(argument) {}

        Result add(Span<const uint8_t> data)
        {
            SC_TRY_MSG(size + data.sizeInBytes() <= sizeof(bytes), "TestHmac - input too large");
            memcpy(bytes + size, data.data(), data.sizeInBytes());
            size += data.sizeInBytes();
            return Result(true);
        }

        void reset()
        {
            resetCalls += 1;
            memset(bytes, 0, sizeof(bytes));
            size = 0;
        }
    };

    CryptographyTransformStreamsTest(TestReport& report) : TestCase(report, "CryptographyTransformStreamsTest")
    {
        if (test_section("cipher template adapter"))
            cipherTemplateAdapter();
        if (test_section("HMAC template adapter"))
            hmacTemplateAdapter();
        if (test_section("cipher rejects undersized buffers"))
            cipherRejectsUndersizedBuffers();
        if (test_section("explicit destruction resets sessions"))
            explicitDestructionResetsSessions();
    }

    void cipherTemplateAdapter();
    void hmacTemplateAdapter();
    void cipherRejectsUndersizedBuffers();
    void explicitDestructionResetsSessions();
};

void SC::CryptographyTransformStreamsTest::cipherTemplateAdapter()
{
    char            outputStorage[32] = {0};
    AsyncBufferView bufferViews[4];
    bufferViews[0] = Span<char>(outputStorage, 16);
    bufferViews[0].setReusable(true);
    bufferViews[1] = Span<char>(outputStorage + 16, 16);
    bufferViews[1].setReusable(true);

    AsyncBuffersPool pool;
    pool.setBuffers(bufferViews);

    AsyncCipherTransformStreamT<TestCipher> stream(17);
    SC_TEST_EXPECT(stream.cipher.constructorArgument == 17);
    AsyncReadableStream::Request readRequests[3];
    AsyncWritableStream::Request writeRequests[3];
    SC_TEST_EXPECT(stream.init(pool, readRequests, writeRequests));

    uint8_t transformed[32] = {0};
    size_t  transformedSize = 0;
    struct Collector
    {
        AsyncCipherTransformStreamT<TestCipher>* stream;

        uint8_t* transformed;

        size_t* transformedSize;
        size_t  capacity;
    } collector = {&stream, transformed, &transformedSize, sizeof(transformed)};
    (void)stream.eventData.addListener(
        [this, collector = &collector](AsyncBufferView::ID bufferID)
        {
            Span<const char> data;
            SC_TEST_EXPECT(collector->stream->AsyncReadableStream::getBuffersPool().getReadableData(bufferID, data));
            SC_TEST_EXPECT(*collector->transformedSize + data.sizeInBytes() <= collector->capacity);
            memcpy(collector->transformed + *collector->transformedSize, data.data(), data.sizeInBytes());
            *collector->transformedSize += data.sizeInBytes();
        });
    (void)stream.AsyncReadableStream::eventError.addListener([this](Result result) { SC_TEST_EXPECT(result); });
    (void)stream.AsyncWritableStream::eventError.addListener([this](Result result) { SC_TEST_EXPECT(result); });

    static constexpr uint8_t input[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
    SC_TEST_EXPECT(stream.AsyncReadableStream::start());
    SC_TEST_EXPECT(stream.AsyncWritableStream::write(
        AsyncBufferView(Span<const char>(reinterpret_cast<const char*>(input), sizeof(input)))));
    stream.AsyncWritableStream::end();

    SC_TEST_EXPECT(transformedSize == sizeof(input) + 1);
    for (size_t idx = 0; idx < sizeof(input); ++idx)
        SC_TEST_EXPECT(transformed[idx] == static_cast<uint8_t>(input[idx] ^ 0x5a));
    SC_TEST_EXPECT(transformed[sizeof(input)] == 0xee);
    SC_TEST_EXPECT(stream.cipher.updateCalls == static_cast<int>(sizeof(input)));
    SC_TEST_EXPECT(stream.cipher.finishCalls == 1);
    SC_TEST_EXPECT(stream.cipher.resetCalls >= 1);
}

void SC::CryptographyTransformStreamsTest::hmacTemplateAdapter()
{
    AsyncBufferView  bufferViews[2];
    AsyncBuffersPool pool;
    pool.setBuffers(bufferViews);

    AsyncHmacWritableStreamT<TestHmac> sink(23);
    SC_TEST_EXPECT(sink.hmac.constructorArgument == 23);
    AsyncWritableStream::Request writeRequests[3];
    sink.setWriteQueue(writeRequests);
    SC_TEST_EXPECT(sink.init(pool));

    bool observedMacInput = false;
    struct FinishContext
    {
        AsyncHmacWritableStreamT<TestHmac>* sink;
        bool*                               observedMacInput;
    } finishContext = {&sink, &observedMacInput};
    (void)sink.eventFinish.addListener(
        [this, finishContext = &finishContext]
        {
            SC_TEST_EXPECT(finishContext->sink->hmac.size == 8);
            SC_TEST_EXPECT(memcmp(finishContext->sink->hmac.bytes, "Hi There", 8) == 0);
            *finishContext->observedMacInput = true;
        });
    (void)sink.eventError.addListener([this](Result result) { SC_TEST_EXPECT(result); });

    SC_TEST_EXPECT(sink.write(AsyncBufferView("Hi ")));
    SC_TEST_EXPECT(sink.write(AsyncBufferView("There")));
    sink.end();

    SC_TEST_EXPECT(observedMacInput);
    SC_TEST_EXPECT(sink.hmac.resetCalls == 1);
}

void SC::CryptographyTransformStreamsTest::cipherRejectsUndersizedBuffers()
{
    char            outputStorage[15];
    AsyncBufferView bufferViews[2];
    bufferViews[0] = Span<char>(outputStorage);
    bufferViews[0].setReusable(true);
    AsyncBuffersPool pool;
    pool.setBuffers(bufferViews);

    AsyncCipherTransformStreamT<TestCipher> stream;
    AsyncReadableStream::Request            readRequests[2];
    AsyncWritableStream::Request            writeRequests[2];
    SC_TEST_EXPECT(stream.init(pool, readRequests, writeRequests));

    bool emittedError = false;
    (void)stream.AsyncWritableStream::eventError.addListener([emittedError = &emittedError](Result)
                                                             { *emittedError = true; });
    SC_TEST_EXPECT(stream.AsyncReadableStream::start());
    SC_TEST_EXPECT(stream.AsyncWritableStream::write(AsyncBufferView("x")));
    SC_TEST_EXPECT(emittedError);
    SC_TEST_EXPECT(stream.cipher.updateCalls == 0);
    SC_TEST_EXPECT(stream.cipher.resetCalls == 1);
}

void SC::CryptographyTransformStreamsTest::explicitDestructionResetsSessions()
{
    char            outputStorage[16];
    AsyncBufferView cipherBufferViews[2];
    cipherBufferViews[0] = Span<char>(outputStorage);
    cipherBufferViews[0].setReusable(true);
    AsyncBuffersPool cipherPool;
    cipherPool.setBuffers(cipherBufferViews);

    AsyncCipherTransformStreamT<TestCipher> cipherStream;
    AsyncReadableStream::Request            readRequests[2];
    AsyncWritableStream::Request            writeRequests[2];
    SC_TEST_EXPECT(cipherStream.init(cipherPool, readRequests, writeRequests));
    cipherStream.AsyncReadableStream::destroy();
    cipherStream.AsyncWritableStream::destroy();
    SC_TEST_EXPECT(cipherStream.cipher.resetCalls == 2);

    AsyncBufferView  hmacBufferViews[1];
    AsyncBuffersPool hmacPool;
    hmacPool.setBuffers(hmacBufferViews);
    AsyncHmacWritableStreamT<TestHmac> hmacStream;
    AsyncWritableStream::Request       hmacWriteRequests[2];
    hmacStream.setWriteQueue(hmacWriteRequests);
    SC_TEST_EXPECT(hmacStream.init(hmacPool));
    hmacStream.destroy();
    SC_TEST_EXPECT(hmacStream.hmac.resetCalls == 1);
}

namespace SC
{
void runCryptographyTransformStreamsTest(TestReport& report) { CryptographyTransformStreamsTest test(report); }
} // namespace SC
