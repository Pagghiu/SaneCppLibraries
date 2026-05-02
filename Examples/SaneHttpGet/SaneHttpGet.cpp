// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A simple command-line HTTP GET tool using the HttpClient library.
// Similar to curl/wget but uses native OS networking stacks.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Execute `BuildAndRun.{bat|sh}` to quickly try this example.
// (Alternatively) Run `./SC.sh build configure` from repo root to generate IDE projects in _Build/_Projects.
//---------------------------------------------------------------------------------------------------------------------
// Usage: SaneHttpGet <url>
//   Example: SaneHttpGet https://example.com
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/HttpClient/HttpClient.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/StringView.h"

namespace SC
{
Result saneMain(Span<StringSpan> args)
{
    Console console;
    Console::tryAttachingToParentConsole();

    if (args.sizeInElements() < 1)
    {
        console.print("Usage: SaneHttpGet <url>\n");
        console.print("  Example: SaneHttpGet https://example.com\n");
        return Result::Error("Missing URL argument");
    }

    const StringSpan urlArg = args[0];

    console.print("GET {}\n", urlArg);

    HttpClientRequest request;
    request.url = urlArg;

    request.options.timeouts.requestTimeoutMs = 30000;

    HttpClientResponse       response;
    HttpClientResponseBuffer responseBuffers[16];
    HttpClientOperationEvent eventQueue[32];

    size_t bodyLength = 0;

    char bodyBuf[256 * 1024]; // 256 KB max response body
    char responseMemory[64 * 1024];
    char headerBuf[8 * 1024];
    char metadataBuf[4 * 1024];
    char backendScratch[16 * 1024];

    HttpClientOperationMemory operationMemory;
    operationMemory.responseBuffers      = {responseBuffers, 16};
    operationMemory.responseBufferMemory = {responseMemory, sizeof(responseMemory)};
    operationMemory.eventQueue           = {eventQueue, 32};
    operationMemory.responseHeaders      = {headerBuf, sizeof(headerBuf)};
    operationMemory.responseMetadata     = {metadataBuf, sizeof(metadataBuf)};
    operationMemory.backendScratch       = {backendScratch, sizeof(backendScratch)};

    Result res =
        HttpClient::executeBlocking(request, response, {bodyBuf, sizeof(bodyBuf)}, bodyLength, operationMemory);

    if (not res)
    {
        console.print("Error: request failed\n");
        return res;
    }

    console.print("Status: {}\n", response.statusCode);
    console.print("Effective URL: {}\n", StringView(response.effectiveUrl));
    console.print("Headers ({} bytes):\n", response.headersLength);

    StringView headers({headerBuf, response.headersLength}, false, StringEncoding::Ascii);
    console.print("{}\n", headers);

    console.print("Body ({} bytes):\n", bodyLength);
    StringView body({bodyBuf, bodyLength}, false, StringEncoding::Ascii);
    console.print("{}\n", body);

    return Result(true);
}
} // namespace SC

int main(int argc, char** argv)
{
    using namespace SC;
    constexpr auto NUM_ARGS_MAX = 10;
    StringSpan     args[NUM_ARGS_MAX];
    for (int idx = 1; idx < min(argc, NUM_ARGS_MAX); ++idx)
        args[idx - 1] = StringSpan::fromNullTerminated(argv[idx], StringEncoding::Utf8);
    return SC::saneMain({args, static_cast<size_t>(min(argc - 1, NUM_ARGS_MAX))}) ? 0 : -1;
}
