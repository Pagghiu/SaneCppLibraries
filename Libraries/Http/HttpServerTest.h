// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Strings/StringBuilder.h"
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
            constexpr int wantedNumTries = 2;
            int           numTries       = 0;
            EventLoop     eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            HttpServerAsync server;
            SC_TEST_EXPECT(server.start(eventLoop, 10, "127.0.0.1", 8080));
            server.onClient = [this, &numTries, &server](HttpServer::ClientChannel& client)
            {
                SC_TEST_EXPECT(client.request.headersEndReceived);
                numTries++;
                if (numTries == wantedNumTries)
                {
                    SC_TEST_EXPECT(server.stop());
                }
                auto& res = client.response;
                SC_TEST_EXPECT(res.startResponse(200));
                SC_TEST_EXPECT(res.addHeader("Connection", "Closed"));
                SC_TEST_EXPECT(res.addHeader("Content-Type", "text/html"));
                SC_TEST_EXPECT(res.addHeader("Server", "SC"));
                SC_TEST_EXPECT(res.addHeader("Date", "Mon, 27 Aug 2023 16:37:00 GMT"));
                SC_TEST_EXPECT(res.addHeader("Last-Modified", "Wed, 27 Aug 2023 16:37:00 GMT"));
                String        str;
                StringBuilder sb(str);
                const char    sampleHtml[] = "<html>\r\n"
                                             "<body bgcolor=\"#000000\" text=\"#ffffff\">\r\n"
                                             "<h1>This is a title {}!</h1>\r\n"
                                             "We must start from somewhere\r\n"
                                             "</body>\r\n"
                                             "</html>\r\n";
                SC_TEST_EXPECT(sb.format(sampleHtml, numTries));
                SC_TEST_EXPECT(client.response.end(str.view()));
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
                client[i].callback = [this](HttpClient& result)
                { SC_TEST_EXPECT(result.getResponse().containsString("This is a title")); };
                SC_TEST_EXPECT(client[i].start(eventLoop, "127.0.0.1", 8080, req));
            }
            SC_TEST_EXPECT(eventLoop.run());
            SC_TEST_EXPECT(numTries == wantedNumTries);
            SC_TEST_EXPECT(eventLoop.close());
        }
    }
};
