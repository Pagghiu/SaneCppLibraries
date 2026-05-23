# HTTP Client Streaming

## Use This When

- Use the separate native HTTP client instead of the Sane HTTP server/parser stack.
- Stream request or response bodies with a poll-driven core.
- Adapt the client into `AsyncStreams`.

## Main Types

- `SC::HttpClient`
- `SC::HttpClientRequest`
- `SC::HttpClientResponse`
- `SC::HttpClientOperationMemory`
- `SC::HttpClientAsyncT`
- `SC::HttpClientAsyncOperationMemoryT`
- `SC::HttpClientRequestBody`
- `SC::HttpClientRequestOptions`
- `SC::HttpClientSession`
- `SC::HttpClientOperationScheduler`

## Typical Flow

1. Create caller-owned operation memory, including response headers and response metadata buffers.
2. Initialize `HttpClient`.
3. Start a request with a request object that already contains headers, body, and transport options.
4. Poll until completion or attach the async adapter and stream bodies.
5. If stateful behavior is needed, prepare retries or headers with `HttpClientSession` before starting the next operation.

## Limits To Remember

- One in-flight request per operation.
- Multiple operations can share one client session/backend owner.
- Redirects, protocol preferences, proxy policy, and TLS customization are explicit request options.
- Chunked request bodies are controlled by `HttpClientRequestBody::framing`; callers should not add `Transfer-Encoding` manually.
- TLS and backend behavior are still experimental at the time of writing.
- The core does not allocate for decompression, cookies, credentials, or retry state; use caller-owned helpers above it.
- `getNextContentCoding()` parses response codings, but callers still own any `AsyncStreams` transform chain.

## Best Sources

- `Documentation/Libraries/HttpClient.md`
- `Libraries/HttpClient/HttpClientAsync.h`
- `Tests/Libraries/HttpClient/HttpClientTest.cpp`
