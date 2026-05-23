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
- Inline, sized-stream, or chunked-stream request bodies
- Optional caller-owned session helpers for cookies, auth cache entries, and retry bookkeeping
- Optional caller-owned operation scheduler for many poll-driven operations
- Async streaming integration available through `SC::HttpClientAsyncT`
- Blocking helper for simple synchronous workflows

# Status
🟥 Draft  
The API is stabilizing and the streaming core is in place, but consider everything HIGHLY experimental.

# Description
HttpClient is designed to stay allocation-free by relying on caller-provided buffers and queues. The core library is poll-driven and independent from `Async`, `AsyncStreams`, `Threading`, and `Time`. Response headers and transport metadata are written into user-provided buffers, while response body chunks are delivered during `poll()` through a small listener interface.

`HttpClientRequest` groups caller-owned headers, body, and transport options into one request object. Request bodies are explicitly framed as fixed-size inline bytes, a fixed-size stream, or a chunked stream by setting `HttpClientRequestBody::framing`. Redirect, timeout, TLS, and protocol concerns are grouped under `HttpClientRequestOptions`.
Request method, redirect mode, header names, and header values are validated before any backend-specific request setup starts.
Header names must be HTTP token names, and header values reject CR, LF, and NUL bytes.
Request methods, body framing modes, redirect modes, protocol preferences, and proxy modes expose static name helpers for allocation-free diagnostics.
`HttpClientRequest::validate()` exposes the same request-shape checks used by `HttpClientOperation::start()`.
The request object is copied by value when an operation starts, but URL, headers, body bytes, streamed-body providers, and option string views remain caller-owned and must outlive the operation.

For stream-first integration there is a separate `SC::HttpClientAsyncT<T_AsyncEventLoop, T_AsyncStreams>` adapter that translates the same core operation into `AsyncReadableStream` and `AsyncWritableStream`.

For stateful workflows there is a separate `SC::HttpClientSession` helper declared in
`Libraries/HttpClient/HttpClientSession.h`. It remains layered above the transport core: cookies,
authorization cache entries, prepared request headers, retry state, and all durable strings live in
caller-provided spans and scratch buffers. `prepareRequest()` produces a derived request that must
be started before reusing the same session header scratch for another prepared request.

For high-concurrency poll loops there is a separate `SC::HttpClientOperationScheduler` helper
declared in `Libraries/HttpClient/HttpClientScheduler.h`. It owns no operations and starts no
requests; it only installs itself as the operation notifier and tracks ready operations in
caller-provided bytes so callers can poll signaled operations instead of scanning blindly.

Current limitations:
- One in-flight request per `SC::HttpClientOperation`
- Multiple `HttpClientOperation` instances can share one `SC::HttpClient`
- `Transfer-Encoding` is controlled by `HttpClientRequestBody::framing`; callers must not provide it manually
- Chunked request trailers are not exposed
- Apple treats `Default` and `Http2Preferred` as `NSURLSession` default policy, but fails fast for forced `Http11Only` or `Http2Required`
- `Http2Required` must negotiate HTTP/2; supported backends fail the request instead of silently downgrading
- Some TLS customizations fail fast on backends that do not support them yet

Protocol policy:
- `Default` lets the backend choose
- `Http11Only` forces HTTP/1.1 where the backend exposes that control
- `Http2Preferred` enables HTTP/2 where available while allowing HTTP/1.1 fallback
- `Http2Required` rejects unsupported backends and rejects negotiated HTTP/1.1 responses

Proxy policy:
- `Default` keeps the backend/system proxy behavior
- `NoProxy` bypasses proxies where the backend exposes per-request control
- `Http` uses a caller-owned `http://` proxy URL
- `authorization` is an optional exact `Proxy-Authorization` value for explicit HTTP proxies
- `bypassList` is an optional comma-separated host list for explicit HTTP proxies
- Apple currently fails fast for non-default proxy policy because `NSURLSession` proxy dictionaries would need a separate supported API shape

Response headers:
- `HttpClientResponse::headers` remains the raw caller-owned header buffer view and source of truth
- `getProtocolName()` returns static names for negotiated protocol metadata
- `isInformationalStatus()`, `isSuccessfulStatus()`, `isRedirectStatus()`, and error helpers classify status codes
- `getNextHeader()` walks parsed header name/value views with a caller-owned iterator
- `findNextHeader()` supports repeated headers without building a map
- `hasHeader()` checks for a header name without exposing a value view
- common helpers such as `getContentLength()`, `getContentType()`, `getContentEncoding()`,
  `getTransferEncoding()`, `getLocation()`, `getWwwAuthenticate()`, and `getProxyAuthenticate()` stay layered over the raw buffer
- `getNextTransferCoding()` iterates comma-separated `Transfer-Encoding` tokens and classifies common codings without allocation
- `hasContentCoding()` and `hasTransferCoding()` scan classified coding tokens without caching parsed state

Capability reporting:
- `HttpClient::getCapabilities()` returns the compiled backend and supported policy groups
- `HttpClientCapabilities::supports(feature)` exposes the same fields through a stable feature enum for future transport expansion
- `HttpClientCapabilities::supportsAll()` / `requireFeatures()` let callers fail fast when they need a backend feature set
- `HttpClientCapabilities::supportsRequestOptions()` / `requireRequestOptions()` preflight request policy without starting an operation
- `HttpClientCapabilities::hasBackend()` / `requireBackend()` let callers fail fast when they intentionally target one compiled backend
- `HttpClient::init(requiredBackend/features)` overloads apply the same checks before backend initialization
- `HttpClientOperation::start()` preflights protocol, TLS, and proxy policy against those capabilities before backend request setup
- `HttpClientCapabilities::getBackendName()` returns a static backend name for logs, diagnostics, and tests
- `HttpClientCapabilities::getFeatureName()` returns static feature names for capability diagnostics
- capability fields describe explicit API support, not whether a remote server will negotiate a feature
- `contentCodingPolicy` is currently false because decompression belongs in a future caller-owned streaming layer

Content-coding policy:
- The core client does not request or decode compressed content on behalf of the caller
- Callers can still send `Accept-Encoding` explicitly as a normal request header
- `Content-Encoding` is exposed as response metadata through `getContentEncoding()`
- `getNextContentCoding()` iterates comma-separated `Content-Encoding` tokens and classifies common codings without allocation
- `hasContentCoding()` checks for a classified `Content-Encoding` token without building a token list
- `HttpClientContentCoding::writeAcceptEncoding()` builds `Accept-Encoding` values into caller-owned buffers
- Decompression should be built as a caller-owned streaming transform above the raw response body, not as hidden state inside `HttpClientOperation`
- `AsyncStreams` already provides zlib/gzip/deflate transform primitives that callers can compose explicitly when they opt into that dependency

# Recommended Patterns

Blocking:
```cpp
SC::HttpClientRequest request;
request.url = "https://example.com"_a8;

SC::HttpClientResponse response;
char body[4096];
size_t bodyLength = 0;
SC_TRY(SC::HttpClient::executeBlocking(request, response, {body, sizeof(body)}, bodyLength, memory));
```

Poll-driven:
```cpp
SC::HttpClient client;
SC_TRY(client.init());

SC::HttpClientOperation operation;
SC_TRY(operation.init(client, memory));
SC_TRY(operation.start(request, response, &listener));
while (operation.isRequestInFlight())
{
    SC_TRY(operation.poll(16));
}
```

Session layer:
```cpp
SC::HttpClientSession session;
SC_TRY(session.init(sessionMemory));
SC_TRY(session.prepareRequest(sourceRequest, preparedRequest));
SC_TRY(operation.start(preparedRequest, response));
```

Scheduler:
```cpp
SC::HttpClientOperationScheduler scheduler;
SC_TRY(scheduler.init(schedulerMemory));
while (scheduler.hasRequestsInFlight())
{
    size_t numPolled = 0;
    SC_TRY(scheduler.pollReady(numPolled, 16));
}
```

# Allocation-Free Audit

The stabilization audit treats `Libraries/HttpClient` as a caller-owned layer. The core operation,
session helper, and scheduler do not allocate durable state: request/response views point into
caller-owned storage, session cookies/auth entries copy into caller-provided scratch, and scheduler
readiness uses caller-provided bytes. Native backend APIs may allocate internally, but the library
does not introduce heap containers, owned strings, hidden cookie/auth stores, or decompression state.
Operation-memory validation rejects missing event queues, response buffers, response-header storage,
response-metadata storage, empty per-buffer response storage, and undersized sliced response memory.

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

## Session Layer
`SC::HttpClientSession` is optional and does not own allocations. It can capture `Set-Cookie`
headers into caller-provided cookie slots, add cached `Authorization` values by exact origin, and
track retry attempts for one logical request. It also provides `makeBasicAuthorization()` to build
`Authorization` or `Proxy-Authorization` values into caller-owned buffers. Basic authentication
challenge helpers can inspect `WWW-Authenticate` and `Proxy-Authenticate` response headers and
prepare a retry header without storing credentials. Auth challenge target and scheme names are
available as static strings for diagnostics. The caller still drives `HttpClientOperation` directly;
the session layer only prepares request metadata and records response metadata.
Use `findCookie()`, `hasCookie()`, `findAuthorization()`, `hasAuthorization()`, `getNumCookies()`,
and `getNumAuthorizations()` to inspect caller-owned session state without allocating.
`clearCookies()` and `clearAuthorizations()` clear their slots independently; use `clear()` when
you also want to reclaim session scratch space.
Retry helpers expose idempotent-method checks, retryable-status checks, and remaining-attempt state
without hiding the transport `Result` that caused the retry decision.

## Operation Scheduler
`SC::HttpClientOperationScheduler` is optional and caller-owned. Register initialized
`HttpClientOperation` pointers plus one ready byte per operation, start requests normally, then call
`pollReady()` from the application loop. The scheduler uses the existing notifier hook and never
changes the single-operation request/response contract.
Use `getNumOperations()`, `isOperationRegistered()`, and `getNumRequestsInFlight()` for allocation-free
orchestration diagnostics.

# Blog

Some relevant blog posts are:

- [March 2026 Update](https://pagghiu.github.io/site/blog/2026-03-31-SaneCppLibrariesUpdate.html)

# Examples

- Unit tests in `Tests/Libraries/HttpClient` show blocking and async usage patterns
- AsyncStreams examples show how to integrate streaming pipelines with `AsyncReadableStream`

# Roadmap

🟨 MVP
- No remaining MVP item in the allocation-free core; non-Basic authentication schemes remain application- or backend-specific policy.

🟩 Usable Features:
- Higher-level content-coding transform composition helpers
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
