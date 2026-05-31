// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
// --------------------------------------------------------------------------------------------------
// SC_BEGIN_PLUGIN
//
// Name:          CollaborativeCanvas
// Version:       1
// Description:   Runs a small WebSocket-backed collaborative drawing canvas from SCExample
// Category:      Http
// Build:         libc
// Dependencies:
//
// SC_END_PLUGIN
// --------------------------------------------------------------------------------------------------
#include "../ImguiHelpers.h"
#include "imgui.h"

#include "Libraries/Containers/VirtualArray.h"
#include "Libraries/ContainersReflection/ContainersReflection.h"
#include "Libraries/ContainersReflection/MemorySerialization.h"
#include "Libraries/Http/HttpAsyncServer.h"
#include "Libraries/Http/HttpHeaders.h"
#include "Libraries/Http/HttpURLParser.h"
#include "Libraries/Http/HttpWebSocket.h"
#include "Libraries/Plugin/PluginMacros.h"
#include "Libraries/SerializationBinary/SerializationBinary.h"

#include "../ISCExample.h"

#include <stdio.h>
#include <string.h>

namespace SC
{
struct CollaborativeCanvasModel;
struct CollaborativeCanvasView;
struct CollaborativeCanvasModelState;
struct CollaborativeCanvasViewState;
} // namespace SC

struct SC::CollaborativeCanvasModelState
{
    String  interface  = "0.0.0.0";
    int32_t port       = 8092;
    int32_t maxClients = 32;

    HttpConnectionsPool::Configuration asyncConfiguration;

    CollaborativeCanvasModelState()
    {
        asyncConfiguration.readQueueSize    = 8;
        asyncConfiguration.writeQueueSize   = 64;
        asyncConfiguration.buffersQueueSize = 128;
    }
};

SC_REFLECT_STRUCT_VISIT(SC::CollaborativeCanvasModelState)
SC_REFLECT_STRUCT_FIELD(0, interface)
SC_REFLECT_STRUCT_FIELD(1, port)
SC_REFLECT_STRUCT_FIELD(2, maxClients)
SC_REFLECT_STRUCT_LEAVE()

struct SC::CollaborativeCanvasViewState
{
    Buffer inputTextBuffer;

    bool needsRestart = false;
    bool needsResize  = false;
};
SC_REFLECT_STRUCT_VISIT(SC::CollaborativeCanvasViewState)
SC_REFLECT_STRUCT_FIELD(0, needsRestart)
SC_REFLECT_STRUCT_LEAVE()

struct SC::CollaborativeCanvasModel
{
    static constexpr size_t MAX_CONNECTIONS  = 1000000;
    static constexpr size_t MAX_READ_QUEUE   = 16;
    static constexpr size_t MAX_WRITE_QUEUE  = 128;
    static constexpr size_t MAX_BUFFERS      = 256;
    static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024;
    static constexpr size_t MAX_HEADER_SIZE  = 32 * 1024;

    static const char* opcodeName(HttpWebSocketOpcode opcode)
    {
        switch (opcode)
        {
        case HttpWebSocketOpcode::Continuation: return "Continuation";
        case HttpWebSocketOpcode::Text: return "Text";
        case HttpWebSocketOpcode::Binary: return "Binary";
        case HttpWebSocketOpcode::Close: return "Close";
        case HttpWebSocketOpcode::Ping: return "Ping";
        case HttpWebSocketOpcode::Pong: return "Pong";
        }
        return "Unknown";
    }

    struct WebSocketRuntime
    {
        CollaborativeCanvasModel*     owner      = nullptr;
        HttpConnection*               connection = nullptr;
        size_t                        hubIndex   = size_t(-1);
        HttpWebSocketConnectionPump   pump;
        HttpWebSocketMessageAssembler messageAssembler;
        char                          messageStorage[16 * 1024] = {0};

        Result onFrameHeader(const HttpWebSocketFrameHeaderView& header)
        {
            return messageAssembler.onFrameHeader(header);
        }

        Result onMessage(HttpWebSocketOpcode opcode, Span<const char> message)
        {
            if (opcode != HttpWebSocketOpcode::Text and opcode != HttpWebSocketOpcode::Binary)
            {
                return Result(true);
            }

            owner->totalFrames++;
            owner->totalBytes += message.sizeInBytes();
            char frameStorage[16 * 1024]     = {0};
            owner->currentBroadcastSourceHub = hubIndex;
            Result broadcast                 = owner->webSocketHub.broadcastText(message, frameStorage);
            owner->currentBroadcastSourceHub = size_t(-1);
            return broadcast;
        }

        Result onPayload(HttpWebSocketOpcode, Span<char> payload, bool frameFinished)
        {
            return messageAssembler.onFramePayload(payload, frameFinished);
        }

        Result onPing(Span<char>)
        {
            if (owner != nullptr)
            {
                owner->controlFrames++;
            }
            return Result(true);
        }

        Result onClose(uint16_t, Span<char>)
        {
            if (owner != nullptr)
            {
                owner->controlFrames++;
            }
            return Result(true);
        }

        void onError(Result result)
        {
            if (connection != nullptr)
            {
                ::printf("[CanvasWS] destroying readable conn=%zu reason=%s\n",
                         connection->getConnectionID().getIndex(), result.message);
                ::fflush(stdout);
            }
        }

        void detach()
        {
            const size_t connectionIndex =
                connection != nullptr ? connection->getConnectionID().getIndex() : size_t(-1);
            if (connection != nullptr or hubIndex != size_t(-1))
            {
                ::printf("[CanvasWS] detach conn=%zu hub=%zu attached=%d activeBefore=%zu\n", connectionIndex, hubIndex,
                         pump.isAttached() ? 1 : 0, owner != nullptr ? owner->webSocketHub.getNumClients() : 0);
                ::fflush(stdout);
            }
            pump.detach();
            if (owner != nullptr and hubIndex != size_t(-1))
            {
                (void)owner->webSocketHub.leave(hubIndex);
                hubIndex = size_t(-1);
            }
            owner      = nullptr;
            connection = nullptr;
            if (connectionIndex != size_t(-1))
            {
                ::printf("[CanvasWS] detached conn=%zu\n", connectionIndex);
                ::fflush(stdout);
            }
        }

        void onEnd()
        {
            ::printf("[CanvasWS] onEnd conn=%zu hub=%zu\n",
                     connection != nullptr ? connection->getConnectionID().getIndex() : size_t(-1), hubIndex);
            ::fflush(stdout);
            detach();
        }
    };

    CollaborativeCanvasModelState modelState;

    AsyncEventLoop* eventLoop = nullptr;
    HttpAsyncServer httpServer;

    VirtualArray<HttpConnection> clients = {MAX_CONNECTIONS};

    VirtualArray<AsyncReadableStream::Request> allReadQueues  = {MAX_CONNECTIONS * MAX_READ_QUEUE};
    VirtualArray<AsyncWritableStream::Request> allWriteQueues = {MAX_CONNECTIONS * MAX_WRITE_QUEUE};
    VirtualArray<AsyncBufferView>              allBuffers     = {MAX_CONNECTIONS * MAX_BUFFERS};
    VirtualArray<char>                         allHeaders     = {MAX_CONNECTIONS * MAX_HEADER_SIZE};
    VirtualArray<char>                         allStreams     = {MAX_CONNECTIONS * MAX_REQUEST_SIZE};

    VirtualArray<HttpWebSocketHubClient> webSocketHubClients = {MAX_CONNECTIONS};
    VirtualArray<WebSocketRuntime>       webSocketRuntimes   = {MAX_CONNECTIONS};
    HttpWebSocketSmallHub                webSocketHub;

    size_t totalFrames               = 0;
    size_t totalBytes                = 0;
    size_t controlFrames             = 0;
    size_t droppedBroadcasts         = 0;
    size_t currentBroadcastSourceHub = size_t(-1);

    Result start()
    {
        normalizeConfiguration();
        ::printf("[CanvasWS] start interface=%.*s port=%d maxClients=%d\n",
                 static_cast<int>(modelState.interface.view().sizeInBytes()),
                 modelState.interface.view().bytesWithoutTerminator(), modelState.port, modelState.maxClients);
        ::fflush(stdout);
        SC_TRY(assignConnectionMemory(static_cast<size_t>(modelState.maxClients)));
        SC_TRY(httpServer.init(clients.toSpan()));
        SC_TRY(httpServer.start(*eventLoop, modelState.interface.view(), static_cast<uint16_t>(modelState.port)));
        httpServer.onRequest = [&](HttpConnection& connection) { SC_ASSERT_RELEASE(handleRequest(connection)); };
        ::printf("[CanvasWS] started\n");
        ::fflush(stdout);
        return Result(true);
    }

    Result stop()
    {
        ::printf("[CanvasWS] stop activeClients=%zu activeConnections=%zu\n", webSocketHub.getNumClients(),
                 httpServer.getConnections().getNumActiveConnections());
        ::fflush(stdout);
        SC_TRY(httpServer.stop());
        SC_TRY(httpServer.close());
        SC_TRY(clients.clearAndDecommit());
        SC_TRY(webSocketHubClients.clearAndDecommit());
        SC_TRY(webSocketRuntimes.clearAndDecommit());
        ::printf("[CanvasWS] stopped\n");
        ::fflush(stdout);
        return Result(true);
    }

    Result runtimeResize()
    {
        const size_t numClients =
            max(static_cast<size_t>(modelState.maxClients), httpServer.getConnections().getHighestActiveConnection());
        SC_TRY(assignConnectionMemory(numClients));
        SC_TRY(httpServer.resize(clients.toSpan()));
        return Result(true);
    }

    Result assignConnectionMemory(size_t numClients)
    {
        normalizeConfiguration();
        SC_TRY(clients.resize(numClients));
        SC_TRY(webSocketHubClients.resize(numClients));
        SC_TRY(webSocketRuntimes.resize(numClients));
        SC_TRY(allReadQueues.resize(numClients * modelState.asyncConfiguration.readQueueSize));
        SC_TRY(allWriteQueues.resize(numClients * modelState.asyncConfiguration.writeQueueSize));
        SC_TRY(allBuffers.resize(numClients * modelState.asyncConfiguration.buffersQueueSize));
        SC_TRY(allHeaders.resize(numClients * modelState.asyncConfiguration.headerBytesLength));
        SC_TRY(allStreams.resize(numClients * modelState.asyncConfiguration.streamBytesLength));
        HttpConnectionsPool::Memory memory;
        memory.allBuffers    = allBuffers;
        memory.allReadQueue  = allReadQueues;
        memory.allWriteQueue = allWriteQueues;
        memory.allHeaders    = allHeaders;
        memory.allStreams    = allStreams;
        SC_TRY(memory.assignTo(modelState.asyncConfiguration, clients.toSpan()));
        SC_TRY(webSocketHub.init(webSocketHubClients.toSpan()));
        webSocketHub.onBroadcastFrame.bind<CollaborativeCanvasModel, &CollaborativeCanvasModel::writeBroadcastFrame>(
            *this);
        return Result(true);
    }

    Result writeBroadcastFrame(size_t clientIndex, Span<const char> encodedFrame)
    {
        if (clientIndex == currentBroadcastSourceHub)
        {
            return Result(true);
        }
        if (clientIndex >= webSocketHubClients.size())
        {
            return Result(true);
        }

        HttpWebSocketHubClient& client = webSocketHubClients.toSpan()[clientIndex];
        if (not client.active or not client.transport.isValid())
        {
            return Result(true);
        }

        AsyncBufferView::ID bufferID;
        Span<char>          writableData;
        Result              buffer =
            client.transport.buffersPool->requestNewBuffer(encodedFrame.sizeInBytes(), bufferID, writableData);
        if (not buffer)
        {
            droppedBroadcasts++;
            ::printf("[CanvasWS] drop broadcast targetHub=%zu bytes=%zu reason=%s dropped=%zu\n", clientIndex,
                     encodedFrame.sizeInBytes(), buffer.message, droppedBroadcasts);
            ::fflush(stdout);
            return Result(true);
        }

        ::memcpy(writableData.data(), encodedFrame.data(), encodedFrame.sizeInBytes());
        client.transport.buffersPool->setNewBufferSize(bufferID, encodedFrame.sizeInBytes());
        Result write = client.transport.writableStream->write(bufferID);
        client.transport.buffersPool->unrefBuffer(bufferID);
        if (not write)
        {
            droppedBroadcasts++;
            ::printf("[CanvasWS] drop broadcast targetHub=%zu bytes=%zu reason=%s dropped=%zu\n", clientIndex,
                     encodedFrame.sizeInBytes(), write.message, droppedBroadcasts);
            ::fflush(stdout);
            return Result(true);
        }
        return Result(true);
    }

    void normalizeConfiguration()
    {
        modelState.asyncConfiguration.readQueueSize =
            max(modelState.asyncConfiguration.readQueueSize, static_cast<size_t>(8));
        modelState.asyncConfiguration.writeQueueSize =
            max(modelState.asyncConfiguration.writeQueueSize, static_cast<size_t>(64));
        modelState.asyncConfiguration.buffersQueueSize =
            max(modelState.asyncConfiguration.buffersQueueSize, static_cast<size_t>(128));
    }

    bool canBeStarted() const { return eventLoop and modelState.port != 0 and not modelState.interface.isEmpty(); }

    Result handleRequest(HttpConnection& connection)
    {
        const StringSpan      target = connection.request.getURL();
        HttpRequestTargetView requestTarget;
        if (not requestTarget.parse(target))
        {
            return connection.sendTextCopy(400, "bad request\n");
        }
        const StringSpan path = requestTarget.path;

        ::printf("[CanvasWS] request conn=%zu method=%d target=%.*s path=%.*s\n",
                 connection.getConnectionID().getIndex(), static_cast<int>(connection.request.getParser().method),
                 static_cast<int>(target.sizeInBytes()), target.bytesWithoutTerminator(),
                 static_cast<int>(path.sizeInBytes()), path.bytesWithoutTerminator());
        ::fflush(stdout);

        if (path == "/ws")
        {
            return handleWebSocketRequest(connection);
        }
        if (connection.request.getParser().method != HttpParser::Method::HttpGET)
        {
            return connection.response.sendMethodNotAllowed("GET");
        }
        if (path == "/" or path == "/index.html" or path == "/canvas")
        {
            return connection.sendBodyCopy(200, canvasHtmlSpan(), HttpContentTypeTextHtmlUtf8());
        }
        if (path == "/health")
        {
            return connection.sendJsonCopy(200, "{\"status\":\"ok\"}");
        }
        return notFound(connection);
    }

    Result handleWebSocketRequest(HttpConnection& connection)
    {
        const size_t connectionIndex = connection.getConnectionID().getIndex();
        SC_TRY_MSG(connectionIndex < webSocketRuntimes.size(), "CollaborativeCanvas connection index out of range");

        ::printf("[CanvasWS] ws request conn=%zu activeClientsBefore=%zu\n", connectionIndex,
                 webSocketHub.getNumClients());
        ::fflush(stdout);

        WebSocketRuntime& runtime = webSocketRuntimes.toSpan()[connectionIndex];
        runtime.detach();
        runtime.owner      = this;
        runtime.connection = &connection;
        runtime.hubIndex   = size_t(-1);
        runtime.messageAssembler.reset(runtime.messageStorage);
        runtime.messageAssembler.onMessage.bind<WebSocketRuntime, &WebSocketRuntime::onMessage>(runtime);
        runtime.pump.onDataFramePayload.bind<WebSocketRuntime, &WebSocketRuntime::onPayload>(runtime);
        runtime.pump.onEnd.bind<WebSocketRuntime, &WebSocketRuntime::onEnd>(runtime);
        runtime.pump.onError.bind<WebSocketRuntime, &WebSocketRuntime::onError>(runtime);

        HttpWebSocketTransportView transport;
        char                       acceptStorage[HttpWebSocketHandshake::AcceptKeyLength] = {0};
        const auto                 validation = HttpWebSocketHandshake::validateServerRequest(connection.request);
        if (not validation.accepted())
        {
            ::printf("[CanvasWS] ws reject conn=%zu status=%d\n", connectionIndex, validation.httpStatusCode());
            ::fflush(stdout);
            return HttpWebSocketHandshake::rejectServerConnection(connection.response, validation);
        }

        SC_TRY(HttpWebSocketHandshake::acceptServerConnection(connection, transport, acceptStorage));
        SC_TRY(webSocketHub.join(transport, runtime.hubIndex));
        ::printf("[CanvasWS] ws accepted conn=%zu hub=%zu activeClients=%zu\n", connectionIndex, runtime.hubIndex,
                 webSocketHub.getNumClients());
        ::fflush(stdout);

        Result attachResult = runtime.pump.attach(transport, HttpWebSocketEndpointRole::Server);
        if (not attachResult)
        {
            (void)webSocketHub.leave(runtime.hubIndex);
            runtime.hubIndex = size_t(-1);
            return attachResult;
        }
        runtime.pump.getEndpoint().onFrameHeader.bind<WebSocketRuntime, &WebSocketRuntime::onFrameHeader>(runtime);
        runtime.pump.getEndpoint().onPing.bind<WebSocketRuntime, &WebSocketRuntime::onPing>(runtime);
        runtime.pump.getEndpoint().onClose.bind<WebSocketRuntime, &WebSocketRuntime::onClose>(runtime);
        return Result(true);
    }

    Result broadcastClear()
    {
        char frameStorage[128] = {0};
        ::printf("[CanvasWS] clear button activeClients=%zu\n", webSocketHub.getNumClients());
        ::fflush(stdout);
        Result broadcast = webSocketHub.broadcastText("{\"type\":\"clear\"}"_a8.toCharSpan(), frameStorage);
        ::printf("[CanvasWS] clear broadcast result=%s activeClients=%zu\n", broadcast ? "ok" : broadcast.message,
                 webSocketHub.getNumClients());
        ::fflush(stdout);
        return broadcast;
    }

    Result notFound(HttpConnection& connection) { return connection.sendTextCopy(404, "not found\n"); }

    Result saveToBinary(Buffer& modelStateBuffer)
    {
        return Result(SC::SerializationBinary::writeWithSchema(modelState, modelStateBuffer));
    }

    Result loadFromBinary(Span<const char> modelStateSpan)
    {
        return Result(SC::SerializationBinary::loadVersionedWithSchema(modelState, modelStateSpan));
    }

    static StringSpan canvasHtmlSpan()
    {
        static constexpr char canvasHtml[] = R"SC_CANVAS_HTML(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SCExample Collaborative Canvas</title>
    <style>
        :root {
            color-scheme: light;
            --ink: #14213d;
            --paper: #fff8ec;
            --accent: #fca311;
            --line: rgba(20, 33, 61, 0.18);
        }

        * {
            box-sizing: border-box;
        }

        body {
            margin: 0;
            min-height: 100vh;
            font-family: Georgia, "Times New Roman", serif;
            color: var(--ink);
            background:
                radial-gradient(circle at 15% 10%, rgba(252, 163, 17, 0.26), transparent 24rem),
                radial-gradient(circle at 85% 15%, rgba(42, 157, 143, 0.18), transparent 22rem),
                linear-gradient(135deg, #fff8ec 0%, #f7efe1 100%);
        }

        main {
            width: min(1100px, calc(100vw - 32px));
            margin: 0 auto;
            padding: 36px 0;
        }

        header {
            display: flex;
            align-items: end;
            justify-content: space-between;
            gap: 24px;
            margin-bottom: 18px;
        }

        h1 {
            margin: 0;
            font-size: clamp(2.4rem, 7vw, 5.8rem);
            line-height: 0.9;
            letter-spacing: -0.06em;
        }

        .status,
        button {
            border: 1px solid var(--line);
            border-radius: 999px;
            padding: 10px 14px;
            background: rgba(255, 255, 255, 0.58);
            font: inherit;
            font-size: 0.9rem;
            white-space: nowrap;
        }

        button {
            cursor: pointer;
        }

        .board {
            border: 1px solid var(--line);
            border-radius: 28px;
            overflow: hidden;
            background: rgba(255, 255, 255, 0.72);
            box-shadow: 0 24px 80px rgba(20, 33, 61, 0.16);
        }

        canvas {
            display: block;
            width: 100%;
            height: min(68vh, 720px);
            touch-action: none;
            cursor: crosshair;
            background:
                linear-gradient(var(--line) 1px, transparent 1px),
                linear-gradient(90deg, var(--line) 1px, transparent 1px),
                #fffdf8;
            background-size: 28px 28px;
        }

        .bar {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 14px;
            margin: 14px 4px 0;
        }

        .hint {
            margin: 0;
            font-size: 0.95rem;
            opacity: 0.72;
        }

        @media (max-width: 720px) {
            header,
            .bar {
                align-items: start;
                flex-direction: column;
            }

            canvas {
                height: 70vh;
            }
        }
    </style>
</head>
<body>
    <main>
        <header>
            <h1>SCExample Canvas</h1>
            <div class="status" id="status">Connecting...</div>
        </header>
        <section class="board">
            <canvas id="canvas"></canvas>
        </section>
        <div class="bar">
            <p class="hint">Open this page in another browser window and draw. The SCExample plugin relays every
                stroke through its WebSocket hub.</p>
            <button type="button" id="clear">Clear for everyone</button>
        </div>
    </main>

    <script>
        const canvas = document.getElementById("canvas");
        const status = document.getElementById("status");
        const clearButton = document.getElementById("clear");
        const ctx = canvas.getContext("2d");
        const color = `hsl(${Math.floor(Math.random() * 360)} 76% 42%)`;

        let socket;
        let drawing = false;
        let previous = null;
        let pendingStrokes = [];
        let flushScheduled = false;

        function resizeCanvas() {
            const rect = canvas.getBoundingClientRect();
            const ratio = window.devicePixelRatio || 1;
            const snapshot = document.createElement("canvas");
            snapshot.width = canvas.width;
            snapshot.height = canvas.height;
            snapshot.getContext("2d").drawImage(canvas, 0, 0);

            canvas.width = Math.max(1, Math.floor(rect.width * ratio));
            canvas.height = Math.max(1, Math.floor(rect.height * ratio));
            ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
            ctx.lineCap = "round";
            ctx.lineJoin = "round";
            ctx.drawImage(snapshot, 0, 0, snapshot.width / ratio, snapshot.height / ratio);
        }

        function clearCanvas() {
            ctx.save();
            ctx.setTransform(1, 0, 0, 1, 0, 0);
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            ctx.restore();
        }

        function drawStroke(stroke) {
            ctx.strokeStyle = stroke.color;
            ctx.lineWidth = stroke.width;
            ctx.beginPath();
            ctx.moveTo(stroke.previousX, stroke.previousY);
            ctx.lineTo(stroke.x, stroke.y);
            ctx.stroke();
        }

        function flushStrokes() {
            flushScheduled = false;
            if (!socket || socket.readyState !== WebSocket.OPEN || pendingStrokes.length === 0) {
                return;
            }

            const strokes = pendingStrokes;
            pendingStrokes = [];
            socket.send(JSON.stringify({ type: "strokes", strokes }));
        }

        function scheduleStrokeFlush() {
            if (!flushScheduled) {
                flushScheduled = true;
                requestAnimationFrame(flushStrokes);
            }
        }

        function sendStroke(point) {
            if (!previous || !socket || socket.readyState !== WebSocket.OPEN) {
                if (socket && socket.readyState !== WebSocket.OPEN) {
                    console.log("[CanvasWS] skip stroke, socket state", socket.readyState);
                }
                previous = point;
                return;
            }

            const stroke = {
                type: "stroke",
                previousX: previous.x,
                previousY: previous.y,
                x: point.x,
                y: point.y,
                color,
                width: 4
            };
            drawStroke(stroke);
            pendingStrokes.push(stroke);
            scheduleStrokeFlush();
            previous = point;
        }

        function pointFromEvent(event) {
            const rect = canvas.getBoundingClientRect();
            return {
                x: event.clientX - rect.left,
                y: event.clientY - rect.top
            };
        }

        function sendClear() {
            clearCanvas();
            if (socket && socket.readyState === WebSocket.OPEN) {
                flushStrokes();
                console.log("[CanvasWS] send clear");
                socket.send(JSON.stringify({ type: "clear" }));
            } else {
                console.log("[CanvasWS] clear local only, socket state", socket ? socket.readyState : "missing");
            }
        }

        function connect() {
            console.log("[CanvasWS] connecting");
            socket = new WebSocket(`${location.protocol === "https:" ? "wss" : "ws"}://${location.host}/ws`);
            socket.addEventListener("open", () => {
                console.log("[CanvasWS] open");
                status.textContent = "Connected";
            });
            socket.addEventListener("close", (event) => {
                console.log("[CanvasWS] close", { code: event.code, reason: event.reason, wasClean: event.wasClean });
                status.textContent = "Disconnected. Reconnecting...";
                setTimeout(connect, 800);
            });
            socket.addEventListener("error", (event) => {
                console.log("[CanvasWS] error", event);
            });
            socket.addEventListener("message", (event) => {
                try {
                    const message = JSON.parse(event.data);
                    if (message.type === "clear") {
                        console.log("[CanvasWS] apply clear");
                        clearCanvas();
                    } else if (message.type === "strokes") {
                        for (const stroke of message.strokes) {
                            drawStroke(stroke);
                        }
                    } else {
                        drawStroke(message);
                    }
                } catch {
                    status.textContent = "Ignored malformed canvas message";
                    console.log("[CanvasWS] malformed message", event.data);
                }
            });
        }

        canvas.addEventListener("pointerdown", (event) => {
            drawing = true;
            previous = pointFromEvent(event);
            canvas.setPointerCapture(event.pointerId);
        });
        canvas.addEventListener("pointermove", (event) => {
            if (drawing) {
                sendStroke(pointFromEvent(event));
            }
        });
        canvas.addEventListener("pointerup", () => {
            drawing = false;
            previous = null;
        });
        canvas.addEventListener("pointercancel", () => {
            drawing = false;
            previous = null;
        });
        clearButton.addEventListener("click", sendClear);

        window.addEventListener("resize", resizeCanvas);
        resizeCanvas();
        connect();
    </script>
</body>
</html>
)SC_CANVAS_HTML";
        return {{canvasHtml, sizeof(canvasHtml) - 1}, false, StringEncoding::Utf8};
    }
};

struct SC::CollaborativeCanvasView
{
    CollaborativeCanvasViewState viewState;

    Result saveToBinary(Buffer& viewStateBuffer)
    {
        return Result(SC::SerializationBinary::writeWithSchema(viewState, viewStateBuffer));
    }

    Result loadFromBinary(Span<const char> viewStateSpan)
    {
        return Result(SC::SerializationBinary::loadVersionedWithSchema(viewState, viewStateSpan));
    }

    bool InputSizeT(const char* name, size_t* num)
    {
        int intNum = static_cast<int>(*num);
        if (ImGui::InputInt(name, &intNum))
        {
            if (intNum < 0)
                intNum = 0;
            *num = static_cast<size_t>(intNum);
            return true;
        }
        return false;
    }

    Result draw(CollaborativeCanvasModel& model)
    {
        auto& buffer = viewState.inputTextBuffer;
        SC_TRY(InputText("Interface", buffer, model.modelState.interface, viewState.needsRestart));
        ImGui::SameLine();
        if (ImGui::Button("Listen on LAN"))
        {
            model.modelState.interface = "0.0.0.0";
            viewState.needsRestart     = model.httpServer.isStarted();
        }
        ImGui::PushItemWidth(130);
        viewState.needsRestart |= ImGui::InputInt("Port", &model.modelState.port);
        viewState.needsResize |= ImGui::InputInt("Max Clients", &model.modelState.maxClients);
        ImGui::Text("Per connection quantities (need restart)");
        auto& configuration = model.modelState.asyncConfiguration;
        viewState.needsRestart |= InputSizeT("Read Queue (items)", &configuration.readQueueSize);
        viewState.needsRestart |= InputSizeT("Write Queue (items)", &configuration.writeQueueSize);
        viewState.needsRestart |= InputSizeT("Buffers Queue (items)", &configuration.buffersQueueSize);
        viewState.needsRestart |= InputSizeT("Header buffer (bytes)", &configuration.headerBytesLength);
        viewState.needsRestart |= InputSizeT("Streams buffer (bytes)", &configuration.streamBytesLength);
        ImGui::PopItemWidth();

        ImGui::Separator();
        if (model.modelState.interface.view() == "0.0.0.0")
        {
            ImGui::Text("Local:  http://127.0.0.1:%d/", model.modelState.port);
            ImGui::Text("Remote: http://<this-computer-LAN-IP>:%d/", model.modelState.port);
        }
        else
        {
            ImGui::Text("Open http://%s:%d/", model.modelState.interface.view().bytesIncludingTerminator(),
                        model.modelState.port);
            ImGui::Text("Use 0.0.0.0 or the Listen on LAN button to allow other computers.");
        }
        ImGui::Text("Active Canvas Clients: %zu", model.webSocketHub.getNumClients());
        ImGui::Text("Broadcast Data Frames: %zu", model.totalFrames);
        ImGui::Text("Broadcast Payload Bytes: %zu", model.totalBytes);
        ImGui::Text("Automatic Control Frames: %zu", model.controlFrames);
        ImGui::Text("Total Connections : %zu", model.httpServer.getConnections().getNumTotalConnections());
        ImGui::Text("Active Connections: %zu", model.httpServer.getConnections().getNumActiveConnections());

        if (not model.httpServer.isStarted())
        {
            viewState.needsRestart = false;
        }

        if (viewState.needsResize)
        {
            ImGui::BeginDisabled(not model.canBeStarted());
            if (ImGui::Button("Resize"))
            {
                (void)model.runtimeResize();
                viewState.needsResize = false;
            }
            ImGui::EndDisabled();
        }
        else if (viewState.needsRestart)
        {
            ImGui::BeginDisabled(not model.canBeStarted());
            if (ImGui::Button("Apply Changes"))
            {
                (void)model.stop();
                (void)model.start();
                viewState.needsRestart = false;
            }
            ImGui::EndDisabled();
        }
        else
        {
            ImGui::BeginDisabled(not model.canBeStarted() or model.httpServer.isStarted());
            if (ImGui::Button("Start"))
            {
                (void)model.start();
            }
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(not model.httpServer.isStarted());
        if (ImGui::Button("Stop"))
        {
            (void)model.stop();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Browser Canvases"))
        {
            (void)model.broadcastClear();
        }
        ImGui::EndDisabled();
        return Result(true);
    }
};

struct CollaborativeCanvasExample : public SC::ISCExample
{
    SC::CollaborativeCanvasModel model;
    SC::CollaborativeCanvasView  view;

    CollaborativeCanvasExample()
    {
        ISCExample::onDraw.bind<CollaborativeCanvasExample, &CollaborativeCanvasExample::draw>(*this);
        ISCExample::serialize.bind<CollaborativeCanvasExample, &CollaborativeCanvasExample::serialize>(*this);
        ISCExample::deserialize.bind<CollaborativeCanvasExample, &CollaborativeCanvasExample::deserialize>(*this);
        ISCExample::initAsync.bind<CollaborativeCanvasExample, &CollaborativeCanvasExample::initAsync>(*this);
        ISCExample::closeAsync.bind<CollaborativeCanvasExample, &CollaborativeCanvasExample::closeAsync>(*this);
    }

    [[nodiscard]] bool init() { return true; }

    [[nodiscard]] bool close() { return model.stop(); }

    SC::Result initAsync(SC::AsyncEventLoop& eventLoop)
    {
        model.eventLoop = &eventLoop;
        return SC::Result(true);
    }

    SC::Result closeAsync(SC::AsyncEventLoop&) { return model.stop(); }

    void draw() { (void)view.draw(model); }

    SC::Result serialize(SC::Buffer& modelStateBuffer, SC::Buffer& viewStateBuffer)
    {
        SC_TRY(model.saveToBinary(modelStateBuffer));
        SC_TRY(view.saveToBinary(viewStateBuffer));
        return SC::Result(true);
    }

    SC::Result deserialize(SC::Span<const char> modelStateBuffer, SC::Span<const char> viewStateBuffer)
    {
        SC_TRY(model.loadFromBinary(modelStateBuffer));
        SC_TRY(view.loadFromBinary(viewStateBuffer));
        return SC::Result(true);
    }
};

SC_PLUGIN_DEFINE(CollaborativeCanvasExample)
SC_PLUGIN_EXPORT_INTERFACES(CollaborativeCanvasExample, SC::ISCExample)
