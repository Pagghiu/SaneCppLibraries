@page library_http Http

@brief 🟥 HTTP parser, server and client

[TOC]

[SaneCppHttp.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppHttp.h) is a library implementing a hand-written HTTP/1.1 parser, server and client.

# Dependencies
- Dependencies: [Async](@ref library_async), [AsyncStreams](@ref library_async_streams)
- All dependencies: [Async](@ref library_async), [AsyncStreams](@ref library_async_streams), [File](@ref library_file), [FileSystem](@ref library_file_system), [Foundation](@ref library_foundation), [Socket](@ref library_socket), [Threading](@ref library_threading)

![Dependency Graph](Http.svg)


# Features
- HTTP 1.1 Parser
- HTTP 1.1 Server
- HTTP 1.1 Client

# Status
🟥 Draft  
In current state the library is able to host simple static website but it cannot be used for any internet facing application.  
Additionally its API will be changing heavily as it's undergoing major re-design to make it fully allocation free and extend it to support more of the HTTP 1.1 standard.

# Description
The HTTP parser is an incremental parser, that will emit events as soon as a valid element has been successfully parsed.
This allows handling incomplete responses without needing holding it entirely in memory.

The HTTP server is for now just a basic implementations and it's missing many important features.
It's however capable of serving files through http while staying inside a few user provided fixed buffers, without allocating any kind of dynamic memory.

The HTTP client follows the same fixed-memory approach used by the server side:
- caller-provided connection storage through `SC::HttpAsyncClientConnection`
- request headers built inside fixed header memory
- fixed-span or streamed request bodies with explicit `Content-Length`
- incremental response-header parsing into fixed buffers
- streamed response bodies exposed through `SC::AsyncReadableStream`
- optional sequential keep-alive reuse for requests targeting the same origin

The expected client lifecycle is stream-first:
1. Create `SC::HttpAsyncClientConnection<...>` storage and initialize `SC::HttpAsyncClient`
2. Call `start(loop, method, url)` and configure the active `SC::HttpAsyncClientRequest` inside `onPrepareRequest`, or use `get` / `put` / `post`
3. Send headers through `HttpAsyncClientRequest::sendHeaders()` and write any manual request body through `HttpAsyncClientRequest::getWritableStream()`
4. Handle `onResponse` once headers are parsed and attach to `HttpAsyncClientResponse::getReadableStream()`
5. Consume the response body incrementally and use the response readable stream `eventEnd` as the completion signal

Current client limitations:
- `http` only, no `https`
- one in-flight request at a time
- no HTTP pipelining
- no redirect policy helper

# Error Categories
HTTP APIs report failures through `SC::Result` and keep the error text stable enough to diagnose the failing layer.
The library does not use exceptions or a large error hierarchy; instead, messages are grouped by prefix and by the
operation that produced them.

Typical categories:
- Protocol errors: malformed HTTP syntax, invalid URL/request-target data, bad WebSocket frames, invalid masking,
  invalid chunk framing, unsupported transfer encodings, or unexpected response bodies.
- Unsupported feature errors: explicit limits such as unsupported content encodings, WebSocket extensions, pipelined
  body data, non-empty chunk trailers, or TLS when using this library.
- Storage errors: fixed header memory, caller-provided output buffers, stream queues, or `AsyncBuffersPool` capacity are
  too small for the requested operation.
- Stream/transport errors: socket disconnects, readable/writable stream failures, file stream failures, or backpressure
  that could not be queued.
- Lifecycle errors: calling request/response methods in the wrong order, starting a second client request while one is
  active, using an uninitialized server/client, or writing after a stream has ended.

Common prefixes point to the owner of the invariant:
- `HttpIncomingMessage`: request/response body framing and chunked decoding.
- `HttpOutgoingMessage` / `HttpResponse` / `HttpAsyncClientRequest`: outgoing header/body ordering and fixed header
  storage.
- `HttpAsyncServer` / `HttpAsyncClient`: async connection lifecycle and transport integration.
- `HttpAsyncFileServer`: safe path extraction, upload policy, file/range/validator formatting.
- `HttpURLParser` / `HttpRequestTargetView`: URL and origin-form request-target parsing.
- `HttpWebSocketHandshake`, `HttpWebSocketFrameReader`, `HttpWebSocketFrameWriter`, `HttpWebSocketEndpoint`,
  `HttpWebSocketConnectionPump`, and `HttpWebSocketSmallHub`: WebSocket handshake, frame protocol, control-frame
  lifecycle, stream pumping, and hub capacity/backpressure.

When adding new HTTP errors, prefer messages that name the component first, then the violated invariant, for example
`HttpWebSocketFrameReader control frame payload too large`. This keeps errors readable without adding allocations or a
new dependency.

# Videos

This is the list of videos that have been recorded showing some of the internal thoughts that have been going into this library:

- [Ep.27 - C++ Async Http Web Server](https://www.youtube.com/watch?v=yg438A9Db50)

# Blog

Some relevant blog posts are:

- [August 2024 Update](https://pagghiu.github.io/site/blog/2024-08-30-SaneCppLibrariesUpdate.html)
- [September 2025 Update](https://pagghiu.github.io/site/blog/2025-09-30-SaneCppLibrariesUpdate.html)
- [November 2025 Update](https://pagghiu.github.io/site/blog/2025-11-30-SaneCppLibrariesUpdate.html)
- [December 2025 Update](https://pagghiu.github.io/site/blog/2025-12-31-SaneCppLibrariesUpdate.html)
- [January 2026 Update](https://pagghiu.github.io/site/blog/2026-01-31-SaneCppLibrariesUpdate.html)
- [February 2026 Update](https://pagghiu.github.io/site/blog/2026-02-28-SaneCppLibrariesUpdate.html)
- [March 2026 Update](https://pagghiu.github.io/site/blog/2026-03-31-SaneCppLibrariesUpdate.html)
- [May 2026 Update](https://pagghiu.github.io/site/blog/2026-05-31-SaneCppLibrariesUpdate.html)

## HttpAsyncServer
@copydoc SC::HttpAsyncServer

## HttpAsyncFileServer
@copydoc SC::HttpAsyncFileServer

## HttpAsyncClient
`SC::HttpAsyncClient` supports both convenience helpers for fixed in-memory request bodies and the lower-level
`start()` flow for streamed or manually written request bodies. The API reference below includes small examples for
both styles of usage.

@copydoc SC::HttpAsyncClient

# Validation
For HTTP changes, start with the focused test, then run the broader stress and portability checks before merging.

Local macOS / Linux:

```sh
./SC.sh build compile SCTest Debug
./SC.sh build run SCTest Debug -- --test "HttpStressTest"
./SC.sh build run SCTest Debug -- --test "HttpAsyncFileServerTest"
./SC.sh build compile SCTest Release
./SC.sh build compile "SCSingleFileLibs:" Release
./SC.sh build documentation
```

Linux VM:

```sh
./SC.sh build compile SCTest Debug
./SC.sh build run SCTest Debug -- --test "HttpStressTest" --port-offset 200
./SC.sh build run SCTest Debug -- --test "HttpAsyncFileServerTest" --port-offset 200
./SC.sh build compile SCTest Release
./SC.sh build compile "SCSingleFileLibs:" Release
```

Windows VM:

```bat
SC.bat build compile SCTest Debug
SC.bat build run SCTest Debug -- --test "HttpStressTest" --port-offset 400
SC.bat build run SCTest Debug -- --test "HttpAsyncFileServerTest" --port-offset 400
SC.bat build compile SCTest Release
```

`HttpStressTest` is intentionally bounded and fast. It is a smoke signal for repeated keep-alive requests, chunked
request burst decoding, and WebSocket fan-out writes through caller-owned buffer pools. `HttpAsyncFileServerTest` covers
the heavier HTTP paths, including file requests, range and conditional requests, large PUT uploads, and multipart
uploads.

# Error Taxonomy
HTTP APIs return `Result` for protocol, storage, stream, and lifecycle failures. Prefer messages that make the failure
class obvious:

- Protocol errors: syntactically valid HTTP/WebSocket input that violates the protocol state machine, for example
  unexpected continuations, invalid control-frame fragmentation, or invalid request target usage.
- Malformed input: invalid bytes or incomplete syntax, for example bad request lines, invalid header fields, malformed
  multipart boundaries, or invalid extended WebSocket lengths.
- Unsupported feature: valid input intentionally not implemented by this library layer, for example HTTP trailers in
  request chunked decoding.
- Storage too small: caller-provided fixed storage cannot hold output, headers, copied response bodies, parsed fields, or
  queued async buffers. These errors should never silently truncate.
- Stream or transport failure: async read/write, file, socket, or buffer-pool operations failed below the HTTP parser.
- Lifecycle misuse: APIs were called in the wrong order or after ownership changed, for example writing after upgrade or
  attaching a WebSocket pump twice without detach.

# API Friction Audit
Recent example and test call sites point to these remaining improvement seams:

- Response bodies: use `HttpResponse::sendBody` / `sendJson` when the body span stays alive until the async write
  finishes, and `HttpConnection::sendBodyCopy` / `sendTextCopy` / `sendJsonCopy` when stack-owned response text should
  be copied into the connection's fixed async buffer pool before returning.
- Method routing: `HttpRouter::formatAllowHeader` can feed `HttpResponse::sendMethodNotAllowed` directly for the common
  405 + `Allow` + empty-body response path.
- Redirects: `HttpResponse::sendRedirect` emits the 3xx status, `Location`, and empty-body framing as one checked
  operation.
- Header writing: `HttpWriteBasicAuthorizationCredentials` writes Basic authorization from raw username/password into
  caller-provided storage, avoiding caller-side temporary allocation or ad-hoc base64 helpers.
- Multipart writing: `HttpMultipartWriter` rejects unsafe boundaries, field names, filenames, and content-type values
  before they can be written into request headers.
- Streaming echo responses: `Examples/ApiServer/ApiServer.cpp` now uses `AsyncPipeline` to mirror the request body into
  the response body without collecting it in memory or hand-rolling pause/drain listeners.
- WebSocket examples: `Examples/AsyncWebServer/AsyncWebServer.cpp` and the collaborative canvas now use
  `HttpWebSocketConnectionPump` for listener lifecycle and automatic control-frame flushing. The canvas keeps its
  caller-owned message assembler, source-client suppression, and best-effort dropped-broadcast accounting as application
  policy.
- File uploads: `HttpAsyncFileServerTest` covers PUT and multipart uploads well, and `Examples/AsyncWebServer` now
  exposes upload enable/disable and max-size policy from the command line. `HttpAsyncFileServerOptions` also supports
  SPA fallback, validators, ranges, and a zero-allocation MIME lookup hook.

# Examples

- [SCExample](@ref page_examples) features the `WebServerExample` sample showing how to use SC::HttpAsyncFileServer and SC::HttpAsyncServer
- Unit tests show how to use `SC::HttpAsyncClient`, `SC::HttpAsyncFileServer` and `SC::HttpAsyncServer`

# Roadmap

🟨 MVP
- HTTP 1.1 Chunked Encoding

🟩 Usable Features:
- Connection Upgrade
- Multipart streamed encoding

🟦 Complete Features:
- HTTPS
- Support all HTTP verbs / methods

💡 Unplanned Features:
- Http 2.0 
- Http 3.0

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 1517			| 871		| 2388	|
| Sources   | 7781			| 1050		| 8831	|
| Sum       | 9298			| 1921		| 11219	|
