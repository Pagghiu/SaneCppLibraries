// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// Poll-driven HttpClient example using caller-owned operation memory, HttpClientSession, and
// HttpClientOperationScheduler.
//---------------------------------------------------------------------------------------------------------------------
// Usage: HttpClientPollSession <url>
//   Example: HttpClientPollSession https://example.com
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/HttpClient/HttpClient.h"
#include "../../Libraries/HttpClient/HttpClientScheduler.h"
#include "../../Libraries/HttpClient/HttpClientSession.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/StringView.h"

#include <string.h>

namespace SC
{
struct ResponseCollector final : public HttpClientOperationListener
{
    Span<char> body;
    size_t     bodyLength = 0;
    bool       completed  = false;
    Result     error      = Result(true);

    virtual void onResponseBody(Span<const char> data) override
    {
        if (body.sizeInBytes() - bodyLength < data.sizeInBytes())
        {
            error = Result::Error("Response body buffer too small");
            return;
        }
        memcpy(body.data() + bodyLength, data.data(), data.sizeInBytes());
        bodyLength += data.sizeInBytes();
    }

    virtual void onResponseComplete() override { completed = true; }
    virtual void onError(Result newError) override { error = newError; }
};

Result saneMain(Span<StringSpan> args)
{
    Console console;
    Console::tryAttachingToParentConsole();

    if (args.sizeInElements() < 1)
    {
        console.print("Usage: HttpClientPollSession <url>\n");
        console.print("  Example: HttpClientPollSession https://example.com\n");
        return Result::Error("Missing URL argument");
    }

    //! [HttpClientPollRequestSnippet]
    HttpClient client;
    SC_TRY(client.init());

    HttpClientResponseBuffer responseBuffers[8];
    HttpClientOperationEvent eventQueue[16];
    char                     responseMemory[64 * 1024];
    char                     responseHeaders[8 * 1024];
    char                     responseMetadata[4 * 1024];
    char                     backendScratch[16 * 1024];

    HttpClientOperationMemory operationMemory;
    operationMemory.responseBuffers      = {responseBuffers, 8};
    operationMemory.responseBufferMemory = {responseMemory, sizeof(responseMemory)};
    operationMemory.eventQueue           = {eventQueue, 16};
    operationMemory.responseHeaders      = {responseHeaders, sizeof(responseHeaders)};
    operationMemory.responseMetadata     = {responseMetadata, sizeof(responseMetadata)};
    operationMemory.backendScratch       = {backendScratch, sizeof(backendScratch)};

    HttpClientOperation operation;
    SC_TRY(operation.init(client, operationMemory));

    HttpClientOperation* schedulerOperations[] = {&operation};
    uint8_t              readyOperations[1]    = {};

    HttpClientOperationSchedulerMemory schedulerMemory;
    schedulerMemory.operations      = {schedulerOperations, 1};
    schedulerMemory.readyOperations = {readyOperations, 1};

    HttpClientOperationScheduler scheduler;
    SC_TRY(scheduler.init(schedulerMemory));

    HttpClientSessionCookie         cookies[4];
    HttpClientSessionAuthCacheEntry authEntries[2];
    HttpClientHeader                sessionHeaders[4];
    char                            sessionHeaderScratch[256];
    char                            sessionStateScratch[512];

    HttpClientSessionMemory sessionMemory;
    sessionMemory.cookies        = {cookies, 4};
    sessionMemory.authEntries    = {authEntries, 2};
    sessionMemory.requestHeaders = {sessionHeaders, 4};
    sessionMemory.headerScratch  = {sessionHeaderScratch, sizeof(sessionHeaderScratch)};
    sessionMemory.stateScratch   = {sessionStateScratch, sizeof(sessionStateScratch)};

    HttpClientSession session;
    SC_TRY(session.init(sessionMemory));

    HttpClientHeader  requestHeaders[] = {{StringSpan("User-Agent"), StringSpan("SaneCpp-HttpClientPollSession")}};
    HttpClientRequest request;
    request.url                               = args[0];
    request.headers                           = {requestHeaders, 1};
    request.options.timeouts.requestTimeoutMs = 30000;

    HttpClientRequest preparedRequest;
    SC_TRY(session.prepareRequest(request, preparedRequest));

    HttpClientResponse response;
    char               body[256 * 1024];
    ResponseCollector  collector;
    collector.body = {body, sizeof(body)};

    SC_TRY(operation.start(preparedRequest, response, &collector));

    while (scheduler.hasRequestsInFlight())
    {
        size_t numPolled = 0;
        SC_TRY(scheduler.pollReady(numPolled, 100));
    }
    //! [HttpClientPollRequestSnippet]

    SC_TRY(collector.error);
    SC_TRY_MSG(collector.completed, "Response did not complete");
    SC_TRY(session.captureResponse(preparedRequest, response));

    console.print("Status: {}\n", response.statusCode);
    console.print("Protocol: {}\n", response.getProtocolName());
    console.print("Effective URL: {}\n", StringView(response.effectiveUrl));
    console.print("Cookies captured: {}\n", session.getNumCookies());
    console.print("Body ({} bytes):\n", collector.bodyLength);
    console.print("{}\n", StringView({body, collector.bodyLength}, false, StringEncoding::Ascii));

    SC_TRY(scheduler.close());
    SC_TRY(operation.close());
    SC_TRY(client.close());
    return Result(true);
}
} // namespace SC

int main(int argc, char** argv)
{
    using namespace SC;
    constexpr auto NumArgsMax = 10;
    StringSpan     args[NumArgsMax];
    for (int idx = 1; idx < min(argc, NumArgsMax); ++idx)
    {
        args[idx - 1] = StringSpan::fromNullTerminated(argv[idx], StringEncoding::Utf8);
    }
    return SC::saneMain({args, static_cast<size_t>(min(argc - 1, NumArgsMax))}) ? 0 : -1;
}
