// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpMultipartParser.h"
#include "Libraries/Memory/Buffer.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct HttpMultipartParserTest;
}

struct SC::HttpMultipartParserTest : public SC::TestCase
{
    void testMultipart(const StringView originalString, const StringView boundary, bool isStreaming)
    {
        HttpMultipartParser parser;
        SC_TEST_EXPECT(parser.initWithBoundary(boundary));

        size_t position  = 0;
        size_t readBytes = 0;

        int numMatches[9] = {0};

        size_t length = isStreaming ? 1 : originalString.sizeInBytes();
        Buffer currentField;
        int    stallCount = 0;

        while (true)
        {
            // If streaming, length is 1. If not, length is full size.
            // But we must support "streaming" chunks.
            // If isStreaming=true, we slice 1 char at a time.

            size_t bytesLeft = originalString.sizeInBytes() - position;
            if (bytesLeft == 0)
                break;

            size_t chunkLen = min(length, bytesLeft);

            const StringView sv = {
                {originalString.bytesWithoutTerminator() + position, chunkLen}, false, originalString.getEncoding()};

            Span<const char> parsedData;
            Result           res = parser.parse(sv.toCharSpan(), readBytes, parsedData);
            if (!res)
            {
                SC_TEST_EXPECT(res); // Fail
                break;
            }

            position += readBytes;

            if (parser.state == HttpMultipartParser::State::Finished)
                break;

            SC_TEST_EXPECT(currentField.append(parsedData));

            if (parser.state == HttpMultipartParser::State::Result)
            {
                const StringView parsed(currentField.toSpan(), false, StringEncoding::Ascii);
                switch (parser.token)
                {
                case HttpMultipartParser::Token::Boundary: {
                    // New Part
                    break;
                }
                case HttpMultipartParser::Token::HeaderName: {
                    if (parsed == "Content-Disposition")
                        numMatches[0]++;
                    else if (parsed == "name")
                        numMatches[1]++; // Wait, params are inside header value usually
                    break;
                }
                case HttpMultipartParser::Token::HeaderValue: {
                    if (parsed.containsString("form-data"))
                        numMatches[2]++;
                    break;
                }
                case HttpMultipartParser::Token::PartBody: {
                    if (parsed == "value1")
                        numMatches[3]++;
                    break;
                }
                case HttpMultipartParser::Token::PartHeaderEnd: break;
                case HttpMultipartParser::Token::Finished: break;
                }
                currentField.clear();
            }

            if (readBytes == 0 and chunkLen > 0)
            {
                // We didn't consume anything.
                // If we are in streaming mode, we must provide MORE data next time.
                if (isStreaming)
                {
                    length++; // Increase chunk size (pseudo-buffer growth)
                }
                else
                {
                    // In non-streaming mode, we might be in a "CheckingBoundary" pause.
                    // We should allow it, but we MUST ensure we don't loop forever.
                    stallCount++;
                    if (stallCount > 100)
                    {
                        SC_TEST_EXPECT(false && "Stalled in non-streaming mode");
                        break;
                    }
                }
            }
            else
            {
                stallCount = 0;
                // Reset chunk length to 1 if we succeeded consuming
                if (isStreaming)
                    length = 1;
            }
        }

        SC_TEST_EXPECT(parser.state == HttpMultipartParser::State::Finished);
    }

    HttpMultipartParserTest(SC::TestReport& report) : TestCase(report, "HttpMultipartParserTest")
    {
        if (test_section("simple multipart"))
        {
            testMultipart("--boundary\r\n"
                          "Content-Disposition: form-data; name=\"field1\"\r\n"
                          "\r\n"
                          "value1\r\n"
                          "--boundary--",
                          "boundary",
                          false // Full buffer
            );
        }

        if (test_section("streaming multipart"))
        {
            testMultipart("--boundary\r\n"
                          "Content-Disposition: form-data; name=\"field1\"\r\n"
                          "\r\n"
                          "value1\r\n"
                          "--boundary--",
                          "boundary",
                          true // Streaming
            );
        }

        if (test_section("multiple parts"))
        {
            testMultipart("--boundary\r\n"
                          "Content-Disposition: form-data; name=\"field1\"\r\n"
                          "\r\n"
                          "value1\r\n"
                          "--boundary\r\n"
                          "Content-Disposition: form-data; name=\"field2\"\r\n"
                          "\r\n"
                          "value2\r\n"
                          "--boundary--",
                          "boundary", false);
        }

        if (test_section("no headers"))
        {
            testMultipart("--boundary\r\n"
                          "\r\n"
                          "value1\r\n"
                          "--boundary--",
                          "boundary", false);
        }

        if (test_section("empty body"))
        {
            testMultipart("--boundary\r\n"
                          "Content-Disposition: form-data; name=\"field1\"\r\n"
                          "\r\n"
                          "\r\n"
                          "--boundary--",
                          "boundary", false);
        }

        if (test_section("preamble (should skip)"))
        {
            testMultipart("This is a preamble\r\n"
                          "--boundary\r\n"
                          "Content-Disposition: form-data; name=\"field1\"\r\n"
                          "\r\n"
                          "value1\r\n"
                          "--boundary--",
                          "boundary", false);
        }

        if (test_section("boundary in body"))
        {
            testMultipart("--boundary\r\n"
                          "Content-Disposition: form-data; name=\"field1\"\r\n"
                          "\r\n"
                          "This is a \r-not-the-boundary string\r\n"
                          "--boundary--",
                          "boundary", false);
        }

        if (test_section("large data streaming"))
        {
            // We want to verify that PartBody is emitted in chunks
            // and that PartHeaderEnd is received.
            HttpMultipartParser parser;
            SC_TEST_EXPECT(parser.initWithBoundary("boundary"));

            Buffer fullData;
            SC_TEST_EXPECT(fullData.append(StringView("--boundary\r\n").toCharSpan()));
            SC_TEST_EXPECT(fullData.append(StringView("Content-Type: text/plain\r\n").toCharSpan()));
            SC_TEST_EXPECT(fullData.append(StringView("\r\n").toCharSpan()));
            SC_TEST_EXPECT(
                fullData.append(StringView("This is a very long body that we want to stream in chunks.").toCharSpan()));
            SC_TEST_EXPECT(fullData.append(StringView("\r\n--boundary--").toCharSpan()));

            const auto dataSpan = fullData.toSpan().reinterpret_as_span_of<const char>();

            size_t           readBytes;
            Span<const char> parsedData;
            Span<const char> sub;

            size_t totalRead    = 0;
            bool   sawHeaderEnd = false;
            int    bodyChunks   = 0;
            size_t maxChunkSize = 5; // Force small chunks for streaming behavior

            while (totalRead < dataSpan.sizeInElements())
            {
                size_t bytesLeft = dataSpan.sizeInElements() - totalRead;
                size_t chunkLen  = min(maxChunkSize, bytesLeft);

                SC_TEST_EXPECT(dataSpan.sliceStartLength(totalRead, chunkLen, sub));
                SC_TEST_EXPECT(parser.parse(sub, readBytes, parsedData));

                totalRead += readBytes;

                if (parsedData.sizeInBytes() > 0)
                {
                    if (parser.token == HttpMultipartParser::Token::PartBody)
                    {
                        bodyChunks++;
                    }
                }

                if (parser.state == HttpMultipartParser::State::Result)
                {
                    if (parser.token == HttpMultipartParser::Token::PartHeaderEnd)
                    {
                        sawHeaderEnd = true;
                    }
                }
                if (parser.state == HttpMultipartParser::State::Finished)
                    break;

                // If we didn't consume anything, we MUST increase chunk size or we stall.
                // However, in this test we are already at maxChunkSize.
                // If readBytes == 0, it means it's a state transition token (like PartHeaderEnd).
                // We should just call it again with the same or more data.
                if (readBytes == 0)
                {
                    maxChunkSize += 5;
                }
                else
                {
                    maxChunkSize = 5;
                }
            }
            SC_TEST_EXPECT(sawHeaderEnd);
            SC_TEST_EXPECT(bodyChunks > 1);
            SC_TEST_EXPECT(parser.state == HttpMultipartParser::State::Finished);
        }
    }
};

namespace SC
{
void runHttpMultipartParserTest(SC::TestReport& report) { HttpMultipartParserTest test(report); }
} // namespace SC
