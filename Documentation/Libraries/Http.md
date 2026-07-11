@page library_http Http

@brief 🟥 Allocation-free HTTP/1.1 client, server, parser, and WebSocket building blocks

[TOC]

`Http` is the low-level HTTP/1.1 stack for applications already built around SC's asynchronous event loop and streams.
It provides an incremental parser, an asynchronous client and server, static-file serving, routing and header helpers,
multipart processing, and WebSocket handshake and framing support. The implementation does not allocate: connection
capacity, stream queues, header space, and body buffering are chosen and owned by the caller.

That control is the main reason to choose this library. It is a good fit when memory bounds and transport integration
matter more than a high-level, batteries-included HTTP API. It is not currently a general replacement for a mature
internet-facing HTTP stack: applications must supply policy such as redirects, authentication, retries, request limits,
and security hardening, and some valid protocol features are deliberately rejected rather than partially handled.

[SaneCppHttp.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppHttp.h) packages the library
as a single-file distribution.

# Dependencies
- Dependencies: [Async](@ref library_async), [AsyncStreams](@ref library_async_streams)
- All dependencies: [Async](@ref library_async), [AsyncStreams](@ref library_async_streams), [File](@ref library_file), [FileSystem](@ref library_file_system), [Socket](@ref library_socket), [Threading](@ref library_threading)

![Dependency Graph](Http.svg)


# The model: messages moving through fixed storage

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

# Choosing memory limits

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

The library reports these conditions through `Result`; it does not silently truncate and does not throw. Error messages
usually name the layer enforcing the invariant (`HttpIncomingMessage`, `HttpAsyncClient`, `HttpWebSocketFrameReader`,
and so on), which is more useful here than a large exception hierarchy.

# Representative server

The server owns no connection allocation. This tested example fixes capacity at three connections and sizes each
connection's queues and storage at compile time. The callback reads the parsed request and writes the response through
that connection; shutdown must finish before the connection array is reclaimed. The example deliberately uses an
application-owned `String` for its response body to exercise asynchronous span lifetime. That allocation is in the test
application, not the server; a bounded application can instead write caller-owned fixed storage or use the connection's
fixed-pool copy helpers.

@snippet Tests/Libraries/Http/HttpAsyncServerTest.cpp HttpAsyncServerSnippet

For an API server, place `HttpRouter` in the callback to match a method and path template without allocation. It is a
small dispatcher, not a framework: middleware, request contexts, authorization, and response schemas remain application
code. `Examples/ApiServer/ApiServer.cpp` is the more representative source when evaluating that shape.

For files, `HttpAsyncFileServer` composes with `HttpAsyncServer` and the File/Threading stack. It streams GET responses
and uploads, and supports ranges, validators, MIME lookup, upload limits, and optional SPA fallback. Its root directory,
option strings, connection storage, and per-request stream objects are caller-owned; it is convenient infrastructure,
not a hardened reverse proxy.

# Representative client

The client processes one request at a time. Convenience methods cover bodyless requests and fixed-span PUT/POST/PATCH;
`start()` plus `onPrepareRequest` is the lower-level path for streamed or manually written bodies. Response bodies are
always consumed as streams, even when the application chooses to collect them.

Before the tested excerpt, the application creates an `HttpAsyncClientConnection<...>`, passes it to `client.init()`,
and builds the URL in caller-owned storage. The `ResponseCollector` shown here attaches stream listeners and invokes its
completion callback on `eventEnd`; an allocation-free application would collect into a fixed span or process each data
event directly. Run the event loop after starting the request, then close the client before reclaiming its connection
storage.

@snippet Tests/Libraries/Http/HttpAsyncClientTest.cpp HttpAsyncClientBasicSnippet

Sequential keep-alive reuse is supported for the same origin. A different origin reconnects; pipelining is not
supported. Redirect handling is application policy. Plain `http` uses the socket streams directly. An `https` URL is
rejected unless a transport setup adapter is installed; the neighboring `Https` library supplies
the TLS composition without putting TLS inside the HTTP message layer.

Optional gzip/deflate response decoding is also a composition: the client inserts AsyncStreams transform streams when
enabled, while the caller still consumes the decoded readable stream and provides the fixed storage used by the
connection.

# Protocol helpers and deliberate boundaries

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
TLS belongs to `Https`; DNS and sockets belong to `Socket`; asynchronous byte ownership and backpressure belong to
`AsyncStreams`. Keeping those seams visible avoids pulling transport and policy concerns into the HTTP message layer,
but it also means an application integrates more pieces itself.

# Operational notes

`HttpAsyncServer::stop()` begins asynchronous shutdown; `close()` waits for outstanding work and releases references to
caller memory. The same ownership rule applies to the client: close it before destroying its connection storage. For
servers, set an explicit maximum header size and bound uploads and keep-alive request counts for the deployment.

The focused executable examples are useful complements to the snippets:

- `Examples/ApiServer` shows method/path routing and streaming echo responses.
- `Examples/AsyncWebServer` shows static files, uploads, and WebSocket connection pumping.
- `Examples/HttpClientAsyncGet` shows the public asynchronous client lifecycle.
- [SCExample](@ref page_examples) contains the GUI-integrated `WebServerExample`.

The implementation is still marked draft. Before adopting it for an exposed service, evaluate the supported protocol
surface against your threat model and put a production proxy in front where appropriate. The bounded
`HttpStressTest` exercises repeated keep-alive requests, chunked request bursts, and WebSocket fan-out; it is a regression
signal, not a claim of protocol conformance or internet hardening.

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
| Library     | 1861		| 7587		| 9448	|
| Single File | 2641		| 8007		| 10648	|
| Standalone  | 10541		| 23331		| 33872	|
