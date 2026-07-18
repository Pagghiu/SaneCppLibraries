// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// Async-stream HttpClient GET example using caller-owned operation memory and AsyncStreams queues.
//---------------------------------------------------------------------------------------------------------------------
// Usage: HttpClientAsyncGet <url>
//   Example: HttpClientAsyncGet https://example.com
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Async/Async.h"
#include "../../Libraries/AsyncStreams/AsyncStreams.h"
#include "../../Libraries/HttpClient/HttpClientAsync.h"
#include "../../Libraries/Strings/CommandLine.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/StringView.h"

#include <string.h>

namespace SC
{
using AsyncHttpClientOperation = HttpClientAsyncT<AsyncEventLoop, AsyncStreams>;

struct AsyncResponseCollector
{
    AsyncHttpClientOperation* operation = nullptr;
    Span<char>                body;

    size_t bodyLength = 0;
    bool   completed  = false;
    Result error      = Result(true);

    void onData(AsyncBufferView::ID bufferID)
    {
        Span<const char> data;
        if (not operation->getResponseBodyStream().getBuffersPool().getReadableData(bufferID, data))
        {
            error     = Result::Error("Failed to read response buffer");
            completed = true;
            (void)operation->cancel();
            return;
        }

        if (body.sizeInBytes() - bodyLength < data.sizeInBytes())
        {
            error     = Result::Error("Response body buffer too small");
            completed = true;
            (void)operation->cancel();
            return;
        }

        memcpy(body.data() + bodyLength, data.data(), data.sizeInBytes());
        bodyLength += data.sizeInBytes();
    }

    void onEnd() { completed = true; }

    void onError(Result newError)
    {
        error     = newError;
        completed = true;
    }
};

Result saneMain(Span<StringSpan> args)
{
    Console console;
    Console::tryAttachingToParentConsole();

    if (args.sizeInElements() < 1)
    {
        console.print("Usage: HttpClientAsyncGet <url>\n");
        console.print("  Example: HttpClientAsyncGet https://example.com\n");
        return Result::Error("Missing URL argument");
    }

    AsyncEventLoop loop;
    SC_TRY(loop.create());

    HttpClient client;
    SC_TRY(client.init());

    HttpClientResponseBuffer     responseBuffers[8];
    HttpClientOperationEvent     eventQueue[16];
    AsyncBufferView              asyncResponseBuffers[8];
    AsyncReadableStream::Request responseReadQueue[8];
    AsyncWritableStream::Request requestWriteQueue[4];

    char responseMemory[64 * 1024];
    char responseHeaders[8 * 1024];
    char responseMetadata[4 * 1024];
    char backendScratch[16 * 1024];

    HttpClientOperationMemory operationMemory;
    operationMemory.responseBuffers      = {responseBuffers, 8};
    operationMemory.responseBufferMemory = {responseMemory, sizeof(responseMemory)};
    operationMemory.eventQueue           = {eventQueue, 16};
    operationMemory.responseHeaders      = {responseHeaders, sizeof(responseHeaders)};
    operationMemory.responseMetadata     = {responseMetadata, sizeof(responseMetadata)};
    operationMemory.backendScratch       = {backendScratch, sizeof(backendScratch)};

    HttpClientAsyncOperationMemoryT<AsyncStreams> asyncMemory;
    asyncMemory.responseBuffers      = {asyncResponseBuffers, 8};
    asyncMemory.responseBufferMemory = {responseMemory, sizeof(responseMemory)};
    asyncMemory.responseReadQueue    = {responseReadQueue, 8};
    asyncMemory.requestWriteQueue    = {requestWriteQueue, 4};

    AsyncHttpClientOperation operation;
    SC_TRY(operation.init(client, loop, operationMemory, asyncMemory));

    HttpClientRequest request;
    request.url                               = args[0];
    request.options.timeouts.requestTimeoutMs = 30000;

    HttpClientResponse     response;
    char                   body[256 * 1024];
    AsyncResponseCollector collector;
    collector.operation = &operation;
    collector.body      = {body, sizeof(body)};

    const bool dataAdded =
        operation.getResponseBodyStream()
            .eventData.addListener<AsyncResponseCollector, &AsyncResponseCollector::onData>(collector);
    const bool endAdded =
        operation.getResponseBodyStream().eventEnd.addListener<AsyncResponseCollector, &AsyncResponseCollector::onEnd>(
            collector);
    const bool errorAdded =
        operation.getResponseBodyStream()
            .eventError.addListener<AsyncResponseCollector, &AsyncResponseCollector::onError>(collector);
    SC_TRY_MSG(dataAdded and endAdded and errorAdded, "Failed to register async response listeners");

    SC_TRY(operation.start(request, response));
    while (not collector.completed)
    {
        SC_TRY(loop.runOnce());
    }

    SC_TRY(collector.error);

    console.print("Status: {}\n", response.statusCode);
    console.print("Protocol: {}\n", response.getProtocolName());
    console.print("Effective URL: {}\n", StringView(response.effectiveUrl));
    console.print("Body ({} bytes):\n", collector.bodyLength);
    console.print("{}\n", StringView({body, collector.bodyLength}, false, StringEncoding::Ascii));

    SC_TRY(operation.close());
    SC_TRY(client.close());
    SC_TRY(loop.close());
    return Result(true);
}
} // namespace SC

int main(int argc, char** argv)
{
    using namespace SC;
    constexpr auto       NumArgsMax = 10;
    StringSpan           argumentStorage[NumArgsMax];
    CommandLineArguments arguments;
    if (not arguments.setFromMainArguments(argc, argv, argumentStorage))
    {
        return -1;
    }
    return SC::saneMain(arguments.values) ? 0 : -1;
}
