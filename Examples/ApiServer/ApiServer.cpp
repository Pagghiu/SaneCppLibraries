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
#include "../../Libraries/Http/HttpURLParser.h"
#include "../../Libraries/Memory/String.h"
#include "../../Libraries/Strings/CommandLine.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/StringBuilder.h"
#include "../../Libraries/Strings/StringView.h"

#include <string.h>

namespace SC
{
struct ApiServerExample
{
    static constexpr size_t MaxConnections = 32;

    using Connection = HttpAsyncConnection<16, 16, 16 * 1024, 512 * 1024>;

    struct EchoStream
    {
        HttpConnection*     connection       = nullptr;
        AsyncBufferView::ID pendingBufferID  = {};
        bool                hasPendingBuffer = false;
        bool                drainListening   = false;
        bool                endAfterPending  = false;

        Result start(HttpConnection& newConnection)
        {
            connection       = &newConnection;
            pendingBufferID  = {};
            hasPendingBuffer = false;
            drainListening   = false;
            endAfterPending  = false;
            SC_TRY(connection->response.startResponse(200));
            SC_TRY(connection->response.addHeader("Content-Type", "text/plain"));
            if (connection->request.getBodyFramingKind() == HttpBodyFramingKind::ContentLength)
            {
                SC_TRY(connection->response.addContentLength(connection->request.getBodyBytesRemaining()));
            }
            else
            {
                connection->response.setKeepAlive(false);
            }
            SC_TRY(connection->response.sendHeaders());
            return Result(true);
        }

        void onData(AsyncBufferView::ID bufferID)
        {
            if (connection == nullptr)
            {
                return;
            }
            if (not writeOrDefer(bufferID))
            {
                SC_ASSERT_RELEASE(false);
            }
        }

        void onDrain()
        {
            if (connection == nullptr or not hasPendingBuffer)
            {
                return;
            }

            const AsyncBufferView::ID bufferID = pendingBufferID;
            const Result writeResult           = connection->response.getWritableStream().write(bufferID);
            if (not writeResult)
            {
                return;
            }

            connection->buffersPool.unrefBuffer(bufferID);
            pendingBufferID  = {};
            hasPendingBuffer = false;
            removeDrainListener();

            if (endAfterPending)
            {
                SC_ASSERT_RELEASE(connection->response.end());
                connection = nullptr;
                return;
            }
            connection->request.getReadableStream().resumeReading();
        }

        void onEnd()
        {
            if (connection != nullptr)
            {
                removeBodyListeners();
                if (hasPendingBuffer)
                {
                    endAfterPending = true;
                    return;
                }
                SC_ASSERT_RELEASE(connection->response.end());
                connection = nullptr;
            }
        }

        bool writeOrDefer(AsyncBufferView::ID bufferID)
        {
            const Result writeResult = connection->response.getWritableStream().write(bufferID);
            if (writeResult)
            {
                return true;
            }
            if (hasPendingBuffer)
            {
                return false;
            }

            connection->buffersPool.refBuffer(bufferID);
            pendingBufferID  = bufferID;
            hasPendingBuffer = true;
            connection->request.getReadableStream().pause();
            if (not drainListening)
            {
                drainListening = connection->response.getWritableStream()
                                     .eventDrain.addListener<EchoStream, &EchoStream::onDrain>(*this);
            }
            if (not drainListening)
            {
                connection->buffersPool.unrefBuffer(bufferID);
                pendingBufferID  = {};
                hasPendingBuffer = false;
            }
            return drainListening;
        }

        void removeBodyListeners()
        {
            (void)connection->request.getReadableStream().eventData.removeListener<EchoStream, &EchoStream::onData>(
                *this);
            (void)connection->request.getReadableStream().eventEnd.removeListener<EchoStream, &EchoStream::onEnd>(
                *this);
        }

        void removeDrainListener()
        {
            if (drainListening)
            {
                (void)connection->response.getWritableStream()
                    .eventDrain.removeListener<EchoStream, &EchoStream::onDrain>(*this);
                drainListening = false;
            }
        }
    };

    Connection      connections[MaxConnections];
    EchoStream      echoStreams[MaxConnections];
    HttpAsyncServer server;

    String  interface = "127.0.0.1";
    int32_t port      = 8091;

    Result start(AsyncEventLoop& loop)
    {
        SC_TRY(server.init(Span<Connection>(connections)));
        SC_TRY(server.start(loop, interface.view(), static_cast<uint16_t>(port)));
        server.onRequest.bind<ApiServerExample, &ApiServerExample::onRequest>(*this);
        return Result(true);
    }

    void onRequest(HttpConnection& connection)
    {
        String        targetURL = StringEncoding::Ascii;
        HttpURLParser url;
        SC_ASSERT_RELEASE(StringBuilder::format(targetURL, "http://localhost{}", connection.request.getRequestTarget()));
        SC_ASSERT_RELEASE(url.parse(targetURL.view()));

        if (url.pathname == "/health")
        {
            SC_ASSERT_RELEASE(sendText(connection, 200, "application/json", "{\"status\":\"ok\"}"));
            return;
        }

        if (url.pathname == "/hello")
        {
            StringSpan name;
            if (not url.getQueryValue("name", name) or name.isEmpty())
            {
                name = "world";
            }
            String response = StringEncoding::Ascii;
            SC_ASSERT_RELEASE(StringBuilder::format(response, "{{\"hello\":\"{}\"}}", name));
            SC_ASSERT_RELEASE(sendText(connection, 200, "application/json", response.view()));
            return;
        }

        if (url.pathname == "/echo")
        {
            if (connection.request.getParser().method != HttpParser::Method::HttpPOST)
            {
                SC_ASSERT_RELEASE(methodNotAllowed(connection, "POST"));
                return;
            }
            receiveEchoBody(connection);
            return;
        }

        SC_ASSERT_RELEASE(notFound(connection));
    }

    void receiveEchoBody(HttpConnection& connection)
    {
        const size_t index = connection.getConnectionID().getIndex();
        SC_ASSERT_RELEASE(index < MaxConnections);
        EchoStream& echo = echoStreams[index];
        SC_ASSERT_RELEASE(echo.start(connection));

        const bool addedData = connection.request.getReadableStream().eventData.addListener<EchoStream, &EchoStream::onData>(
            echo);
        const bool addedEnd = connection.request.getReadableStream().eventEnd.addListener<EchoStream, &EchoStream::onEnd>(
            echo);
        SC_ASSERT_RELEASE(addedData);
        SC_ASSERT_RELEASE(addedEnd);
        if (connection.request.getBodyFramingKind() == HttpBodyFramingKind::None or
            (connection.request.getBodyFramingKind() == HttpBodyFramingKind::ContentLength and
             connection.request.getBodyBytesRemaining() == 0))
        {
            (void)connection.request.getReadableStream()
                .eventData.removeListener<EchoStream, &EchoStream::onData>(echo);
            (void)connection.request.getReadableStream().eventEnd.removeListener<EchoStream, &EchoStream::onEnd>(echo);
            SC_ASSERT_RELEASE(connection.response.end());
            echo.connection = nullptr;
        }
    }

    Result sendText(HttpConnection& connection, int code, StringSpan contentType, StringSpan body)
    {
        SC_TRY(connection.response.startResponse(code));
        SC_TRY(connection.response.addHeader("Content-Type", contentType));
        SC_TRY(connection.response.addContentLength(body.sizeInBytes()));
        SC_TRY(connection.response.sendHeaders());
        if (body.sizeInBytes() > 0)
        {
            AsyncBufferView::ID bufferID;
            Span<char>          writableData;
            SC_TRY(connection.buffersPool.requestNewBuffer(body.sizeInBytes(), bufferID, writableData));
            ::memcpy(writableData.data(), body.bytesWithoutTerminator(), body.sizeInBytes());
            connection.buffersPool.setNewBufferSize(bufferID, body.sizeInBytes());
            const Result writeResult = connection.response.getWritableStream().write(bufferID);
            connection.buffersPool.unrefBuffer(bufferID);
            SC_TRY(writeResult);
        }
        return connection.response.end();
    }

    Result methodNotAllowed(HttpConnection& connection, StringSpan allow)
    {
        SC_TRY(connection.response.startResponse(405));
        SC_TRY(connection.response.addHeader("Allow", allow));
        SC_TRY(connection.response.addContentLength(0));
        SC_TRY(connection.response.sendHeaders());
        return connection.response.end();
    }

    Result notFound(HttpConnection& connection) { return sendText(connection, 404, "text/plain", "not found\n"); }
};

Result saneMain(Span<const StringSpan> args)
{
    Console::tryAttachingToParentConsole();
    Console console;

    static ApiServerExample sample;
    uint16_t         port = static_cast<uint16_t>(sample.port);

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
