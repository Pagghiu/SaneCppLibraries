// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/AsyncStreams/CryptographyTransformStreams.h"
#include "Libraries/Cryptography/Cryptography.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"

#include <memory.h>

namespace SC
{
struct CryptographyAsyncStreamsTest;
}

struct SC::CryptographyAsyncStreamsTest : public SC::TestCase
{
    static constexpr uint8_t ZeroKey16[16]   = {0};
    static constexpr uint8_t ZeroIV16[16]    = {0};
    static constexpr uint8_t ZeroPlain16[16] = {0};

    static constexpr uint8_t Aes128CbcExpected[32] = {
        0x66, 0xe9, 0x4b, 0xd4, 0xef, 0x8a, 0x2c, 0x3b, 0x88, 0x4c, 0xfa, 0x59, 0xca, 0x34, 0x2b, 0x2e,
        0x94, 0x34, 0xde, 0xc2, 0xd0, 0x0f, 0xda, 0xc7, 0x65, 0xf0, 0x0c, 0x0c, 0x11, 0x62, 0x8c, 0xd1,
    };
    static constexpr uint8_t HmacSha256Expected[32] = {
        0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0x0b, 0xf1, 0x2b,
        0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83, 0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7,
    };

    Cryptography::Features features;

    CryptographyAsyncStreamsTest(TestReport& report) : TestCase(report, "CryptographyAsyncStreamsTest")
    {
        SC_TEST_EXPECT(Cryptography::queryFeatures(features));
        if (test_section("CBC transform known answer and buffer boundaries"))
            cbcTransformKnownAnswerAndBufferBoundaries();
        if (test_section("CBC transform rejects invalid padding"))
            cbcTransformRejectsInvalidPadding();
        if (test_section("HMAC pipeline fan-out"))
            hmacPipelineFanOut();
    }

    template <size_t BufferSize>
    Result runCipher(AsyncCipherTransformStreamT<Cryptography::Cipher>& stream, Span<const uint8_t> input,
                     Span<const size_t> chunks, Span<uint8_t> output, size_t& outputSize)
    {
        char            outputStorage[BufferSize] = {0};
        AsyncBufferView bufferViews[2];
        bufferViews[0] = Span<char>(outputStorage);
        bufferViews[0].setReusable(true);
        AsyncBuffersPool pool;
        pool.setBuffers(bufferViews);

        AsyncReadableStream::Request readRequests[3];
        AsyncWritableStream::Request writeRequests[3];
        SC_TRY(stream.init(pool, readRequests, writeRequests));

        struct Collector
        {
            AsyncCipherTransformStreamT<Cryptography::Cipher>* stream;

            Span<uint8_t> output;

            size_t* outputSize;
            bool    succeeded = true;
        } collector = {&stream, output, &outputSize};
        (void)stream.eventData.addListener(
            [collector = &collector](AsyncBufferView::ID bufferID)
            {
                Span<const char> data;
                if (not collector->stream->AsyncReadableStream::getBuffersPool().getReadableData(bufferID, data) or
                    *collector->outputSize + data.sizeInBytes() > collector->output.sizeInBytes())
                {
                    collector->succeeded = false;
                    return;
                }
                memcpy(collector->output.data() + *collector->outputSize, data.data(), data.sizeInBytes());
                *collector->outputSize += data.sizeInBytes();
            });
        (void)stream.AsyncReadableStream::eventError.addListener([collector = &collector](Result)
                                                                 { collector->succeeded = false; });
        (void)stream.AsyncWritableStream::eventError.addListener([collector = &collector](Result)
                                                                 { collector->succeeded = false; });

        SC_TRY(stream.AsyncReadableStream::start());
        size_t inputOffset = 0;
        for (size_t chunk : chunks)
        {
            SC_TRY_MSG(inputOffset + chunk <= input.sizeInBytes(), "runCipher - invalid input chunks");
            const Span<const char> chunkData(reinterpret_cast<const char*>(input.data() + inputOffset), chunk);
            SC_TRY(stream.AsyncWritableStream::write(AsyncBufferView(chunkData)));
            inputOffset += chunk;
        }
        SC_TRY_MSG(inputOffset == input.sizeInBytes(), "runCipher - input chunks do not cover input");
        stream.AsyncWritableStream::end();
        SC_TRY_MSG(collector.succeeded, "runCipher - stream error");
        return Result(true);
    }

    void cbcTransformKnownAnswerAndBufferBoundaries();
    void cbcTransformRejectsInvalidPadding();
    void hmacPipelineFanOut();
};

namespace SC
{
constexpr uint8_t CryptographyAsyncStreamsTest::ZeroKey16[16];
constexpr uint8_t CryptographyAsyncStreamsTest::ZeroIV16[16];
constexpr uint8_t CryptographyAsyncStreamsTest::ZeroPlain16[16];
constexpr uint8_t CryptographyAsyncStreamsTest::Aes128CbcExpected[32];
constexpr uint8_t CryptographyAsyncStreamsTest::HmacSha256Expected[32];
} // namespace SC

void SC::CryptographyAsyncStreamsTest::cbcTransformKnownAnswerAndBufferBoundaries()
{
    if (not features.aes128CbcPkcs7)
        return;

    static constexpr size_t encryptChunks[] = {5, 1, 10};
    static constexpr size_t decryptChunks[] = {7, 9, 1, 15};

    uint8_t encrypted[32] = {0};
    size_t  encryptedSize = 0;

    AsyncCipherTransformStreamT<Cryptography::Cipher> encrypt;
    SC_TEST_EXPECT(encrypt.cipher.start(Cryptography::CipherType::AES128CBCPKCS7,
                                        Cryptography::Cipher::Operation::Encrypt, ZeroKey16, ZeroIV16));
    SC_TEST_EXPECT(runCipher<16>(encrypt, ZeroPlain16, encryptChunks, encrypted, encryptedSize));
    SC_TEST_EXPECT(encryptedSize == sizeof(Aes128CbcExpected));
    SC_TEST_EXPECT(memcmp(encrypted, Aes128CbcExpected, sizeof(encrypted)) == 0);

    uint8_t decrypted[16] = {0};
    size_t  decryptedSize = 0;

    AsyncCipherTransformStreamT<Cryptography::Cipher> decrypt;
    SC_TEST_EXPECT(decrypt.cipher.start(Cryptography::CipherType::AES128CBCPKCS7,
                                        Cryptography::Cipher::Operation::Decrypt, ZeroKey16, ZeroIV16));
    SC_TEST_EXPECT(runCipher<17>(decrypt, Aes128CbcExpected, decryptChunks, decrypted, decryptedSize));
    SC_TEST_EXPECT(decryptedSize == sizeof(ZeroPlain16));
    SC_TEST_EXPECT(memcmp(decrypted, ZeroPlain16, sizeof(decrypted)) == 0);

    uint8_t encryptedLargeBuffer[32] = {0};
    size_t  encryptedLargeBufferSize = 0;

    AsyncCipherTransformStreamT<Cryptography::Cipher> encryptLargeBuffer;
    SC_TEST_EXPECT(encryptLargeBuffer.cipher.start(Cryptography::CipherType::AES128CBCPKCS7,
                                                   Cryptography::Cipher::Operation::Encrypt, ZeroKey16, ZeroIV16));
    SC_TEST_EXPECT(runCipher<4096>(encryptLargeBuffer, ZeroPlain16, encryptChunks, encryptedLargeBuffer,
                                   encryptedLargeBufferSize));
    SC_TEST_EXPECT(encryptedLargeBufferSize == sizeof(Aes128CbcExpected));
    SC_TEST_EXPECT(memcmp(encryptedLargeBuffer, Aes128CbcExpected, sizeof(encryptedLargeBuffer)) == 0);

    uint8_t encryptedBoundaryBuffer[32] = {0};
    size_t  encryptedBoundaryBufferSize = 0;

    AsyncCipherTransformStreamT<Cryptography::Cipher> encryptBoundaryBuffer;
    SC_TEST_EXPECT(encryptBoundaryBuffer.cipher.start(Cryptography::CipherType::AES128CBCPKCS7,
                                                      Cryptography::Cipher::Operation::Encrypt, ZeroKey16, ZeroIV16));
    SC_TEST_EXPECT(runCipher<31>(encryptBoundaryBuffer, ZeroPlain16, encryptChunks, encryptedBoundaryBuffer,
                                 encryptedBoundaryBufferSize));
    SC_TEST_EXPECT(encryptedBoundaryBufferSize == sizeof(Aes128CbcExpected));
    SC_TEST_EXPECT(memcmp(encryptedBoundaryBuffer, Aes128CbcExpected, sizeof(encryptedBoundaryBuffer)) == 0);
}

void SC::CryptographyAsyncStreamsTest::cbcTransformRejectsInvalidPadding()
{
    if (not features.aes128CbcPkcs7)
        return;

    uint8_t corrupted[sizeof(Aes128CbcExpected)];
    memcpy(corrupted, Aes128CbcExpected, sizeof(corrupted));
    corrupted[15] ^= 0x01;

    static constexpr size_t decryptChunks[] = {3, 13, 16};
    uint8_t                 decrypted[32]   = {0};
    size_t                  decryptedSize   = 0;

    AsyncCipherTransformStreamT<Cryptography::Cipher> decrypt;
    SC_TEST_EXPECT(decrypt.cipher.start(Cryptography::CipherType::AES128CBCPKCS7,
                                        Cryptography::Cipher::Operation::Decrypt, ZeroKey16, ZeroIV16));
    SC_TEST_EXPECT(not runCipher<31>(decrypt, corrupted, decryptChunks, decrypted, decryptedSize));
}

void SC::CryptographyAsyncStreamsTest::hmacPipelineFanOut()
{
    if (not features.hmacSha256)
        return;

    char            sourceStorage[16] = {0};
    AsyncBufferView bufferViews[3];
    bufferViews[0] = Span<char>(sourceStorage);
    bufferViews[0].setReusable(true);
    AsyncBuffersPool pool;
    pool.setBuffers(bufferViews);

    struct Source : public AsyncReadableStream
    {
        bool emitted = false;

        virtual Result asyncRead() override
        {
            if (emitted)
            {
                pushEnd();
                return Result(true);
            }
            AsyncBufferView::ID bufferID;
            Span<char>          output;
            if (getBufferOrPause(8, bufferID, output))
            {
                emitted = true;
                memcpy(output.data(), "Hi There", 8);
                const bool keepReading = push(bufferID, 8);
                getBuffersPool().unrefBuffer(bufferID);
                if (keepReading)
                    reactivate(true);
            }
            return Result(true);
        }
    } source;

    struct CopySink : public AsyncWritableStream
    {
        uint8_t bytes[8] = {0};
        size_t  size     = 0;

        virtual Result asyncWrite(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> callback) override
        {
            Span<const char> input;
            SC_TRY(getBuffersPool().getReadableData(bufferID, input));
            SC_TRY_MSG(size + input.sizeInBytes() <= sizeof(bytes), "CopySink - input too large");
            memcpy(bytes + size, input.data(), input.sizeInBytes());
            size += input.sizeInBytes();
            finishedWriting(bufferID, move(callback), Result(true));
            return Result(true);
        }
    } copySink;

    AsyncHmacWritableStreamT<Cryptography::Hmac> hmacSink;
    SC_TEST_EXPECT(hmacSink.hmac.setType(Cryptography::HashType::SHA256));
    SC_TEST_EXPECT(hmacSink.hmac.setKey(
        "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"_a8.toBytesSpan()));

    AsyncReadableStream::Request sourceRequests[2];
    AsyncWritableStream::Request hmacRequests[2];
    AsyncWritableStream::Request copyRequests[2];
    source.setReadQueue(sourceRequests);
    hmacSink.setWriteQueue(hmacRequests);
    copySink.setWriteQueue(copyRequests);
    SC_TEST_EXPECT(source.init(pool));
    SC_TEST_EXPECT(hmacSink.init(pool));
    SC_TEST_EXPECT(copySink.init(pool));

    Cryptography::MacResult mac;
    bool                    macReady = false;
    struct FinishContext
    {
        AsyncHmacWritableStreamT<Cryptography::Hmac>* sink;

        Cryptography::MacResult* mac;
        bool*                    macReady;
    } finishContext = {&hmacSink, &mac, &macReady};
    (void)hmacSink.eventFinish.addListener(
        [finishContext = &finishContext]
        { *finishContext->macReady = finishContext->sink->hmac.getMac(*finishContext->mac); });

    bool          pipelineSucceeded = true;
    AsyncPipeline pipeline          = {&source, {}, {&copySink, &hmacSink}};
    (void)pipeline.eventError.addListener([pipelineSucceeded = &pipelineSucceeded](Result)
                                          { *pipelineSucceeded = false; });
    SC_TEST_EXPECT(pipeline.pipe());
    SC_TEST_EXPECT(pipeline.start());

    SC_TEST_EXPECT(pipelineSucceeded);
    SC_TEST_EXPECT(copySink.size == 8);
    SC_TEST_EXPECT(memcmp(copySink.bytes, "Hi There", 8) == 0);
    SC_TEST_EXPECT(macReady);
    SC_TEST_EXPECT(mac.size == sizeof(HmacSha256Expected));
    SC_TEST_EXPECT(memcmp(mac.bytes, HmacSha256Expected, sizeof(HmacSha256Expected)) == 0);
}

namespace SC
{
void runCryptographyAsyncStreamsTest(TestReport& report) { CryptographyAsyncStreamsTest test(report); }
} // namespace SC
