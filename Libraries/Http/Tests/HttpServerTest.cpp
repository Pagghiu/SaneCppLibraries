// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../HttpServer.h"
#include "../../Strings/StringBuilder.h"
#include "../../Testing/Testing.h"
#include "../HttpClient.h"
namespace SC
{
struct HttpServerTest;
}

struct SC::HttpServerTest : public SC::TestCase
{
    int numTries = 0;
    HttpServerTest(SC::TestReport& report) : TestCase(report, "HttpServerTest")
    {
        if (test_section("server async"))
        {
            constexpr int wantedNumTries = 3;
            EventLoop     eventLoop;
            numTries = 0;
            SC_TEST_EXPECT(eventLoop.create());
            HttpServerAsync server;
            SC_TEST_EXPECT(server.start(eventLoop, 10, "127.0.0.1", 8080));
            server.onClient = [this, &server](HttpServer::ClientChannel& client)
            {
                auto& res = client.response;
                SC_TEST_EXPECT(client.request.headersEndReceived);
                if (client.request.parser.method != HttpParser::Method::HttpGET)
                {
                    SC_TEST_EXPECT(res.startResponse(405));
                    SC_TEST_EXPECT(client.response.end(""));
                    return;
                }
                if (client.request.url != "/index.html" and client.request.url != "/")
                {
                    SC_TEST_EXPECT(res.startResponse(404));
                    SC_TEST_EXPECT(client.response.end(""));
                    return;
                }
                numTries++;
                if (numTries == wantedNumTries)
                {
                    SC_TEST_EXPECT(server.stop());
                }
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
            SmallString<255> buffer;
            for (int i = 0; i < wantedNumTries; ++i)
            {
                StringBuilder sb(buffer, StringBuilder::Clear);
                SC_TEST_EXPECT(sb.format("HttpClient [{}]", i));
                SC_TEST_EXPECT(client[i].setCustomDebugName(buffer.view()));
                client[i].callback = [this](HttpClient& result)
                { SC_TEST_EXPECT(result.getResponse().containsString("This is a title")); };
                SC_TEST_EXPECT(client[i].get(eventLoop, "http://localhost:8080/index.html"));
            }
            SC_TEST_EXPECT(eventLoop.run());
            SC_TEST_EXPECT(numTries == wantedNumTries);
            SC_TEST_EXPECT(eventLoop.close());
        }
    }
};

namespace SC
{
void runHttpServerTest(SC::TestReport& report) { HttpServerTest test(report); }
} // namespace SC
