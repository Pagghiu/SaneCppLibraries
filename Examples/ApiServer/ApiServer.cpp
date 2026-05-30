// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A small async HTTP API server example using the low-level HttpAsyncServer surface.
//---------------------------------------------------------------------------------------------------------------------
// Usage:
//   ApiServer --port 8091
//   curl http://127.0.0.1:8091/health
//   curl "http://127.0.0.1:8091/hello?name=SaneCpp"
//   curl -X POST --data "hello" http://127.0.0.1:8091/echo
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Http/HttpAsyncServer.h"
#include "../../Libraries/Http/HttpHeaders.h"
#include "../../Libraries/Http/HttpRouter.h"
#include "../../Libraries/Http/HttpURLParser.h"
#include "../../Libraries/Memory/String.h"
#include "../../Libraries/Strings/CommandLine.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/StringBuilder.h"
#include "../../Libraries/Strings/StringView.h"

namespace SC
{
struct ApiServerExample
{
    static constexpr size_t MaxConnections = 32;

    using Connection = HttpAsyncConnection<16, 16, 16 * 1024, 512 * 1024>;

    Connection      connections[MaxConnections];
    HttpAsyncServer server;
    HttpRouter      router;
    HttpRoute       routes[3] = {
        {HttpParser::Method::HttpGET, "/health"},
        {HttpParser::Method::HttpGET, "/hello"},
        {HttpParser::Method::HttpPOST, "/echo"},
    };

    String  interface = "127.0.0.1";
    int32_t port      = 8091;

    Result start(AsyncEventLoop& loop)
    {
        SC_TRY(server.init(Span<Connection>(connections)));
        SC_TRY(router.init(routes));
        SC_TRY(server.start(loop, interface.view(), static_cast<uint16_t>(port)));
        server.onRequest.bind<ApiServerExample, &ApiServerExample::onRequest>(*this);
        return Result(true);
    }

    void onRequest(HttpConnection& connection)
    {
        HttpRouteMatch match;
        SC_ASSERT_RELEASE(
            router.match(connection.request.getParser().method, connection.request.getRequestTarget(), {}, match));
        if (match.status == HttpRouteMatchStatus::NotFound)
        {
            SC_ASSERT_RELEASE(notFound(connection));
            return;
        }
        if (match.status == HttpRouteMatchStatus::MethodNotAllowed)
        {
            char       allowStorage[64];
            StringSpan allow;
            SC_ASSERT_RELEASE(router.formatAllowHeader(connection.request.getRequestTarget(), allowStorage, allow));
            SC_ASSERT_RELEASE(connection.response.sendMethodNotAllowed(allow));
            return;
        }
        if (match.status != HttpRouteMatchStatus::Matched)
        {
            SC_ASSERT_RELEASE(connection.sendTextCopy(500, "route error\n"));
            return;
        }

        if (match.route == &routes[0])
        {
            SC_ASSERT_RELEASE(connection.sendJsonCopy(200, "{\"status\":\"ok\"}"));
            return;
        }
        if (match.route == &routes[1])
        {
            HttpRequestTargetView target;
            SC_ASSERT_RELEASE(target.parse(connection.request.getRequestTarget()));

            StringSpan name;
            if (not target.getQueryValue("name", name) or name.isEmpty())
            {
                name = "world";
            }
            String response = StringEncoding::Ascii;
            SC_ASSERT_RELEASE(StringBuilder::format(response, "{{\"hello\":\"{}\"}}", name));
            SC_ASSERT_RELEASE(connection.sendJsonCopy(200, response.view()));
            return;
        }
        if (match.route == &routes[2])
        {
            receiveEchoBody(connection);
            return;
        }

        SC_ASSERT_RELEASE(connection.sendTextCopy(500, "route error\n"));
    }

    void receiveEchoBody(HttpConnection& connection)
    {
        SC_ASSERT_RELEASE(connection.response.startResponse(200));
        SC_ASSERT_RELEASE(connection.response.addHeader("Content-Type", HttpContentTypeTextPlainUtf8()));
        if (connection.request.getBodyFramingKind() == HttpBodyFramingKind::ContentLength)
        {
            SC_ASSERT_RELEASE(connection.response.addContentLength(connection.request.getBodyBytesRemaining()));
        }
        else
        {
            connection.response.setKeepAlive(false);
        }
        SC_ASSERT_RELEASE(connection.response.sendHeaders());

        if (connection.request.getBodyFramingKind() == HttpBodyFramingKind::None or
            (connection.request.getBodyFramingKind() == HttpBodyFramingKind::ContentLength and
             connection.request.getBodyBytesRemaining() == 0))
        {
            SC_ASSERT_RELEASE(connection.response.end());
            return;
        }

        connection.pipeline.source   = &connection.request.getReadableStream();
        connection.pipeline.sinks[0] = &connection.response.getWritableStream();
        SC_ASSERT_RELEASE(connection.pipeline.pipe());
        SC_ASSERT_RELEASE(connection.pipeline.start());
    }

    Result notFound(HttpConnection& connection) { return connection.sendTextCopy(404, "not found\n"); }
};

Result saneMain(Span<const StringSpan> args)
{
    Console::tryAttachingToParentConsole();
    Console console;

    static ApiServerExample sample;
    uint16_t                port = static_cast<uint16_t>(sample.port);

    CommandLineOption options[1];
    options[0].longName  = "port";
    options[0].help      = "Port to listen on";
    options[0].valueName = "PORT";
    options[0].shortName = 'p';
    options[0].value     = CommandLineValue::uint16(port);

    CommandLineSpec spec;
    spec.programName = "ApiServer";
    spec.summary     = "A small async JSON/text API server example.";
    spec.options     = options;

    const CommandLineParseResult parseResult = spec.parse(args);
    if (parseResult.status == CommandLineParseResult::Status::HelpRequested)
    {
        StringFormatOutput output(StringEncoding::Utf8, console, true);
        SC_TRY(spec.writeHelp(output));
        console.flush();
        return Result(true);
    }
    if (parseResult.status == CommandLineParseResult::Status::Error)
    {
        StringFormatOutput output(StringEncoding::Utf8, console, false);
        SC_TRY(spec.writeError(parseResult, output));
        console.flushStdErr();
        return Result(false);
    }
    if (port == 0)
    {
        console.printError("Invalid port value: 0\n");
        return Result(false);
    }

    sample.port = static_cast<int32_t>(port);
    SocketNetworking::initNetworking();

    AsyncEventLoop loop;
    SC_TRY(loop.create());
    SC_TRY(sample.start(loop));
    console.print("ApiServer listening on http://{}:{}\n", sample.interface, sample.port);
    console.print("Routes: GET /health, GET /hello?name=SaneCpp, POST /echo\n");
    return loop.run();
}
} // namespace SC

template <typename CharType>
static int apiServerMain(int argc, CharType** argv)
{
    using namespace SC;
    static constexpr size_t MaxArguments = 4;
    StringSpan              argsStorage[MaxArguments];
    CommandLineArguments    args;
    if (not args.setFromMainArguments(argc, argv, argsStorage))
    {
        return -1;
    }
    return SC::saneMain(args.values) ? 0 : -1;
}

#if SC_PLATFORM_WINDOWS
int wmain(int argc, wchar_t** argv) { return apiServerMain(argc, argv); }
#else
int main(int argc, char** argv) { return apiServerMain(argc, argv); }
#endif
