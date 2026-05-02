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
HttpClient is designed to stay allocation-free by relying on caller-provided buffers and queues. The core library is poll-driven and independent from `Async`, `AsyncStreams`, `Threading`, and `Time`. Response headers and transport metadata are written into user-provided buffers, while response body chunks are delivered during `poll()` through a small listener interface.

`HttpClientRequest` groups caller-owned headers, body, and transport options into one request object. Request bodies can be passed as a fixed span or streamed by providing a pull-based `HttpClientRequestBodyProvider` with an explicit size. Redirect, timeout, TLS, and protocol concerns are grouped under `HttpClientRequestOptions`.

For stream-first integration there is a separate `SC::HttpClientAsyncT<T_AsyncEventLoop, T_AsyncStreams>` adapter that translates the same core operation into `AsyncReadableStream` and `AsyncWritableStream`.

Current limitations:
- One in-flight request per `SC::HttpClientOperation`
- Multiple `HttpClientOperation` instances can share one `SC::HttpClient`
- Chunked request bodies are not fully supported on all backends
- Non-default protocol preference is not fully implemented yet
- Some TLS customizations fail fast on backends that do not support them yet
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

🟩 Usable Features:
- Expanded protocol controls
- Broader TLS customization parity

🟦 Complete Features:
- Pluggable backend selection

💡 Unplanned Features:
- HTTP/3

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 664			| 229		| 893	|
| Sources   | 2092			| 335		| 2427	|
| Sum       | 2756			| 564		| 3320	|
