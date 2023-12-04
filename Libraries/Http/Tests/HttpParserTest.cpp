// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../HttpParser.h"
#include "../../Strings/StringBuilder.h"
#include "../../Testing/Testing.h"
namespace SC
{
struct HttpParserTest;
}

struct SC::HttpParserTest : public SC::TestCase
{
    void testRequest(Http::Parser& parser, const StringView originalString, const StringView expectedMethodView)
    {
        parser.type = Http::Parser::Type::Request;
        // Test the streaming parser sending a single character at time
        size_t position  = 0;
        size_t readBytes = 0;

        int numMatches[9] = {0};

        size_t       length = 1;
        Vector<char> currentField;
        while (true)
        {
            length              = min(length, originalString.sizeInBytes());
            const auto       sv = originalString.sliceStartLengthBytes(position, length);
            Span<const char> parsedData;
            SC_TEST_EXPECT(parser.parse(sv.toCharSpan(), readBytes, parsedData));
            position += readBytes;
            if (parser.state == Http::Parser::State::Finished)
                break;
            SC_TEST_EXPECT(currentField.append(parsedData));
            if (parser.state == Http::Parser::State::Result)
            {
                const StringView parsed(currentField.toSpan(), false, StringEncoding::Ascii);
                switch (parser.result)
                {
                case Http::Parser::Result::Method: {
                    SC_TEST_EXPECT(parsed == expectedMethodView);
                    break;
                }
                case Http::Parser::Result::Url: {
                    SC_TEST_EXPECT(parsed == "/asd");
                    break;
                }
                case Http::Parser::Result::Version: {
                    SC_TEST_EXPECT(parsed == "HTTP/1.1");
                    break;
                }
                case Http::Parser::Result::HeaderName: {
                    switch (numMatches[static_cast<int>(Http::Parser::Result::HeaderName)])
                    {
                    case 0: SC_TEST_EXPECT(parsed == "User-agent"); break;
                    case 1: SC_TEST_EXPECT(parsed == "Host"); break;
                    }
                    break;
                }
                case Http::Parser::Result::HeaderValue: {
                    switch (numMatches[static_cast<int>(Http::Parser::Result::HeaderValue)])
                    {
                    case 0: SC_TEST_EXPECT(parsed == "Mozilla/1.1"); break;
                    case 1: SC_TEST_EXPECT(parsed == "github.com"); break;
                    }
                    break;
                }
                case Http::Parser::Result::HeadersEnd: break;
                case Http::Parser::Result::StatusCode: break;
                case Http::Parser::Result::StatusString: break;
                case Http::Parser::Result::Body: break;
                }
                numMatches[static_cast<int>(parser.result)]++;
                currentField.clear();
            }
        }
        SC_TEST_EXPECT(parser.state == Http::Parser::State::Finished);

        SC_TEST_EXPECT(numMatches[static_cast<int>(Http::Parser::Result::Method)] == 1);
        SC_TEST_EXPECT(numMatches[static_cast<int>(Http::Parser::Result::Url)] == 1);
        SC_TEST_EXPECT(numMatches[static_cast<int>(Http::Parser::Result::Version)] == 1);
        SC_TEST_EXPECT(numMatches[static_cast<int>(Http::Parser::Result::HeaderName)] == 2);
        SC_TEST_EXPECT(numMatches[static_cast<int>(Http::Parser::Result::HeaderValue)] == 2);
        SC_TEST_EXPECT(numMatches[static_cast<int>(Http::Parser::Result::HeadersEnd)] == 1);
        SC_TEST_EXPECT(numMatches[static_cast<int>(Http::Parser::Result::StatusCode)] == 0);
        SC_TEST_EXPECT(numMatches[static_cast<int>(Http::Parser::Result::StatusString)] == 0);
        SC_TEST_EXPECT(numMatches[static_cast<int>(Http::Parser::Result::Body)] == 0);
    }
    HttpParserTest(SC::TestReport& report) : TestCase(report, "HttpParserTest")
    {
        if (test_section("request GET"))
        {
            Http::Parser parser;
            parser.method = Http::Parser::Method::HttpPUT;
            testRequest(parser,
                        "GET /asd HTTP/1.1\r\n"
                        "User-agent: Mozilla/1.1\r\n"
                        "Host:   github.com\r\n"
                        "\r\n",
                        "GET");
            SC_TEST_EXPECT(parser.method == Http::Parser::Method::HttpGET);
        }
        if (test_section("request POST"))
        {
            Http::Parser parser;
            parser.method = Http::Parser::Method::HttpPUT;
            testRequest(parser,
                        "POST /asd HTTP/1.1\r\n"
                        "User-agent: Mozilla/1.1\r\n"
                        "Host:   github.com\r\n"
                        "\r\n",
                        "POST");
            SC_TEST_EXPECT(parser.method == Http::Parser::Method::HttpPOST);
        }
        if (test_section("request PUT"))
        {
            Http::Parser parser;
            parser.method = Http::Parser::Method::HttpPOST;
            testRequest(parser,
                        "PUT /asd HTTP/1.1\r\n"
                        "User-agent: Mozilla/1.1\r\n"
                        "Host:   github.com\r\n"
                        "\r\n",
                        "PUT");
            SC_TEST_EXPECT(parser.method == Http::Parser::Method::HttpPUT);
        }
        if (test_section("response"))
        {
            Http::Parser parser;
            parser.type = Http::Parser::Type::Response;

            const StringView originalString = "HTTP/1.1   200   OK\r\n"
                                              "Server: nginx/1.2.1\r\n"
                                              "Content-Type: text/html\r\n"
                                              "Content-Length: 8\r\n"
                                              "Connection: keep-alive\r\n"
                                              "\r\n"
                                              "<html />";

            // Test the streaming parser sending a single character at time
            size_t position  = 0;
            size_t readBytes = 0;

            int numMatches[9] = {0};

            size_t       length = 1;
            Vector<char> currentField;
            while (true)
            {
                length              = min(length, originalString.sizeInBytes());
                const auto       sv = originalString.sliceStartLengthBytes(position, length);
                Span<const char> parsedData;
                SC_TEST_EXPECT(parser.parse(sv.toCharSpan(), readBytes, parsedData));
                SC_TEST_EXPECT(currentField.append(parsedData));
                position += readBytes;
                if (parser.state == Http::Parser::State::Finished)
                    break;
                if (parser.state == Http::Parser::State::Result)
                {
                    const StringView parsed(currentField.toSpan(), false, StringEncoding::Ascii);
                    switch (parser.result)
                    {
                    case Http::Parser::Result::Method: break;
                    case Http::Parser::Result::Url: break;

                    case Http::Parser::Result::Version: {
                        SC_TEST_EXPECT(parsed == "HTTP/1.1");
                        break;
                    }
                    case Http::Parser::Result::HeaderName: {
                        switch (numMatches[static_cast<int>(Http::Parser::Result::HeaderName)])
                        {
                        case 0: SC_TEST_EXPECT(parsed == "Server"); break;
                        case 1: SC_TEST_EXPECT(parsed == "Content-Type"); break;
                        case 2: SC_TEST_EXPECT(parsed == "Content-Length"); break;
                        case 3: SC_TEST_EXPECT(parsed == "Connection"); break;
                        }
                        if (numMatches[static_cast<int>(Http::Parser::Result::HeaderName)] == 2)
                        {
                            SC_TEST_EXPECT(parser.matchesHeader(Http::Parser::HeaderType::ContentLength));
                        }
                        else
                        {
                            SC_TEST_EXPECT(not parser.matchesHeader(Http::Parser::HeaderType::ContentLength));
                        }
                        break;
                    }
                    case Http::Parser::Result::HeaderValue: {
                        switch (numMatches[static_cast<int>(Http::Parser::Result::HeaderValue)])
                        {
                        case 0: SC_TEST_EXPECT(parsed == "nginx/1.2.1"); break;
                        case 1: SC_TEST_EXPECT(parsed == "text/html"); break;
                        case 2: SC_TEST_EXPECT(parsed == "8"); break;
                        case 3: SC_TEST_EXPECT(parsed == "keep-alive"); break;
                        }
                        break;
                    }
                    case Http::Parser::Result::HeadersEnd: break;
                    case Http::Parser::Result::StatusCode: {
                        SC_TEST_EXPECT(parsed == "200");
                        break;
                    }
                    case Http::Parser::Result::StatusString: {
                        SC_TEST_EXPECT(parsed == "OK");
                        break;
                    }
                    case Http::Parser::Result::Body: {
                        SC_TEST_EXPECT(parsed == "<html />");
                        break;
                    }
                    }
                    numMatches[static_cast<int>(parser.result)]++;
                    currentField.clear();
                }
            }
            SC_TEST_EXPECT(parser.state == Http::Parser::State::Finished);
            SC_TEST_EXPECT(parser.statusCode == 200);
            SC_TEST_EXPECT(parser.contentLength == 8);

            SC_TEST_EXPECT(numMatches[static_cast<int>(Http::Parser::Result::Method)] == 0);
            SC_TEST_EXPECT(numMatches[static_cast<int>(Http::Parser::Result::Url)] == 0);
            SC_TEST_EXPECT(numMatches[static_cast<int>(Http::Parser::Result::StatusCode)] == 1);
            SC_TEST_EXPECT(numMatches[static_cast<int>(Http::Parser::Result::StatusString)] == 1);
            SC_TEST_EXPECT(numMatches[static_cast<int>(Http::Parser::Result::Version)] == 1);
            SC_TEST_EXPECT(numMatches[static_cast<int>(Http::Parser::Result::HeaderName)] == 4);
            SC_TEST_EXPECT(numMatches[static_cast<int>(Http::Parser::Result::HeaderValue)] == 4);
            SC_TEST_EXPECT(numMatches[static_cast<int>(Http::Parser::Result::HeadersEnd)] == 1);
            SC_TEST_EXPECT(numMatches[static_cast<int>(Http::Parser::Result::Body)] == 1);
        }
    }
};

namespace SC
{
void runHttpParserTest(SC::TestReport& report) { HttpParserTest test(report); }
} // namespace SC
