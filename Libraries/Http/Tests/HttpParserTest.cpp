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
    void testRequest(HttpParser& parser, const StringView originalString, const StringView expectedMethodView)
    {
        parser.type = HttpParser::Type::Request;
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
            if (parser.state == HttpParser::State::Finished)
                break;
            SC_TEST_EXPECT(currentField.append(parsedData));
            if (parser.state == HttpParser::State::Result)
            {
                const StringView parsed(currentField.toSpan(), false, StringEncoding::Ascii);
                switch (parser.result)
                {
                case HttpParser::Result::Method: {
                    SC_TEST_EXPECT(parsed == expectedMethodView);
                    break;
                }
                case HttpParser::Result::Url: {
                    SC_TEST_EXPECT(parsed == "/asd");
                    break;
                }
                case HttpParser::Result::Version: {
                    SC_TEST_EXPECT(parsed == "HTTP/1.1");
                    break;
                }
                case HttpParser::Result::HeaderName: {
                    switch (numMatches[static_cast<int>(HttpParser::Result::HeaderName)])
                    {
                    case 0: SC_TEST_EXPECT(parsed == "User-agent"); break;
                    case 1: SC_TEST_EXPECT(parsed == "Host"); break;
                    }
                    break;
                }
                case HttpParser::Result::HeaderValue: {
                    switch (numMatches[static_cast<int>(HttpParser::Result::HeaderValue)])
                    {
                    case 0: SC_TEST_EXPECT(parsed == "Mozilla/1.1"); break;
                    case 1: SC_TEST_EXPECT(parsed == "github.com"); break;
                    }
                    break;
                }
                case HttpParser::Result::HeadersEnd: break;
                case HttpParser::Result::StatusCode: break;
                case HttpParser::Result::StatusString: break;
                case HttpParser::Result::Body: break;
                }
                numMatches[static_cast<int>(parser.result)]++;
                currentField.clear();
            }
        }
        SC_TEST_EXPECT(parser.state == HttpParser::State::Finished);

        SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Result::Method)] == 1);
        SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Result::Url)] == 1);
        SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Result::Version)] == 1);
        SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Result::HeaderName)] == 2);
        SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Result::HeaderValue)] == 2);
        SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Result::HeadersEnd)] == 1);
        SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Result::StatusCode)] == 0);
        SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Result::StatusString)] == 0);
        SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Result::Body)] == 0);
    }
    HttpParserTest(SC::TestReport& report) : TestCase(report, "HttpParserTest")
    {
        if (test_section("request GET"))
        {
            HttpParser parser;
            parser.method = HttpParser::Method::HttpPUT;
            testRequest(parser,
                        "GET /asd HTTP/1.1\r\n"
                        "User-agent: Mozilla/1.1\r\n"
                        "Host:   github.com\r\n"
                        "\r\n",
                        "GET");
            SC_TEST_EXPECT(parser.method == HttpParser::Method::HttpGET);
        }
        if (test_section("request POST"))
        {
            HttpParser parser;
            parser.method = HttpParser::Method::HttpPUT;
            testRequest(parser,
                        "POST /asd HTTP/1.1\r\n"
                        "User-agent: Mozilla/1.1\r\n"
                        "Host:   github.com\r\n"
                        "\r\n",
                        "POST");
            SC_TEST_EXPECT(parser.method == HttpParser::Method::HttpPOST);
        }
        if (test_section("request PUT"))
        {
            HttpParser parser;
            parser.method = HttpParser::Method::HttpPOST;
            testRequest(parser,
                        "PUT /asd HTTP/1.1\r\n"
                        "User-agent: Mozilla/1.1\r\n"
                        "Host:   github.com\r\n"
                        "\r\n",
                        "PUT");
            SC_TEST_EXPECT(parser.method == HttpParser::Method::HttpPUT);
        }
        if (test_section("response"))
        {
            HttpParser parser;
            parser.type = HttpParser::Type::Response;

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
                if (parser.state == HttpParser::State::Finished)
                    break;
                if (parser.state == HttpParser::State::Result)
                {
                    const StringView parsed(currentField.toSpan(), false, StringEncoding::Ascii);
                    switch (parser.result)
                    {
                    case HttpParser::Result::Method: break;
                    case HttpParser::Result::Url: break;

                    case HttpParser::Result::Version: {
                        SC_TEST_EXPECT(parsed == "HTTP/1.1");
                        break;
                    }
                    case HttpParser::Result::HeaderName: {
                        switch (numMatches[static_cast<int>(HttpParser::Result::HeaderName)])
                        {
                        case 0: SC_TEST_EXPECT(parsed == "Server"); break;
                        case 1: SC_TEST_EXPECT(parsed == "Content-Type"); break;
                        case 2: SC_TEST_EXPECT(parsed == "Content-Length"); break;
                        case 3: SC_TEST_EXPECT(parsed == "Connection"); break;
                        }
                        if (numMatches[static_cast<int>(HttpParser::Result::HeaderName)] == 2)
                        {
                            SC_TEST_EXPECT(parser.matchesHeader(HttpParser::HeaderType::ContentLength));
                        }
                        else
                        {
                            SC_TEST_EXPECT(not parser.matchesHeader(HttpParser::HeaderType::ContentLength));
                        }
                        break;
                    }
                    case HttpParser::Result::HeaderValue: {
                        switch (numMatches[static_cast<int>(HttpParser::Result::HeaderValue)])
                        {
                        case 0: SC_TEST_EXPECT(parsed == "nginx/1.2.1"); break;
                        case 1: SC_TEST_EXPECT(parsed == "text/html"); break;
                        case 2: SC_TEST_EXPECT(parsed == "8"); break;
                        case 3: SC_TEST_EXPECT(parsed == "keep-alive"); break;
                        }
                        break;
                    }
                    case HttpParser::Result::HeadersEnd: break;
                    case HttpParser::Result::StatusCode: {
                        SC_TEST_EXPECT(parsed == "200");
                        break;
                    }
                    case HttpParser::Result::StatusString: {
                        SC_TEST_EXPECT(parsed == "OK");
                        break;
                    }
                    case HttpParser::Result::Body: {
                        SC_TEST_EXPECT(parsed == "<html />");
                        break;
                    }
                    }
                    numMatches[static_cast<int>(parser.result)]++;
                    currentField.clear();
                }
            }
            SC_TEST_EXPECT(parser.state == HttpParser::State::Finished);
            SC_TEST_EXPECT(parser.statusCode == 200);
            SC_TEST_EXPECT(parser.contentLength == 8);

            SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Result::Method)] == 0);
            SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Result::Url)] == 0);
            SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Result::StatusCode)] == 1);
            SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Result::StatusString)] == 1);
            SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Result::Version)] == 1);
            SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Result::HeaderName)] == 4);
            SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Result::HeaderValue)] == 4);
            SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Result::HeadersEnd)] == 1);
            SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Result::Body)] == 1);
        }
    }
};

namespace SC
{
void runHttpParserTest(SC::TestReport& report) { HttpParserTest test(report); }
} // namespace SC
