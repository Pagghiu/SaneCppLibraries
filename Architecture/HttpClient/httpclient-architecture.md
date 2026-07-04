# HttpClient Architecture

## Purpose

`Libraries/HttpClient` is the standalone Sane C++ HTTP(S) client for native operating-system backends. Its purpose is to give callers a streaming-first client over `NSURLSession`, WinHTTP, and libcurl while preserving SC memory ownership rules and avoiding dependencies on `Http`, `Async`, or `AsyncStreams` in the core library.

## Architectural Shape

Treat `HttpClient` as a poll-driven native transport module. `HttpClient` owns backend/session initialization, `HttpClientOperation` owns one in-flight request/response exchange, `HttpClientRequest` describes caller-owned request views and transport policy, and `HttpClientResponse` exposes response metadata in caller-provided buffers. Optional adapters layer on top: `HttpClientAsyncT` maps the operation to AsyncStreams, `HttpClientSession` prepares/captures stateful metadata, and `HttpClientOperationScheduler` coordinates many operations without owning them.

The core interface should stay small enough to be driven directly, but deep enough that native backend differences, event queues, response buffering, request body streaming, cancellation, and diagnostics are hidden behind the operation interface.

## Boundaries

`HttpClient` owns native-backend HTTP(S) request execution, request validation, response metadata, response body delivery during `poll()`, capability reporting, per-request transport policy, optional async-stream adaptation, optional session metadata, and optional operation scheduling.

`HttpClient` does not own the `Http` parser/server stack, SC event loops, AsyncStreams as a core dependency, hidden cookie stores, transparent decompression, browser policy, automatic credential storage, or silent policy downgrades. Backend APIs may allocate internally, but `HttpClient` must not introduce heap-owned durable library state.

## Similarities With Other Libraries

`HttpClient` follows SC conventions: caller-owned storage, explicit `Result` failures, Common primitives, no STL/exceptions/RTTI in library code, no public system headers, and independently consumable single-file outputs. Like other SC libraries with optional integration layers, it keeps adapters separate from the core module.

## Differences From Other Libraries

Unlike `Http`, this library is dependency-free at the Sane-library level and delegates protocol transport to native OS facilities. Unlike `Async` or `AsyncStreams`, its core interface is poll-driven rather than event-loop-native. Unlike a browser client, its session helper is an explicit caller-owned layer, and unlike many HTTP clients it does not transparently decode content or silently downgrade unsupported backend policy.

## Inspirations

The evidenced inspirations are native OS HTTP facilities, SC caller-owned memory style, and the AsyncStreams adapter pattern for callers that opt into stream composition. The March 2026 blog source explicitly describes the separate streaming-first client using `NSURLSession`, WinHTTP, and libcurl.

## Anti-Inspirations

Do not model `HttpClient` after `Http` internals, browser networking stacks, heap-owning convenience clients, global session stores, or transparent content-coding layers. Inference: the design also rejects "best effort" backend policy, where a request asks for one TLS/proxy/protocol behavior and the library silently runs another.

## Architectural Choices

- Keep `HttpClient` separate from `Http`; duplicate small HTTP concepts when needed to preserve independence.
- Keep the core operation poll-driven and dependency-free; put AsyncStreams support in an adapter.
- Keep operation memory caller-owned and view-based, including response buffers, headers, metadata, events, and backend scratch.
- Expose backend capabilities and fail fast when request policy cannot be honored.
- Keep session and scheduler helpers optional, caller-owned, and above the core operation.
- Keep content coding as metadata and explicit caller-owned transforms, not transparent core transport behavior.

## Explicitly Excluded Targets

`HttpClient` is not an HTTP server, not a wrapper around `Libraries/Http`, not an event-loop framework, not a browser-compatible stateful client, not a transparent decompression engine, not an HTTP/3 client, and not a guarantee that every backend supports every request option.

## Sources

- [HttpClient documentation](../../Documentation/Libraries/HttpClient.md)
- [HttpClient public core](../../Libraries/HttpClient/HttpClient.h)
- [HttpClient async adapter](../../Libraries/HttpClient/HttpClientAsync.h)
- [HttpClient session layer](../../Libraries/HttpClient/HttpClientSession.h)
- [HttpClient scheduler](../../Libraries/HttpClient/HttpClientScheduler.h)
- [HttpClient tests](../../Tests/Libraries/HttpClient/HttpClientTest.cpp)
- [HTTPCLIENT-0001 - Keep HttpClient separate from Http](httpclient-0001-keep-httpclient-separate-from-http.md)
- [HTTPCLIENT-0002 - Make the core API poll-driven, with AsyncStreams as an adapter](httpclient-0002-make-the-core-api-poll-driven-with-asyncstreams-as-an-adapter.md)
- [HTTPCLIENT-0003 - Keep operation memory caller-owned and view-based](httpclient-0003-keep-operation-memory-caller-owned-and-view-based.md)
- [HTTPCLIENT-0004 - Report backend capabilities and fail fast on unsupported request policy](httpclient-0004-report-backend-capabilities-and-fail-fast-on-unsupported-request-policy.md)
- [HTTPCLIENT-0005 - Keep session and scheduler as optional caller-owned layers](httpclient-0005-keep-session-and-scheduler-as-optional-caller-owned-layers.md)
- [HTTPCLIENT-0006 - Keep content coding out of the core transport operation](httpclient-0006-keep-content-coding-out-of-the-core-transport-operation.md)
- [SC-0001 - Library Code Must Not Hide Dynamic Allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0003 - Keep Libraries Independently Consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [SC-0008 - Prefer Native OS APIs Over Third-Party Dependencies](../Global/sc-0008-prefer-native-os-apis-over-third-party-dependencies.md)
- [SC-0011 - Make Allocation-Capable Facilities Explicit and Optional](../Global/sc-0011-make-allocation-capable-facilities-explicit-and-optional.md)
- [March 2026 local blog source](../../../SC-website/pagghiu.github.io-source/content/blog/2026-03-31-SaneCppLibrariesUpdate.md)
- [June 2026 local blog source](../../../SC-website/pagghiu.github.io-source/content/blog/2026-06-30-SaneCppLibrariesUpdate.md)

## Decision Log

- [HTTPCLIENT-0001 - Keep HttpClient separate from Http](httpclient-0001-keep-httpclient-separate-from-http.md)
- [HTTPCLIENT-0002 - Make the core API poll-driven, with AsyncStreams as an adapter](httpclient-0002-make-the-core-api-poll-driven-with-asyncstreams-as-an-adapter.md)
- [HTTPCLIENT-0003 - Keep operation memory caller-owned and view-based](httpclient-0003-keep-operation-memory-caller-owned-and-view-based.md)
- [HTTPCLIENT-0004 - Report backend capabilities and fail fast on unsupported request policy](httpclient-0004-report-backend-capabilities-and-fail-fast-on-unsupported-request-policy.md)
- [HTTPCLIENT-0005 - Keep session and scheduler as optional caller-owned layers](httpclient-0005-keep-session-and-scheduler-as-optional-caller-owned-layers.md)
- [HTTPCLIENT-0006 - Keep content coding out of the core transport operation](httpclient-0006-keep-content-coding-out-of-the-core-transport-operation.md)
