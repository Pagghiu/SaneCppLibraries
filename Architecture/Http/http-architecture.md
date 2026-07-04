# Http Architecture

## Purpose

`Libraries/Http` is the Sane C++ async HTTP/1.1 library. Its purpose is to provide an incremental parser, async server, async client, file-server helper, WebSocket support, and HTTP utility helpers that run on caller-provided storage and SC async streams. Future work in this library should deepen that async HTTP/1.1 module rather than turn it into a general web framework or a native-backend HTTP(S) client.

## Architectural Shape

Treat `Http` as a stream-first protocol module. Incoming bytes are parsed incrementally by `HttpParser` and `HttpIncomingMessage`; outgoing bytes are emitted by `HttpOutgoingMessage` and role-specific wrappers. `HttpAsyncServer` owns server connection lifecycle over `HttpConnectionsPool`; `HttpAsyncClient` owns async-client transport lifecycle over caller-provided `HttpConnectionBase` storage; `HttpAsyncFileServer` is a utility layered on top of `HttpAsyncServer`.

Keep the main interface centered on explicit storage, `Result` failures, role wrappers, and async stream composition. Shared message behavior belongs in `HttpIncomingMessage` and `HttpOutgoingMessage`. Server-only, client-only, file-server, WebSocket, or transport policy belongs in the owning wrapper or owner, not in the shared message cores.

## Boundaries

`Http` owns HTTP/1.1 parsing, request/response message framing, server connection pooling, async-client connection reuse, fixed-header writing, chunked body handling, file-server HTTP policy, URL/form/header helpers, and WebSocket upgrade/frame helpers.

`Http` does not own native OS HTTP(S) transports, browser-grade cookie/auth policy, HTTP/2 or HTTP/3, hidden allocation, STL containers, public system headers, or generic application routing beyond small HTTP helpers. HTTPS transport wiring must stay behind explicit client/server transport setup policy and must not leak into message types.

## Similarities With Other Libraries

`Http` follows the project-wide SC rules: no hidden allocation, no STL/exceptions/RTTI, `Result`-based failures, caller-owned buffers, and dependency hygiene. Like `AsyncStreams`, it exposes stream interfaces and queue/backpressure semantics. Like other Sane libraries, public headers should stay system-header-free and concrete platform work should remain private.

## Differences From Other Libraries

Unlike dependency-free libraries, `Http` intentionally depends on `Async` and `AsyncStreams`, and its file-server path reaches transitive file, filesystem, socket, and threading facilities. Unlike `HttpClient`, it is not a native-backend HTTP(S) client: it is an async HTTP/1.1 protocol stack built out of Sane async sockets and streams. Unlike small utility libraries, `Http` has hot paths where parser, connection, and file-server changes need performance awareness.

## Inspirations

The evidenced inspirations are the SC async stream model, caller-owned fixed storage, and the practical `AsyncWebServer` use case. The library should keep taking shape around stream composition, bounded storage, and serving real files/bodies without collecting whole messages in memory.

## Anti-Inspirations

Do not model `Http` after heap-backed web frameworks, full-message AST parsers, browser networking stacks, or automatic "do everything" HTTP clients. Inference: the codebase is also deliberately steering away from passive request descriptors for the async client, test-only allocating helpers in production code, and silent partial support for unsupported protocol features.

## Architectural Choices

- Use caller-provided connection, header, stream, queue, and buffer storage for async client/server paths.
- Keep parser and body framing incremental; do not require whole-message materialization.
- Preserve incoming/outgoing message symmetry across server and async-client wrappers.
- Keep transport concerns in `HttpAsyncClient` and related transport views, outside message types.
- Keep file serving layered on `HttpAsyncServer` with explicit options, stream storage, and upload policy.
- Reject unsupported protocol features explicitly through stable `Result` diagnostics where practical.

## Explicitly Excluded Targets

`Http` is not trying to be a production internet-facing web server yet, a complete HTTP standard implementation, an HTTP/2 or HTTP/3 stack, a replacement for `HttpClient`, a browser-compatible stateful client, or a hidden-allocation application framework.

## Sources

- [Http documentation](../../Documentation/Libraries/Http.md)
- [Http library notes](../../Libraries/Http/AGENTS.md)
- [Http test notes](../../Tests/Libraries/Http/AGENTS.md)
- [Http connection and message types](../../Libraries/Http/HttpConnection.h)
- [Http async client](../../Libraries/Http/HttpAsyncClient.h)
- [Http async server](../../Libraries/Http/HttpAsyncServer.h)
- [Http async file server](../../Libraries/Http/HttpAsyncFileServer.h)
- [Http parser](../../Libraries/Http/HttpParser.h)
- [HTTP-0001 - Keep HTTP on caller-provided async connection storage](http-0001-keep-http-on-caller-provided-async-connection-storage.md)
- [HTTP-0002 - Share incoming/outgoing message cores across server and async client](http-0002-share-incoming-outgoing-message-cores-across-server-and-async-client.md)
- [HTTP-0003 - Keep HTTP parser and body framing incremental, not AST-based](http-0003-keep-http-parser-and-body-framing-incremental-not-ast-based.md)
- [HTTP-0004 - Keep transport concerns in HttpAsyncClient, outside message types](http-0004-keep-transport-concerns-in-httpasyncclient-outside-message-types.md)
- [SC-0001 - Library Code Must Not Hide Dynamic Allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0003 - Keep Libraries Independently Consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [SC-0009 - Isolate Platform-Specific Implementations Behind Internal Code](../Global/sc-0009-isolate-platform-specific-implementations-behind-internal-code.md)
- [January 2026 local blog source](../../../SC-website/pagghiu.github.io-source/content/blog/2026-01-31-SaneCppLibrariesUpdate.md)
- [March 2026 local blog source](../../../SC-website/pagghiu.github.io-source/content/blog/2026-03-31-SaneCppLibrariesUpdate.md)
- [June 2026 local blog source](../../../SC-website/pagghiu.github.io-source/content/blog/2026-06-30-SaneCppLibrariesUpdate.md)

## Decision Log

- [HTTP-0001 - Keep HTTP on caller-provided async connection storage](http-0001-keep-http-on-caller-provided-async-connection-storage.md)
- [HTTP-0002 - Share incoming/outgoing message cores across server and async client](http-0002-share-incoming-outgoing-message-cores-across-server-and-async-client.md)
- [HTTP-0003 - Keep HTTP parser and body framing incremental, not AST-based](http-0003-keep-http-parser-and-body-framing-incremental-not-ast-based.md)
- [HTTP-0004 - Keep transport concerns in HttpAsyncClient, outside message types](http-0004-keep-transport-concerns-in-httpasyncclient-outside-message-types.md)
