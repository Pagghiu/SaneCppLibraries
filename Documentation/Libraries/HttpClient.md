@page library_http_client Http Client

@brief 🟥 Allocation-free HTTP client over the operating system's native transport

[TOC]

[SaneCppHttpClient.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppHttpClient.h)
provides HTTP requests without making an HTTP stack another dependency of the application. It wraps
`NSURLSession` on Apple platforms, WinHTTP on Windows, and libcurl on Linux behind one request and
streaming-response model.

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](HttpClient.svg)


# Is HttpClient the right layer?

Choose HttpClient when an application needs outbound HTTP or HTTPS on the three supported desktop
platforms, wants native transport integration, and can budget memory before starting a request. The
library is particularly suited to tools, agents, and embedded-style applications where an implicit
heap-owned response, cookie jar, or worker object would be undesirable.

It is not a byte-for-byte portable HTTP implementation. The operating system still owns DNS, TLS,
connection pooling, and protocol negotiation, so observable transport behavior can differ between
Apple, Windows, and Linux. `HttpClient::getCapabilities()` makes policy differences queryable, and
request startup fails when a requested protocol, TLS, or proxy policy is unsupported; it does not
quietly weaken an explicit requirement.

The API is 🟥 **Draft** and still highly experimental. In particular, applications should expect API
changes while backend behavior and the streaming adapters converge.

The neighboring [Http](@ref library_http) library solves a different problem: it implements HTTP/1.1
parsing, servers, WebSockets, and an `AsyncStreams` client over caller-selected socket transports.
Use that client when control over the HTTP/1.1 connection and stream pipeline matters. Use HttpClient
when native HTTPS, native proxy behavior, and HTTP/2 negotiation matter more than identical transport
internals. HttpClient itself has no SC library dependency; its optional Async adapter composes with
[Async](@ref library_async) and [AsyncStreams](@ref library_async_streams) in application code.

# The request is a description; the operation owns the conversation

The core has three distinct lifetimes:

1. `HttpClient` initializes the platform backend and may be shared by multiple operations.
2. `HttpClientRequest` describes one request using non-owning views. Starting an operation copies the
   description, not the URL, headers, inline body, option strings, or body provider behind those views.
3. `HttpClientOperation` carries one in-flight request and delivers response events from `poll()`.
   Its queues, response chunks, raw headers, metadata, and backend scratch all come from
   `HttpClientOperationMemory` supplied by the caller.

One operation carries at most one request at a time. Concurrency means creating multiple operations
against the same client, each with separate memory. `poll()` invokes a listener with a response head,
zero or more body spans, and completion or error. A body span is borrowed only for that callback;
copy it or consume it before returning. By contrast, response header and effective-URL views point
into operation memory and remain tied to that storage's lifetime.

This excerpt is compiled as part of `HttpClientPollSession`. It shows the real amount of storage and
coordination needed for a single poll-driven request; the fixed sizes are application policy, not
library constants.

@snippet Examples/HttpClientPollSession/HttpClientPollSession.cpp HttpClientPollRequestSnippet

The listener in that example copies body chunks into another fixed buffer. An application can instead
parse, hash, decompress, or write each chunk as it arrives. If the chosen event queue, header buffer,
metadata buffer, or response buffers are exhausted, the operation reports an error rather than
allocating or truncating silently.

# Request bodies and response metadata

An outgoing body has explicit framing:

- `FixedSize` borrows an inline byte span.
- `SizedStream` repeatedly calls a `HttpClientRequestBodyProvider` and requires the declared byte
  count to match what the provider produces.
- `ChunkedStream` pulls from the provider without a declared length.

The provider and everything referenced by the request must outlive completion or cancellation.
`Transfer-Encoding` belongs to the framing policy and must not also be supplied as a raw request
header. Request validation rejects malformed HTTP(S) URLs, invalid method/header data, CR/LF/NUL in
header values, and inconsistent body descriptions before backend setup.

`HttpClientResponse::headers` is the raw, caller-owned source of truth. Iterators and convenience
functions inspect repeated headers, content length/type/encoding, transfer codings, redirects, and
authentication challenges without building a map. The client does not add `Accept-Encoding` or
decompress a response. This is deliberate: compressed bytes can be fed into a caller-owned transform,
including an `AsyncStreams` gzip/deflate pipeline when that dependency is acceptable.

For a small synchronous request, `HttpClient::executeBlocking()` drives the same operation model and
copies into a caller-provided body span. The span is a hard limit: overflow is an error, and
`bodyLength` reports how many bytes were copied before it was detected. The helper still needs an
`HttpClientOperationMemory`; it is convenience for control flow, not hidden storage.

# Optional state and event-loop layers

The transport core deliberately does not own durable policy. Three optional helpers cover common
application shapes while preserving caller ownership:

- `HttpClientSession` prepares cookie and exact-origin `Authorization` headers, captures response
  cookies, and tracks retry attempts in caller-provided slots and scratch spans. It is a small
  transport helper, not a browser cookie store: cookie matching is intentionally limited and only
  Basic authentication challenge helpers are provided. A prepared request borrows the session's
  header workspace, so start it before preparing another request with the same workspace.
- `HttpClientOperationScheduler` installs the operation notifier and records readiness in one
  caller-provided byte per registered operation. It avoids scanning every operation, but owns no
  operation, starts no request, and does not change the one-request-per-operation rule.
- `HttpClientAsyncT<AsyncEventLoop, AsyncStreams>` translates response callbacks into an
  `AsyncReadableStream` and streamed uploads into an `AsyncWritableStream`. Its stream buffers and
  queues are additional caller-owned memory. It is an adapter over `HttpClientOperation`, not a
  second transport implementation.

Retries remain an application decision. Before retrying, consider the method, whether the body
provider can replay its data, the transport result, and any response status. The session helper can
bookkeep that policy but never hides the original `Result` or restarts an operation by itself.

# Backend policy and current limits

Defaults let the native backend choose where possible. Stronger policies should be checked through
`HttpClientCapabilities::supportsRequestOptions()` or allowed to fail during `start()`:

- `Http2Required` rejects a backend that cannot enforce HTTP/2 and rejects an HTTP/1.1 negotiation;
  `Http2Preferred` permits fallback. Apple currently cannot force HTTP/1.1 or require HTTP/2 through
  this API shape.
- Peer verification is enabled by default. A custom CA path or disabling verification fails on a
  backend that cannot honor it.
- The default proxy mode preserves system behavior. Explicit no-proxy and HTTP-proxy policies are
  backend capabilities; Apple currently rejects non-default proxy policy.
- Redirects are off by default. Following redirects is constrained by method policy and a caller-set
  maximum.
- Native backend APIs may allocate internally. The allocation-free guarantee is that HttpClient does
  not add heap containers, owned strings, cookie/auth stores, or decompression state around them.
- Chunked request trailers, non-Basic authentication policy, HTTP/3, pluggable backend selection, and
  uniform advanced TLS customization are not provided.

Capability fields describe what this build can request from its backend, not what a remote server
will negotiate. Applications with a hard deployment requirement can pass required backend/features
to `HttpClient::init()` and fail before accepting work.

# Where to look next

- `Examples/HttpClientPollSession` is the complete poll-driven example used above, including the
  session and scheduler layers.
- `Examples/HttpClientAsyncGet` shows the `AsyncStreams` adapter and its extra queues and buffers.
- `Tests/Libraries/HttpClient/HttpClientTest.cpp` exercises blocking, poll-driven, concurrent,
  scheduled, streamed-upload, Async, validation, and policy-failure paths against a local server.
- The [March 2026](https://pagghiu.github.io/site/blog/2026-03-31-SaneCppLibrariesUpdate.html),
  [May 2026](https://pagghiu.github.io/site/blog/2026-05-31-SaneCppLibrariesUpdate.html), and
  [June 2026](https://pagghiu.github.io/site/blog/2026-06-30-SaneCppLibrariesUpdate.html) updates record
  the design's recent evolution.

For API signatures and per-member contracts, see @ref group_http_client. The public headers separate
the layers explicitly: `HttpClient.h` for the core, `HttpClientSession.h` for state,
`HttpClientScheduler.h` for readiness coordination, and `HttpClientAsync.h` for stream integration.

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/HttpClient`.
Single File counts
`SaneCppHttpClient.h`.
Standalone counts `SaneCppHttpClientStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 1188		| 4754		| 5942	|
| Single File | 2018		| 4858		| 6876	|
| Standalone  | 2018		| 4858		| 6876	|
