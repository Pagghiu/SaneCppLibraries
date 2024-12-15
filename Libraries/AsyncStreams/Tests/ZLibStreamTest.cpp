// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Internal/ZLibStream.h"
#include "../../Strings/Console.h"
#include "../../Strings/String.h"
#include "../../Strings/StringBuilder.h"
#include "../../Testing/Testing.h"

namespace SC
{
struct ZLibStreamTest;
}

struct SC::ZLibStreamTest : public SC::TestCase
{
    ZLibStreamTest(SC::TestReport& report) : TestCase(report, "ZLibStreamTest")
    {
        if (test_section("gzip"))
        {
            // "test" compressed with gzip
            static constexpr uint8_t testCompressedGZIP[] = {
                0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x2B, 0x49,
                0x2D, 0x2E, 0x01, 0x00, 0x0C, 0x7E, 0x7F, 0xD8, 0x04, 0x00, 0x00, 0x00,
            };
            syncDecompression(ZLibStream::DecompressGZip, "test"_a8, testCompressedGZIP);
            syncCompression(ZLibStream::CompressGZip, "test"_a8, testCompressedGZIP);
        }
        if (test_section("deflate"))
        {
            // "test" compressed with deflate
            static constexpr uint8_t testCompressedDEFLATE[] = {0x2B, 0x49, 0x2D, 0x2E, 0x01, 0x00};
            syncDecompression(ZLibStream::DecompressDeflate, "test"_a8, testCompressedDEFLATE);
            syncCompression(ZLibStream::CompressDeflate, "test"_a8, testCompressedDEFLATE);
        }
        if (test_section("zlib"))
        {
            // "test" compressed with zlib
            static constexpr uint8_t testCompressedZLIB[] = {0x78, 0x9C, 0x2B, 0x49, 0x2D, 0x2E,
                                                             0x01, 0x00, 0x04, 0x5D, 0x01, 0xC1};
            syncDecompression(ZLibStream::DecompressZLib, "test"_a8, testCompressedZLIB);
            syncCompression(ZLibStream::CompressZLib, "test"_a8, testCompressedZLIB);
        }
    }

    void syncCompression(ZLibStream::Algorithm compressionAlgorithm, const StringView inputString,
                         const Span<const uint8_t> compressedReference);
    void syncDecompression(ZLibStream::Algorithm compressionAlgorithm, const StringView referenceString,
                           const Span<const uint8_t> compressedReference);
};

void SC::ZLibStreamTest::syncCompression(ZLibStream::Algorithm compressionAlgorithm, const StringView inputString,
                                         const Span<const uint8_t> compressedReference)
{
    ZLibStream compressor;
    SC_TEST_EXPECT(compressor.init(compressionAlgorithm));

    const size_t halfStringLength = inputString.sizeInBytes() / 2;

    char       writableBufferData[32];
    Span<char> writableBuffer = writableBufferData;

    // Process first half of the input data
    Span<const char> sourceData;
    SC_TEST_EXPECT(inputString.toCharSpan().sliceStartLength(0, halfStringLength, sourceData));
    Span<char> destination = writableBuffer;
    SC_TEST_EXPECT(compressor.process(sourceData, destination));
    SC_TEST_EXPECT(sourceData.empty());

    // Process second half of the input data, but only give a single byte of additional output space
    SC_TEST_EXPECT(inputString.toCharSpan().sliceStart(halfStringLength, sourceData));
    Span<char> singleByte;
    SC_TEST_EXPECT(destination.sliceStartLength(0, 1, singleByte));
    SC_TEST_EXPECT(compressor.process(sourceData, singleByte));
    SC_TEST_EXPECT(destination.sliceStart(1 - singleByte.sizeInBytes(), destination));
    SC_TEST_EXPECT(sourceData.empty());

    // Try finalizing with a single byte of additional space, but the stream should not end
    bool streamEnded = false;
    SC_TEST_EXPECT(destination.sliceStartLength(0, 1, singleByte));
    SC_TEST_EXPECT(compressor.finalize(singleByte, streamEnded));
    SC_TEST_EXPECT(destination.sliceStart(1 - singleByte.sizeInBytes(), destination));
    SC_TEST_EXPECT(not streamEnded);

    // Now try finalizing with all the remaining space, expecting the stream to end
    SC_TEST_EXPECT(compressor.finalize(destination, streamEnded));
    SC_TEST_EXPECT(streamEnded);

    // Check that the output is same as expected
    Span<char> output;
    SC_TEST_EXPECT(writableBuffer.sliceFromStartUntil(destination, output));
    if (compressionAlgorithm == ZLibStream::CompressGZip and output.sizeInBytes() > 9 and
        compressedReference.sizeInBytes() > 9)
    {
        output[9] = static_cast<char>(compressedReference[9]); // Operating System ID will differ on different OS.
    }
    SC_TEST_EXPECT(compressedReference.equals(output));
}

void SC::ZLibStreamTest::syncDecompression(ZLibStream::Algorithm algorithm, const StringView referenceString,
                                           const Span<const uint8_t> compressedReference)
{
    const Span<const char> reference = compressedReference.reinterpret_as_array_of<const char>();

    ZLibStream compressor;
    SC_TEST_EXPECT(compressor.init(algorithm));

    char       writableBufferData[32];
    Span<char> writableBuffer = writableBufferData;

    // Process first half of the input data
    Span<const char> sourceData;
    SC_TEST_EXPECT(reference.sliceStartLength(0, reference.sizeInElements() / 2, sourceData));
    Span<char> destination = writableBuffer;
    SC_TEST_EXPECT(compressor.process(sourceData, destination));
    SC_TEST_EXPECT(sourceData.empty());

    // Process second half of the input data, but only give a single byte of additional output space
    SC_TEST_EXPECT(reference.sliceStart(reference.sizeInElements() / 2, sourceData));
    Span<char> singleByte;
    SC_TEST_EXPECT(destination.sliceStartLength(0, 1, singleByte));
    SC_TEST_EXPECT(compressor.process(sourceData, singleByte));
    SC_TEST_EXPECT(destination.sliceStart(1 - singleByte.sizeInBytes(), destination));
    SC_TEST_EXPECT(singleByte.empty()); // Byte must have been consumed

    // Process all the rest
    SC_TEST_EXPECT(compressor.process(sourceData, destination));
    SC_TEST_EXPECT(sourceData.empty()); // All input data consumed

    // Try finalizing with a single byte of additional space, but the stream should not end
    bool streamEnded = false;
    SC_TEST_EXPECT(compressor.finalize(destination, streamEnded));
    SC_TEST_EXPECT(streamEnded);

    // Check that the output is same as expected
    Span<char> output;
    SC_TEST_EXPECT(writableBuffer.sliceFromStartUntil(destination, output));
    SC_TEST_EXPECT(StringView(output, false, StringEncoding::Ascii) == referenceString);
}
namespace SC
{
void runZLibStreamTest(SC::TestReport& report) { ZLibStreamTest test(report); }
} // namespace SC
