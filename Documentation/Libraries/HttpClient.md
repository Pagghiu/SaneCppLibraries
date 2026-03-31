@page library_http_client Http Client

@brief 🟥 Streaming-first HTTP client with native OS backends

[TOC]

[SaneCppHttpClient.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppHttpClient.h) is a streaming-first HTTP client built on native OS backends.

# Dependencies
- Dependencies: [Foundation](@ref library_foundation)
- All dependencies: [Foundation](@ref library_foundation)

![Dependency Graph](HttpClient.svg)


# Features
- Native OS backends (NSURLSession on Apple, WinHTTP on Windows, libcurl on Linux)
- Poll-driven core API with an optional `HttpClientAsyncT` adapter for `AsyncStreams`
- Inline or streamed request bodies with explicit size
- Async streaming integration available through `SC::HttpClientAsyncT`
- Blocking helper for simple synchronous workflows

# Status
🟥 Draft  
The API is stabilizing and the streaming core is in place, but consider everything HIGHLY experimental.

# Description
HttpClient is designed to stay allocation-free by relying on caller-provided buffers and queues. The core library is poll-driven and independent from `Async`, `AsyncStreams`, `Threading`, and `Time`. Response headers are written into a user-provided buffer, while response body chunks are delivered during `poll()` through a small listener interface.

Request bodies can be passed as a fixed span or streamed by implementing a pull-based provider with an explicit `Content-Length`. For stream-first integration there is a separate `SC::HttpClientAsyncT<T_AsyncEventLoop, T_AsyncStreams>` adapter that translates the same core operation into `AsyncReadableStream` and `AsyncWritableStream`.

Current limitations:
- One in-flight request per `SC::HttpClient`
- Chunked request bodies are not fully supported on all backends
- Redirects disabled by default
- HTTP/2 only when provided by the OS backend

# Details

@copydetails group_http_client

## HttpClient
@copydoc SC::HttpClient

## HttpClientRequest
@copydoc SC::HttpClientRequest

## HttpClientResponse
@copydoc SC::HttpClientResponse

## HttpClientRequestBodyProvider
@copydoc SC::HttpClientRequestBodyProvider

## HttpClientOperationListener
@copydoc SC::HttpClientOperationListener

## HttpClientOperationNotifier
@copydoc SC::HttpClientOperationNotifier

## HttpClientResponseBuffer
@copydoc SC::HttpClientResponseBuffer

## HttpClientOperationEvent
@copydoc SC::HttpClientOperationEvent

## HttpClientOperationMemory
@copydoc SC::HttpClientOperationMemory

## HttpClientOperation
@copydoc SC::HttpClientOperation

## Async Adapter
`SC::HttpClientAsyncT` and `SC::HttpClientAsyncOperationMemoryT` are declared in
`Libraries/HttpClient/HttpClientAsync.h`.

They provide the optional `Async` / `AsyncStreams` integration layer on top of the poll-driven
`HttpClientOperation` core, reusing the same request, response, and caller-owned operation memory model.

With the standard Async Streams library, instantiate the adapter as
`SC::HttpClientAsyncT<SC::AsyncEventLoop, SC::AsyncStreams>` and the adapter memory as
`SC::HttpClientAsyncOperationMemoryT<SC::AsyncStreams>`.

# Blog

Some relevant blog posts are:

- [March 2026 Update](https://pagghiu.github.io/site/blog/2026-03-31-SaneCppLibrariesUpdate.html)

# Examples

- Unit tests in `Tests/Libraries/HttpClient` show blocking and async usage patterns
- AsyncStreams examples show how to integrate streaming pipelines with `AsyncReadableStream`

# Roadmap

🟨 MVP
- Chunked transfer encoding for request bodies
- Multi-inflight support (one client managing multiple concurrent requests)

🟩 Usable Features:
- Expanded redirect policy controls
- Optional TLS configuration hooks

🟦 Complete Features:
- Pluggable backend selection

💡 Unplanned Features:
- HTTP/3

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 664			| 229		| 893	|
| Sources   | 2090			| 335		| 2425	|
| Sum       | 2754			| 564		| 3318	|
