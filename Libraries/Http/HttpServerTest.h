// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/StringBuilder.h"
#include "../Testing/Test.h"
#include "HttpClient.h"
#include "HttpServer.h"
namespace SC
{
struct HttpServerTest;
}

struct SC::HttpServerTest : public SC::TestCase
{
    HttpServerTest(SC::TestReport& report) : TestCase(report, "HttpServerTest")
    {
        if (test_section("server async"))
        {
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            HttpServerAsync server;
            SC_TEST_EXPECT(server.start(eventLoop, 10, "127.0.0.1", 8080));
            int           numTries       = 0;
            constexpr int wantedNumTries = 2;
            server.onClient = [this, &numTries, &server](HttpServer::Request& req, HttpServer::Response& res)
            {
                SC_TEST_EXPECT(req.headersEndReceived);
                numTries++;
                if (numTries == wantedNumTries)
                {
                    SC_TEST_EXPECT(server.stop());
                }
                SC_TEST_EXPECT(res.writeHead(200));
                String        str;
                StringBuilder sb(str);
                const char    sampleHtml[] = "<html>\r\n"
                                             "<body bgcolor=\"#000000\" text=\"#ffffff\">\r\n"
                                             "<h1>This is a title {}!</h1>\r\n"
                                             "We must start from somewhere\r\n"
                                             "</body>\r\n"
                                             "</html>\r\n";
                SC_TEST_EXPECT(sb.format(sampleHtml, numTries));
                SC_TEST_EXPECT(res.end(str.view()));
            };
            HttpClient       client[3];
            const StringView req = "GET /asd HTTP/1.1\r\n"
                                   "User-agent: Mozilla/1.1\r\n"
                                   "Host:   github.com\r\n"
                                   "\r\n";
            SmallString<255> buffer;
            for (int i = 0; i < wantedNumTries; ++i)
            {
                StringBuilder sb(buffer, StringBuilder::Clear);
                SC_TEST_EXPECT(sb.format("HttpClient [{}]", i));
                SC_TEST_EXPECT(client[i].setCustomDebugName(buffer.view()));
                SC_TEST_EXPECT(
                    client[i].start(eventLoop, "127.0.0.1", 8080, req,
                                    [this](HttpClient& result)
                                    { SC_TEST_EXPECT(result.getResponse().containsString("This is a title")); }));
            }
            SC_TEST_EXPECT(eventLoop.run());
            SC_TEST_EXPECT(numTries == wantedNumTries);
            SC_TEST_EXPECT(eventLoop.close());
        }
    }
};
