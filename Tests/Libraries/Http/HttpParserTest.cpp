// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpParser.h"
#include "Libraries/Containers/Vector.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
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

        size_t length = 1;
        Buffer currentField;
        while (true)
        {
            length = min(length, originalString.sizeInBytes());

            const StringView sv = {
                {originalString.bytesWithoutTerminator() + position, length}, false, originalString.getEncoding()};
            Span<const char> parsedData;
            SC_TEST_EXPECT(parser.parse(sv.toCharSpan(), readBytes, parsedData));
            position += readBytes;
            if (parser.state == HttpParser::State::Finished)
                break;
            SC_TEST_EXPECT(currentField.append(parsedData));
            if (parser.state == HttpParser::State::Result)
            {
                const StringView parsed(currentField.toSpan(), false, StringEncoding::Ascii);
                switch (parser.token)
                {
                case HttpParser::Token::Method: {
                    SC_TEST_EXPECT(parsed == expectedMethodView);
                    break;
                }
                case HttpParser::Token::Url: {
                    SC_TEST_EXPECT(parsed == "/asd");
                    break;
                }
                case HttpParser::Token::Version: {
                    SC_TEST_EXPECT(parsed == "HTTP/1.1");
                    break;
                }
                case HttpParser::Token::HeaderName: {
                    switch (numMatches[static_cast<int>(HttpParser::Token::HeaderName)])
                    {
                    case 0: SC_TEST_EXPECT(parsed == "User-agent"); break;
                    case 1: SC_TEST_EXPECT(parsed == "Host"); break;
                    }
                    break;
                }
                case HttpParser::Token::HeaderValue: {
                    switch (numMatches[static_cast<int>(HttpParser::Token::HeaderValue)])
                    {
                    case 0: SC_TEST_EXPECT(parsed == "Mozilla/1.1"); break;
                    case 1: SC_TEST_EXPECT(parsed == "github.com"); break;
                    }
                    break;
                }
                case HttpParser::Token::HeadersEnd: break;
                case HttpParser::Token::StatusCode: break;
                case HttpParser::Token::StatusString: break;
                case HttpParser::Token::Body: break;
                }
                numMatches[static_cast<int>(parser.token)]++;
                currentField.clear();
            }
        }
        SC_TEST_EXPECT(parser.state == HttpParser::State::Finished);

        SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Token::Method)] == 1);
        SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Token::Url)] == 1);
        SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Token::Version)] == 1);
        SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Token::HeaderName)] == 2);
        SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Token::HeaderValue)] == 2);
        SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Token::HeadersEnd)] == 1);
        SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Token::StatusCode)] == 0);
        SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Token::StatusString)] == 0);
        SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Token::Body)] == 0);
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

            size_t length = 1;
            Buffer currentField;
            while (true)
            {
                length = min(length, originalString.sizeInBytes());

                const StringView sv = {
                    {originalString.bytesWithoutTerminator() + position, length}, false, originalString.getEncoding()};
                Span<const char> parsedData;
                SC_TEST_EXPECT(parser.parse(sv.toCharSpan(), readBytes, parsedData));
                SC_TEST_EXPECT(currentField.append(parsedData));
                position += readBytes;
                if (parser.state == HttpParser::State::Finished)
                    break;
                if (parser.state == HttpParser::State::Result)
                {
                    const StringView parsed(currentField.toSpan(), false, StringEncoding::Ascii);
                    switch (parser.token)
                    {
                    case HttpParser::Token::Method: break;
                    case HttpParser::Token::Url: break;

                    case HttpParser::Token::Version: {
                        SC_TEST_EXPECT(parsed == "HTTP/1.1");
                        break;
                    }
                    case HttpParser::Token::HeaderName: {
                        switch (numMatches[static_cast<int>(HttpParser::Token::HeaderName)])
                        {
                        case 0: SC_TEST_EXPECT(parsed == "Server"); break;
                        case 1: SC_TEST_EXPECT(parsed == "Content-Type"); break;
                        case 2: SC_TEST_EXPECT(parsed == "Content-Length"); break;
                        case 3: SC_TEST_EXPECT(parsed == "Connection"); break;
                        }
                        if (numMatches[static_cast<int>(HttpParser::Token::HeaderName)] == 2)
                        {
                            SC_TEST_EXPECT(parser.matchesHeader(HttpParser::HeaderType::ContentLength));
                        }
                        else
                        {
                            SC_TEST_EXPECT(not parser.matchesHeader(HttpParser::HeaderType::ContentLength));
                        }
                        break;
                    }
                    case HttpParser::Token::HeaderValue: {
                        switch (numMatches[static_cast<int>(HttpParser::Token::HeaderValue)])
                        {
                        case 0: SC_TEST_EXPECT(parsed == "nginx/1.2.1"); break;
                        case 1: SC_TEST_EXPECT(parsed == "text/html"); break;
                        case 2: SC_TEST_EXPECT(parsed == "8"); break;
                        case 3: SC_TEST_EXPECT(parsed == "keep-alive"); break;
                        }
                        break;
                    }
                    case HttpParser::Token::HeadersEnd: break;
                    case HttpParser::Token::StatusCode: {
                        SC_TEST_EXPECT(parsed == "200");
                        break;
                    }
                    case HttpParser::Token::StatusString: {
                        SC_TEST_EXPECT(parsed == "OK");
                        break;
                    }
                    case HttpParser::Token::Body: {
                        SC_TEST_EXPECT(parsed == "<html />");
                        break;
                    }
                    }
                    numMatches[static_cast<int>(parser.token)]++;
                    currentField.clear();
                }
            }
            SC_TEST_EXPECT(parser.state == HttpParser::State::Finished);
            SC_TEST_EXPECT(parser.statusCode == 200);
            SC_TEST_EXPECT(parser.contentLength == 8);

            SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Token::Method)] == 0);
            SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Token::Url)] == 0);
            SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Token::StatusCode)] == 1);
            SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Token::StatusString)] == 1);
            SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Token::Version)] == 1);
            SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Token::HeaderName)] == 4);
            SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Token::HeaderValue)] == 4);
            SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Token::HeadersEnd)] == 1);
            SC_TEST_EXPECT(numMatches[static_cast<int>(HttpParser::Token::Body)] == 1);
        }
    }
};

namespace SC
{
void runHttpParserTest(SC::TestReport& report) { HttpParserTest test(report); }
} // namespace SC
