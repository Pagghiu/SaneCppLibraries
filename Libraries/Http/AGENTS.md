# HTTP Library Notes

This directory is performance-sensitive and allocation-free.

## Architecture Overview

- **`HttpConnectionBase`**: The foundational struct holding async readable/writable socket streams, buffer pool pointers, pipeline, and socket descriptor. Shared by both client and server endpoints.
- **`HttpIncomingMessage`**: Shared core for parsed incoming headers/body state. It owns `HttpParsedHeaders`, the readable stream binding, body-bytes-remaining tracking, header lookup helpers, and multipart helpers.
- **`HttpOutgoingMessage`**: Shared core for outgoing headers/body state. It owns the writable stream binding, fixed-buffer header writer, keep-alive policy, and common header emission helpers.
- **`HttpRequest` / `HttpAsyncClientResponse`**: Thin wrappers over `HttpIncomingMessage`. Server requests keep request-specific URL access; client responses expose the same incoming-message shape.
- **`HttpResponse` / `HttpAsyncClientRequest`**: Thin wrappers over `HttpOutgoingMessage`. Server responses keep status-line helpers; client requests expose the same writable-message shape.
- **`HttpConnection`**: Inherits from `HttpConnectionBase` and hosts the server-side `HttpRequest` and `HttpResponse` wrappers. Used exclusively for incoming connections on the server side (via `HttpConnectionsPool`).
- **`HttpStaticConnection<...>`**: A helper template that allocates compile-time fixed buffers (read queue, write queue, stream storage, header storage). It inherits from a given `BaseClass` (`HttpConnection` or `HttpConnectionBase`) and sets up memory spans without dynamic allocation.
- **`HttpAsyncClientConnection<...>`**: Plumbs `HttpStaticConnection` for the client side.
- **`HttpAsyncConnection<...>`**: Plumbs `HttpStaticConnection` for the server side.
- **`HttpAsyncServer`**: Serves asynchronous HTTP requests. Uses `HttpConnection` objects, allocated and grouped inside an `HttpConnectionsPool`.
- **`HttpAsyncClient`**: A zero-allocation async HTTP client. It is initialized with a reference to an `HttpConnectionBase`, owns the active `HttpAsyncClientRequest` / `HttpAsyncClientResponse` pair, and exposes `onRequest(HttpAsyncClientRequest&)` for streamed or manual request bodies.
- **`HttpAsyncFileServer`**: A utility layered on top of `HttpAsyncServer` that handles streaming files to connected clients via `AsyncFileSend` (or pipelined streams) using `HttpAsyncFileServer::Stream`.

## Core constraints

- Keep library code allocation-free. `SC::HttpAsyncClient`, `SC::HttpAsyncServer`, `SC::HttpAsyncFileServer`, `SC::HttpParser`, and `SC::HttpConnection` must keep using caller-provided memory.
- Do not introduce STL, exceptions, RTTI, or new cross-library dependencies.
- Do not put system headers in public headers.
- Prefer the same fixed-buffer patterns already used by the server-side HTTP classes over `IGrowableBuffer`-style helpers.

## Hot paths

The following files are benchmark-sensitive:

- `HttpAsyncServer.cpp`
- `HttpAsyncFileServer.cpp`
- `HttpConnection.cpp`
- `HttpParser.cpp`

Avoid changing them unless the change is required for correctness or the user explicitly asks for the tradeoff.

## Benchmark workflow

If you touch the parser, async server, file server, response emission, or request parsing hot path, validate with the local benchmark:

```bash
./SC.sh build compile AsyncWebServer Release
./SC.sh build run AsyncWebServer Release -- --directory /Users/stefano/Developer/Projects/SC/_Build/_Documentation/docs
Support/Benchmarks/Http/.venv/bin/python Support/Benchmarks/Http/http_benchmark.py --directory "/Users/stefano/Developer/Projects/SC/_Build/_Documentation/docs" --port 8090 --concurrent 10 --mode directory
```

Run the benchmark from a second terminal while the server is running.

## Client-specific notes

- `SC::HttpAsyncClient` is public library code. Keep it independent from test-only helpers.
- The client request lifecycle is active now. Prefer `start(loop, method, url, keepAlive)` plus `onRequest(HttpAsyncClientRequest&)` over reintroducing passive request descriptors.
- Keep transport concerns in `HttpAsyncClient` rather than in message types: URL parsing, DNS/connect, connection reuse, and automatic `Host` / `User-Agent` injection belong there.
- Preserve incoming/outgoing symmetry where practical: shared behavior belongs in `HttpIncomingMessage` / `HttpOutgoingMessage`, while request-only or response-only semantics stay in thin wrappers.
- `HttpTestClient` under `Tests/Libraries/Http` is the old allocating helper kept only for tests.
- Unsupported features should be rejected explicitly rather than partially supported.

## Validation

For HTTP library changes, at minimum:

```bash
./SC.sh build compile SCTest Debug
./SC.sh build run SCTest Debug -- --test "HttpAsyncClientTest"
./SC.sh build run SCTest Debug -- --test "HttpAsyncServerTest"
./SC.sh build run SCTest Debug -- --test "HttpAsyncFileServerTest"
./SC.sh build run SCTest Debug -- --test "HttpKeepAliveTest"
python3 Support/SingleFileLibs/python/amalgamate_single_file_libs.py
./SC.sh build compile "SCSingleFileLibs:" Release
```
