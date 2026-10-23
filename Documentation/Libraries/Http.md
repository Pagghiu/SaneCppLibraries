@page library_http Http

@brief 🟥 Build HTTP/1.1 clients and servers with caller-owned storage

[TOC]

`Http` lets an SC application receive HTTP requests, send responses, and make requests of its own. Use it to expose
a small API, serve files, or exchange streamed data while keeping control of connection capacity and memory use.
It runs on the SC asynchronous event loop and uses AsyncStreams for message bodies.

For example, a local service can accept a request for `/health`, return a JSON response, and reuse its connection
storage for the next client. An upload handler can process the incoming body in pieces instead of allocating enough
memory for the entire file. Your application chooses the limits and handles errors when they are reached.

The API is 🟥 **Draft**. It provides HTTP/1.1 mechanisms; application policy such as authentication, retries and
redirect following remains yours. Start with the runnable example and basic lifecycle below. Storage limits,
protocol helpers and deployment details follow later.

# How it works

An HTTP exchange has a request and a response. Each has headers describing the message and an optional body.
Headers arrive before the body is necessarily complete, so the library exposes bodies as streams.

| Piece | What it does |
| --- | --- |
| `HttpAsyncServer` | Accepts connections and calls your handler when request headers are available |
| `HttpConnection` | Gives a server handler access to the current request and response |
| `HttpAsyncClient` | Sends one request at a time and reports response headers through a callback |
| `AsyncEventLoop` | Drives socket operations and asynchronous completion callbacks |
| Caller-owned connection storage | Holds header memory, stream buffers and request queues |

On the server, your handler reads `connection.request` and writes `connection.response`. On the client, you start a
request and consume the body from `HttpAsyncClientResponse`. Neither starting a request nor receiving headers means
that the whole exchange has finished.

The library does not allocate connections or grow buffers behind the scenes. You provide storage before starting
work and keep it alive until shutdown completes. This makes capacity explicit without requiring you to assemble
every message in memory.

# Try the API server

`Examples/ApiServer/ApiServer.cpp` is a runnable starting point. From the repository root:

@code{.sh}
./SC.sh build configure ApiServer
./SC.sh build compile ApiServer Debug
./SC.sh build run ApiServer Debug -- --port 8091
@endcode

Use `SC.bat` instead of `./SC.sh` on Windows. The example listens on `127.0.0.1`. In another terminal, try:

@code{.sh}
curl http://127.0.0.1:8091/health
curl 'http://127.0.0.1:8091/hello?name=SaneCpp'
curl --data 'Hello from the client' http://127.0.0.1:8091/echo
@endcode

`/health` returns a JSON status, `/hello` uses a query parameter, and `/echo` streams the request body back to the
client. Stop the example process when finished. The application shutdown sequence is explained below.

Read this example alongside the following fragments: it shows event-loop setup, route selection and body handling
in one application. The fragments explain the API; they are not standalone programs.

# Build a small server

Assume socket networking and an `AsyncEventLoop` named `loop` are initialized. In a function returning `Result`,
provide connection storage, initialize the server and install a request handler before running the loop:

@code{.cpp}
using Connection = SC::HttpAsyncConnection<2, 2, 8 * 1024, 1024>;
Connection connections[3];
SC::HttpAsyncServer server;
SC_TRY(server.init(SC::Span<Connection>(connections)));

server.onRequest = [&server](SC::HttpConnection& connection)
{
    const SC::Result sent = connection.sendTextCopy(200, "Hello from SC\n");
    if (not sent and server.onError.isValid())
    {
        server.onError(sent);
    }
};
server.onError = [](SC::Result error)
{
    // Record the error and apply your application's failure policy.
};

SC_TRY(server.start(loop, "127.0.0.1", 8091));
SC_TRY(loop.run());
@endcode

The array allows three concurrent connections. Later clients reuse those slots; this is not a three-request limit.
The template arguments choose read/write queue capacity, header space and stream storage per connection.

The handler sends a short response using `sendTextCopy()`. The copy goes into the connection's fixed buffer pool,
so temporary response text need not survive after the handler returns. Copying can fail when that pool is full.
For larger responses, write to `connection.response.getWritableStream()` and retain the source data until its write
finishes.

To handle different endpoints, inspect `connection.request.getParser().method` and
`connection.request.getRequestTarget()`, or use the router shown in the API server example. When a request has a
body, consume or pipe its readable stream, or deliberately reject the request; headers alone do not complete it.

Arrange an application event to request shutdown. The listener otherwise keeps the event loop active:

@code{.cpp}
// From your application's stop event:
SC_TRY(server.stop());

// After loop.run() returns, outside callbacks:
SC_TRY(server.close());
SC_TRY(loop.run());
@endcode

`stop()` begins shutdown; `close()` waits for server work to finish. Keep the connection array, server and callback
targets alive through cleanup. Close the event loop only after all other users of it have also finished.

# Make a request and read its body

Use `HttpAsyncClient` when you want to make requests using this library's HTTP parser and stream model.
The client processes one request at a time. Convenience methods cover bodyless requests and fixed-span PUT/POST/PATCH;
`start()` plus `onPrepareRequest` is the lower-level path for streamed or manually written bodies. Response bodies are
always consumed as streams, even when the application chooses to collect them.

Before the tested excerpt, the application creates an `HttpAsyncClientConnection<...>`, passes it to `client.init()`,
and builds the URL in caller-owned storage. The `ResponseCollector` shown here attaches stream listeners and invokes its
completion callback on `eventEnd`; an allocation-free application would collect into a fixed span or process each data
event directly. Run the event loop after starting the request, then close the client before reclaiming its connection
storage.

@snippet Tests/Libraries/Http/HttpAsyncClientTest.cpp HttpAsyncClientBasicSnippet

Sequential keep-alive reuse is supported for the same origin. A different origin reconnects; client pipelining is
not supported. Redirect handling is application policy.

Optional gzip/deflate response decoding is also a composition: the client inserts AsyncStreams transform streams when
enabled, while the caller still consumes the decoded readable stream and provides the fixed storage used by the
connection.

Follow the exchange in four steps:

1. Initialize the client with caller-owned connection storage and install response/error callbacks.
2. Call `get()` or another request method, then run the event loop.
3. Consume response data from the readable stream. Its `eventEnd` marks the end of the body; handle stream errors too.
4. Close the client and drain pending cancellation before reclaiming its storage.

The collector in the excerpt is a test helper, not a required library object. Your application can process chunks
as they arrive, pipe the stream to another consumer, or collect into a bounded buffer of its own.

# Understand message and buffer lifetimes

The parser consumes whatever bytes are currently available and reports tokens as soon as they are complete. Headers do
not require the entire message body to be resident, and bodies remain streams. This is the same model at every level:

- `HttpRequest` and `HttpAsyncClientResponse` are incoming messages. They expose parsed headers and an
  `AsyncReadableStream` for the body.
- `HttpResponse` and `HttpAsyncClientRequest` are outgoing messages. They build headers in fixed storage and expose an
  `AsyncWritableStream` for the body.
- `HttpConnectionBase` holds the active streams and buffer pool. Server-side `HttpConnection` adds the request/response
  pair; `HttpAsyncClient` owns the inverse request/response pair over caller-provided connection storage.

This symmetry makes streaming explicit. Receiving response headers is not request completion: attach body listeners to
`HttpAsyncClientResponse::getReadableStream()` and treat its `eventEnd` as completion. Likewise, a server callback may
start a response while the request body is still arriving; it must consume, pipe, or deliberately reject that body.

The incremental design has a cost. Parsed URL and header views exposed by an active connection point into its header
storage. They are meaningful only for the current message and must not be retained after the connection is reused. Spans
passed to asynchronous writes must also remain alive until the write completes. Where stack-owned response text cannot
meet that lifetime, use the `HttpConnection::sendBodyCopy`, `sendTextCopy`, or `sendJsonCopy` helpers; these copy into
the connection's fixed buffer pool and can fail when that pool is full.

# Add routing or file serving

For an API server, place `HttpRouter` in the callback to match a method and path template without allocation. It is a
small dispatcher, not a framework: middleware, request contexts, authorization, and response schemas remain application
code. `Examples/ApiServer/ApiServer.cpp` is the more representative source when evaluating that shape.

For files, `HttpAsyncFileServer` composes with `HttpAsyncServer` and the File/Threading stack. It streams GET responses
and uploads, and supports ranges, validators, MIME lookup, upload limits, and optional SPA fallback. Its root directory,
option strings, connection storage, and per-request stream objects are caller-owned; it is convenient infrastructure,
not a hardened reverse proxy.

# Choose memory limits

`HttpAsyncConnection<ReadQueue, WriteQueue, HeaderBytes, StreamBytes>` and
`HttpAsyncClientConnection<ReadQueue, WriteQueue, HeaderBytes, StreamBytes>` make the common configuration a type-level
choice. On the server, the span passed to `HttpAsyncServer::init()` fixes maximum concurrent connections. Each element
contains its own read queue, write queue, stream storage, and shared request/response header space.

These limits are behavior, not tuning hints:

- a request or response whose headers exceed available header storage fails;
- a burst that cannot fit the fixed async queues applies backpressure or reports an error;
- full-message assembly, decompression output, multipart field collection, and WebSocket message assembly need explicit
  caller storage when requested;
- increasing concurrent connection count multiplies the per-connection storage chosen by the application.

The library reports these conditions through structured `Result` identities; it does not silently truncate and does
not throw. Inspect the library-owned error enum to distinguish conditions, and opt into `HttpErrorFormatter.h` only
when a caller-storage diagnostic is needed.

# Protocol helpers and supported boundaries

The library includes focused pieces that can also be used below the client/server surfaces:

- `HttpParser` incrementally tokenizes HTTP/1.x request or response headers.
- `HttpURLParser`, `HttpRequestTargetView`, and query/form iterators return zero-copy raw slices; percent/form decoding
  writes into caller-provided storage.
- `HttpHeaders` helpers parse and format common cookie, authorization, cache-control, and content-type values.
- `HttpMultipartParser` parses streamed multipart bodies, while `HttpMultipartWriter` emits validated multipart request
  content.
- `HttpRouter` matches methods and path parameters against caller-owned route tables and parameter storage.
- `HttpWebSocketHandshake`, frame reader/writer, endpoint, stream pump, and small hub cover RFC 6455 handshakes, framing,
  control frames, and bounded fan-out without turning WebSockets into a separate networking runtime.

Unsupported input is generally rejected explicitly. Current boundaries include one in-flight client request, no HTTP
pipelining or client redirect-following policy, no HTTP/2 or HTTP/3, no WebSocket extension negotiation, and limited
trailer support. Server responses do have a checked `HttpResponse::sendRedirect` formatting helper.
DNS and sockets belong to `Socket`; asynchronous byte ownership and backpressure belong to `AsyncStreams`. Keeping those seams visible avoids pulling transport and policy concerns into the HTTP message layer,
but it also means an application integrates more pieces itself.

## Choosing the WebSocket SHA-1 provider

RFC 6455 requires SHA-1 to derive `Sec-WebSocket-Accept`. This use proves that the server received the client's opening
key; it does not make SHA-1 suitable for passwords, signatures, certificates, or other security decisions.

Handshake helpers default to `HttpWebSocketSha1Mode::Platform`. A caller can explicitly select the allocation-free
self-contained implementation instead:

```cpp
char acceptStorage[SC::HttpWebSocketHandshake::AcceptKeyLength];
SC::StringSpan accept;
SC_TRY(SC::HttpWebSocketHandshake::computeAccept(clientKey, acceptStorage, accept,
                                                  SC::HttpWebSocketSha1Mode::SelfContained));
```

| Platform | `Platform` provider order | Behavior when unavailable |
|---|---|---|
| Windows | CNG from `bcrypt.dll` | Returns an error |
| macOS | CommonCrypto | Returns an error |
| Linux | Dynamically loaded OpenSSL EVP, then AF_ALG | Returns an error when both are unavailable |

`Platform` adds no link-time dependency. Linux probes `libcrypto.so.3` and then `libcrypto.so.1.1`; if neither exposes
the required public EVP functions, it tries AF_ALG. `SelfContained` never loads a provider and is useful for deployments
that prefer one predictable binary over provider updates. Its implementation is tested against standard SHA-1 vectors,
padding/block boundaries, chunked input, and the independent platform result. The known-answer cases come from
[FIPS PUB 180-4](https://doi.org/10.6028/NIST.FIPS.180-4).

# Advanced transport integration

A transport adapter can also install a preflight hook. It runs after URL parsing but before DNS resolution or socket
activity, allowing transport-specific URL policy to fail without opening a connection. The later setup hook still owns
stream replacement and asynchronous transport readiness after TCP connects.

Transport adapters claim the complete callback group with a non-null, address-stable owner token. Every hook setter
returns `Result` and accepts that token; another adapter or an unowned setter cannot replace claimed hooks.
`clearTransportSetup(owner)` retains the claim; `releaseTransport(owner)` clears and releases it. Configure or release
only after requests, keep-alive connections and pending cancellation have fully retired, never inside a transport
callback. A server must be fully stopped, not merely stopping. Existing unowned setters use the default null token,
but their results must now be checked. The ownership decision is recorded in
`Architecture/Http/http-0008-own-transport-hooks-as-an-exclusive-group.md`.

# Operational notes

`HttpAsyncServer::stop()` begins asynchronous shutdown; `close()` waits for outstanding work and releases references to
caller memory. The same ownership rule applies to the client: close it before destroying its connection storage. For
servers, set an explicit maximum header size and bound uploads and keep-alive request counts for the deployment.

The focused executable examples are useful complements to the snippets:

- `Examples/ApiServer` shows method/path routing and streaming echo responses.
- `Examples/AsyncWebServer` shows static files, uploads, and WebSocket connection pumping.
- [SCExample](@ref page_examples) contains the GUI-integrated `WebServerExample`.

The implementation is still marked draft. Before adopting it for an exposed service, evaluate the supported protocol
surface against your threat model and put a production proxy in front where appropriate. The bounded
`HttpStressTest` exercises repeated keep-alive requests, chunked request bursts, and WebSocket fan-out; it is a regression
signal, not a claim of protocol conformance or internet hardening.

# Dependencies

- Dependencies: [Async](@ref library_async), [AsyncStreams](@ref library_async_streams)
- All dependencies: [Async](@ref library_async), [AsyncStreams](@ref library_async_streams), [File](@ref library_file), [FileSystem](@ref library_file_system), [Socket](@ref library_socket), [Threading](@ref library_threading)

![Dependency Graph](Http.svg)

[SaneCppHttp.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppHttp.h) packages the library as a single-file distribution.

# Further material

- [Ep.27 - C++ Async Http Web Server](https://www.youtube.com/watch?v=yg438A9Db50)
- [August 2024 update](https://pagghiu.github.io/site/blog/2024-08-30-SaneCppLibrariesUpdate.html)
- [September 2025 update](https://pagghiu.github.io/site/blog/2025-09-30-SaneCppLibrariesUpdate.html)
- [November 2025 update](https://pagghiu.github.io/site/blog/2025-11-30-SaneCppLibrariesUpdate.html)
- [December 2025 update](https://pagghiu.github.io/site/blog/2025-12-31-SaneCppLibrariesUpdate.html)
- [January 2026 update](https://pagghiu.github.io/site/blog/2026-01-31-SaneCppLibrariesUpdate.html)
- [February 2026 update](https://pagghiu.github.io/site/blog/2026-02-28-SaneCppLibrariesUpdate.html)
- [March 2026 update](https://pagghiu.github.io/site/blog/2026-03-31-SaneCppLibrariesUpdate.html)
- [May 2026 update](https://pagghiu.github.io/site/blog/2026-05-31-SaneCppLibrariesUpdate.html)
- [June 2026 update](https://pagghiu.github.io/site/blog/2026-06-30-SaneCppLibrariesUpdate.html)
- [September 2026 Update](https://pagghiu.github.io/site/blog/2026-09-30-SaneCppLibrariesUpdate.html)

# API reference

The reference is intentionally secondary to the model and examples above.

## HttpAsyncServer

@copydoc SC::HttpAsyncServer

## HttpAsyncFileServer

@copydoc SC::HttpAsyncFileServer

## HttpAsyncClient

@copydoc SC::HttpAsyncClient

# Statistics

LOC counts exclude comments. Library counts files physically under `Libraries/Http`.
Single File counts
`SaneCppHttp.h`.
Standalone counts `SaneCppHttpStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 2500		| 8403		| 10903	|
| Single File | 3427		| 8824		| 12251	|
| Standalone  | 13988		| 25709		| 39697	|
